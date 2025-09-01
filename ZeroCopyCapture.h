#ifndef ZERO_COPY_CAPTURE_H
#define ZERO_COPY_CAPTURE_H

#include <libcamera/libcamera.h>
#include <libcamera/framebuffer.h>
#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/controls.h>
#include <libcamera/stream.h>

#include <memory>
#include <vector>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#include "ConfigManager.h"

// 프레임 데이터를 담을 구조체
struct FrameData {
    void* data;
    size_t size;
    size_t buffer_index;
};

// 스레드 안전 큐 (Blocking Pop 기능 추가)
template <typename T>
class ThreadSafeQueue {
private:
    mutable std::mutex mtx;
    std::queue<T> data_queue;
    std::condition_variable cv;
    std::atomic<bool> stopped{false};

public:
    void push(T new_value) {
        if (stopped) return;
        std::lock_guard<std::mutex> lock(mtx);
        data_queue.push(std::move(new_value));
        cv.notify_one();
    }

    // 대기하며 pop 하는 함수
    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !data_queue.empty() || stopped.load(); });
        if (stopped.load() && data_queue.empty()) {
            return false;
        }
        value = std::move(data_queue.front());
        data_queue.pop();
        return true;
    }
    
    void stop() {
        stopped.store(true);
        cv.notify_all(); // 모든 대기 중인 스레드를 깨움
    }
};

class ZeroCopyCapture {
private:
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraManager> camera_manager_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    libcamera::Stream* stream_;
    std::shared_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::vector<std::vector<void*>> buffer_plane_mappings_;
    
    std::atomic<bool> stopping_;
    ThreadSafeQueue<FrameData> frame_queue_;
    
    VideoConfig video_config_;
    
    // 프레임 처리 콜백
    std::function<void(const FrameData&)> frame_callback_;

public:
    ZeroCopyCapture(const VideoConfig& config);
    ~ZeroCopyCapture();

    bool initialize();
    bool start();
    void stop();
    
    void setFrameCallback(std::function<void(const FrameData&)> callback);
    
    bool isRunning() const { return !stopping_.load(); }

private:
    bool setupBuffers();
    void cleanup();
    void onRequestCompleted(libcamera::Request* request);
    
    libcamera::PixelFormat getPixelFormat(const std::string& format_str);
};

#endif // ZERO_COPY_CAPTURE_H
