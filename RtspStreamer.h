#ifndef RTSP_STREAMER_H
#define RTSP_STREAMER_H

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>

#include <string>
#include <thread>
#include <atomic>
#include <memory>

#include "ConfigManager.h"
#include "ZeroCopyCapture.h"

class RtspStreamer {
private:
    GMainLoop* loop_;
    GstRTSPServer* server_;
    GstRTSPMountPoints* mounts_;
    GstRTSPMediaFactory* factory_;
    GstAppSrc* appsrc_;
    
    std::thread server_thread_;
    std::atomic<bool> is_running_;
    
    VideoConfig video_config_;
    RtspConfig rtsp_config_;
    
    GstClockTime timestamp_;

public:
    RtspStreamer(const VideoConfig& video_config, const RtspConfig& rtsp_config);
    ~RtspStreamer();

    bool start();
    void stop();
    
    void pushFrame(const FrameData& frame_data);
    
    bool isRunning() const { return is_running_.load(); }

private:
    static void media_configure_callback(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data);
    void on_media_configure(GstRTSPMedia* media);
};

#endif // RTSP_STREAMER_H
