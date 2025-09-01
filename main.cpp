#include "main.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono;
using namespace std::literals::chrono_literals;

// 전역 변수 정의
std::atomic<bool> g_should_exit{false};
CameraStreamerApp* g_app_instance = nullptr;

void globalSignalHandler(int signal) {
    std::cout << "\n[INFO] Signal " << signal << " received. Exiting gracefully..." << std::endl;
    g_should_exit.store(true);
    if (g_app_instance) {
        g_app_instance->signalHandler(signal);
    }
}

CameraStreamerApp::CameraStreamerApp() : should_exit_(false), frame_count_(0) {
}

CameraStreamerApp::~CameraStreamerApp() {
    stop();
}

bool CameraStreamerApp::initialize(const std::string& config_file) {
    std::cout << "[INFO] Initializing CameraStreamerApp..." << std::endl;
    
    // 설정 로드
    config_manager_ = std::make_unique<ConfigManager>();
    if (!config_manager_->loadFromFile(config_file)) {
        std::cerr << "[WARN] Failed to load config file, using default settings" << std::endl;
    }
    config_manager_->printConfig();
    
    // 카메라 캡처 초기화
    camera_capture_ = std::make_unique<ZeroCopyCapture>(config_manager_->getVideoConfig());
    if (!camera_capture_->initialize()) {
        std::cerr << "[ERROR] Failed to initialize camera capture" << std::endl;
        return false;
    }
    
    // RTSP 스트리머 초기화
    rtsp_streamer_ = std::make_unique<RtspStreamer>(
        config_manager_->getVideoConfig(), 
        config_manager_->getRtspConfig()
    );
    
    // 프레임 콜백 설정
    camera_capture_->setFrameCallback(
        [this](const FrameData& frame_data) {
            onFrameReceived(frame_data);
        }
    );
    
    std::cout << "[INFO] CameraStreamerApp initialized successfully" << std::endl;
    return true;
}

bool CameraStreamerApp::start() {
    std::cout << "[INFO] Starting CameraStreamerApp..." << std::endl;
    
    // RTSP 서버 시작
    if (!rtsp_streamer_->start()) {
        std::cerr << "[ERROR] Failed to start RTSP streamer" << std::endl;
        return false;
    }
    
    // 카메라 캡처 시작
    if (!camera_capture_->start()) {
        std::cerr << "[ERROR] Failed to start camera capture" << std::endl;
        return false;
    }
    
    should_exit_.store(false);
    std::cout << "[INFO] CameraStreamerApp started successfully" << std::endl;
    return true;
}

void CameraStreamerApp::stop() {
    if (should_exit_.exchange(true)) return;
    
    std::cout << "[INFO] Stopping CameraStreamerApp..." << std::endl;
    
    if (camera_capture_) {
        camera_capture_->stop();
    }
    
    if (rtsp_streamer_) {
        rtsp_streamer_->stop();
    }
    
    std::cout << "[INFO] CameraStreamerApp stopped" << std::endl;
}

void CameraStreamerApp::run() {
    while (!should_exit_.load() && !g_should_exit.load()) {
        std::this_thread::sleep_for(100ms);
    }
    
    std::cout << "\n[INFO] Main loop exited. Stopping application..." << std::endl;
    stop();
}

void CameraStreamerApp::signalHandler(int signal) {
    should_exit_.store(true);
    stop();
}

void CameraStreamerApp::onFrameReceived(const FrameData& frame_data) {
    if (should_exit_.load()) {
        return;
    }
    
    // RTSP 스트리머로 프레임 전송
    if (rtsp_streamer_) {
        rtsp_streamer_->pushFrame(frame_data);
    }
    
    // 프레임 카운터 업데이트
    frame_count_++;
    if (frame_count_ % (config_manager_->getVideoConfig().fps * 5) == 0) {
        std::cout << "[DEBUG] " << frame_count_ << " frames processed and sent to RTSP server." << std::endl;
    }
}
