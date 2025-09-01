#ifndef RTSP_CLIENT_H
#define RTSP_CLIENT_H

#include <gst/gst.h>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>

class RtspClient {
private:
    GstElement* pipeline_;
    GstElement* source_;
    GstElement* depay_;
    GstElement* decoder_;
    GstElement* converter_;
    GstElement* sink_;
    
    GMainLoop* loop_;
    std::thread main_thread_;
    std::atomic<bool> running_;
    
    std::string rtsp_url_;
    std::atomic<int> frame_count_;
    std::chrono::steady_clock::time_point start_time_;

public:
    RtspClient(const std::string& rtsp_url);
    ~RtspClient();

    bool initialize();
    bool start();
    void stop();
    
    int getFrameCount() const { return frame_count_.load(); }
    double getElapsedTime() const;

private:
    static gboolean bus_callback(GstBus* bus, GstMessage* message, gpointer data);
    static GstPadProbeReturn probe_callback(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    void handleMessage(GstMessage* message);
};

#endif // RTSP_CLIENT_H
