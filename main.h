#ifndef MAIN_H
#define MAIN_H

#include <atomic>
#include <memory>
#include <csignal>

#include "ConfigManager.h"
#include "ZeroCopyCapture.h"
#include "RtspStreamer.h"

class CameraStreamerApp {
private:
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<ZeroCopyCapture> camera_capture_;
    std::unique_ptr<RtspStreamer> rtsp_streamer_;
    
    std::atomic<bool> should_exit_;
    std::atomic<size_t> frame_count_;

public:
    CameraStreamerApp();
    ~CameraStreamerApp();

    bool initialize(const std::string& config_file = "config.json");
    bool start();
    void stop();
    
    void run();
    
    void signalHandler(int signal);

private:
    void onFrameReceived(const FrameData& frame_data);
};

// 전역 변수
extern std::atomic<bool> g_should_exit;
extern CameraStreamerApp* g_app_instance;

// 시그널 핸들러
void globalSignalHandler(int signal);

#endif // MAIN_H
