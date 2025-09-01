#include "ZeroCopyCapture.h"
#include <iostream>
#include <sys/mman.h>
#include <chrono>

using namespace libcamera;
using namespace std::chrono;
using namespace std::literals::chrono_literals;

ZeroCopyCapture::ZeroCopyCapture(const VideoConfig& config) 
    : stream_(nullptr), stopping_(false), video_config_(config) {
}

ZeroCopyCapture::~ZeroCopyCapture() {
    cleanup();
}

bool ZeroCopyCapture::initialize() {
    std::cout << "[INFO] Initializing ZeroCopyCapture..." << std::endl;
    
    camera_manager_ = std::make_unique<CameraManager>();
    if (camera_manager_->start()) {
        std::cerr << "[ERROR] Failed to start camera manager" << std::endl;
        return false;
    }
    
    if (camera_manager_->cameras().empty()) {
        std::cerr << "[ERROR] No cameras found" << std::endl;
        return false;
    }
    
    std::string cameraId = camera_manager_->cameras()[0]->id();
    camera_ = camera_manager_->get(cameraId);
    std::cout << "[INFO] Using camera: " << cameraId << std::endl;

    if (camera_->acquire()) {
        std::cerr << "[ERROR] Failed to acquire camera" << std::endl;
        return false;
    }

    config_ = camera_->generateConfiguration({StreamRole::Viewfinder});
    StreamConfiguration& streamConfig = config_->at(0);
    streamConfig.size = Size(video_config_.width, video_config_.height);
    streamConfig.pixelFormat = getPixelFormat(video_config_.pixel_format);
    streamConfig.bufferCount = video_config_.buffer_count;
    config_->validate();
    
    if (camera_->configure(config_.get()) < 0) {
        std::cerr << "[ERROR] Failed to configure camera" << std::endl;
        return false;
    }
    stream_ = streamConfig.stream();

    std::cout << "[INFO] Stream configured: " << streamConfig.size.width << "x" << streamConfig.size.height
              << " " << streamConfig.pixelFormat.toString() << " with " << streamConfig.bufferCount << " buffers." << std::endl;

    return setupBuffers();
}

bool ZeroCopyCapture::setupBuffers() {
    std::cout << "[INFO] Setting up DMA buffers..." << std::endl;
    allocator_ = std::make_shared<FrameBufferAllocator>(camera_);
    if (allocator_->allocate(stream_) < 0) {
        std::cerr << "[ERROR] Failed to allocate buffers" << std::endl;
        return false;
    }
    
    for (const auto& buffer : allocator_->buffers(stream_)) {
        std::vector<void*> planeMappings;
        for (const auto& plane : buffer->planes()) {
            void* memory = mmap(nullptr, plane.length, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), 0);
            if (memory == MAP_FAILED) {
                std::cerr << "[ERROR] Failed to mmap buffer plane" << std::endl;
                return false;
            }
            planeMappings.push_back(memory);
        }
        buffer_plane_mappings_.push_back(planeMappings);
    }
    
    std::cout << "[INFO] " << buffer_plane_mappings_.size() << " DMA buffers mapped successfully." << std::endl;
    return true;
}

bool ZeroCopyCapture::start() {
    if (!camera_) {
        std::cerr << "[ERROR] Cannot start, camera not initialized." << std::endl;
        return false;
    }

    stopping_.store(false);
    camera_->requestCompleted.connect(this, &ZeroCopyCapture::onRequestCompleted);

    std::vector<std::unique_ptr<Request>> requests;
    for (const auto& buffer : allocator_->buffers(stream_)) {
        auto request = camera_->createRequest();
        if (!request || request->addBuffer(stream_, buffer.get()) < 0) {
            std::cerr << "[ERROR] Failed to create request or add buffer" << std::endl;
            return false;
        }
        requests.push_back(std::move(request));
    }

    ControlList controls;
    int64_t frame_time = 1000000 / video_config_.fps;
    controls.set(controls::FrameDurationLimits, Span<const int64_t, 2>({frame_time, frame_time}));

    std::cout << "[INFO] Starting camera..." << std::endl;
    if (camera_->start(&controls)) {
        std::cerr << "[ERROR] Failed to start camera" << std::endl;
        return false;
    }
    
    for (auto& request : requests) {
        camera_->queueRequest(request.release());
    }
    
    std::cout << "[INFO] Camera started and initial requests queued." << std::endl;
    return true;
}

void ZeroCopyCapture::stop() {
    if (stopping_.exchange(true)) return;
    
    std::cout << "[INFO] Stopping ZeroCopyCapture..." << std::endl;
    
    frame_queue_.stop();
    
    if (camera_) {
        camera_->stop();
        camera_->requestCompleted.disconnect(this, &ZeroCopyCapture::onRequestCompleted);
    }
}

void ZeroCopyCapture::cleanup() {
    std::cout << "[INFO] Cleaning up ZeroCopyCapture resources..." << std::endl;
    stop();
    
    if (allocator_) {
        const auto& buffers = allocator_->buffers(stream_);
        for (size_t i = 0; i < buffer_plane_mappings_.size(); ++i) {
            for (size_t j = 0; j < buffer_plane_mappings_[i].size(); ++j) {
                if (buffer_plane_mappings_[i][j] != MAP_FAILED) {
                    munmap(buffer_plane_mappings_[i][j], buffers[i]->planes()[j].length);
                }
            }
        }
    }
    
    if (camera_) {
        camera_->release();
    }
    
    std::cout << "[INFO] ZeroCopyCapture cleanup complete." << std::endl;
}

void ZeroCopyCapture::setFrameCallback(std::function<void(const FrameData&)> callback) {
    frame_callback_ = callback;
}

void ZeroCopyCapture::onRequestCompleted(Request* request) {
    if (stopping_.load()) {
        request->reuse(Request::ReuseBuffers);
        camera_->queueRequest(request);
        return;
    }

    if (request->status() != Request::RequestComplete) {
        if (request->status() != Request::RequestCancelled) {
             std::cerr << "[WARN] Request failed with status " << request->status() << std::endl;
        }
        request->reuse(Request::ReuseBuffers);
        camera_->queueRequest(request);
        return;
    }

    FrameBuffer* buffer = request->buffers().begin()->second;
    size_t bufferIndex = 0;
    const auto& buffers = allocator_->buffers(stream_);
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i].get() == buffer) {
            bufferIndex = i;
            break;
        }
    }
    
    FrameData frame_data = {
        buffer_plane_mappings_[bufferIndex][0], 
        buffer->planes()[0].length, 
        bufferIndex
    };
    
    // 콜백이 설정되어 있으면 호출
    if (frame_callback_) {
        frame_callback_(frame_data);
    }
    
    request->reuse(Request::ReuseBuffers);
    camera_->queueRequest(request);
}

PixelFormat ZeroCopyCapture::getPixelFormat(const std::string& format_str) {
    if (format_str == "BGR888") {
        return formats::BGR888;
    } else if (format_str == "RGB888") {
        return formats::RGB888;
    } else if (format_str == "YUV420") {
        return formats::YUV420;
    } else if (format_str == "YUYV") {
        return formats::YUYV;
    } else {
        std::cout << "[WARN] Unknown pixel format: " << format_str << ", using BGR888 as default" << std::endl;
        return formats::BGR888;
    }
}
