#include <gst/gst.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

static std::atomic<int> frame_count{0};
static std::atomic<bool> running{true};

// 간단한 프레임 카운터 프로브
static GstPadProbeReturn
frame_probe(GstPad* pad, GstPadProbeInfo* info, gpointer user_data) {
    if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
        frame_count++;
        if (frame_count % 30 == 0) {
            std::cout << "[INFO] Received " << frame_count.load() << " frames" << std::endl;
        }
    }
    return GST_PAD_PROBE_OK;
}

// 버스 메시지 핸들러
static gboolean
bus_call(GstBus* bus, GstMessage* msg, gpointer data) {
    GMainLoop* loop = (GMainLoop*)data;
    
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            std::cout << "[INFO] End of stream" << std::endl;
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug;
            gst_message_parse_error(msg, &err, &debug);
            std::cerr << "[ERROR] " << err->message << std::endl;
            if (debug) {
                std::cerr << "[DEBUG] " << debug << std::endl;
                g_free(debug);
            }
            g_error_free(err);
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err;
            gchar* debug;
            gst_message_parse_warning(msg, &err, &debug);
            std::cout << "[WARN] " << err->message << std::endl;
            if (debug) {
                std::cout << "[DEBUG] " << debug << std::endl;
                g_free(debug);
            }
            g_error_free(err);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data)) {
                GstState old_state, new_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
                std::cout << "[INFO] State changed: " 
                          << gst_element_state_get_name(old_state) << " -> " 
                          << gst_element_state_get_name(new_state) << std::endl;
            }
            break;
        }
        default:
            break;
    }
    return TRUE;
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);
    
    std::string uri = "rtsp://localhost:8554/stream";
    if (argc > 1) {
        uri = argv[1];
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "    Simple RTSP Test Client" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Connecting to: " << uri << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 간단한 파이프라인 생성 (playbin 사용)
    GstElement* pipeline = gst_element_factory_make("playbin", "player");
    if (!pipeline) {
        std::cerr << "[ERROR] Failed to create playbin" << std::endl;
        return -1;
    }
    
    // URI 설정
    g_object_set(G_OBJECT(pipeline), "uri", uri.c_str(), NULL);
    
    // 비디오 싱크를 fakesink로 설정 (화면 출력 없이 테스트)
    GstElement* video_sink = gst_element_factory_make("fakesink", "video-sink");
    g_object_set(G_OBJECT(video_sink), "sync", TRUE, NULL);
    g_object_set(G_OBJECT(pipeline), "video-sink", video_sink, NULL);
    
    // 오디오 싱크도 fakesink로 설정
    GstElement* audio_sink = gst_element_factory_make("fakesink", "audio-sink");
    g_object_set(G_OBJECT(pipeline), "audio-sink", audio_sink, NULL);
    
    // 버스 설정
    GstBus* bus = gst_element_get_bus(pipeline);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);
    
    // 프레임 카운터 프로브 추가 (나중에 연결됨)
    
    std::cout << "[INFO] Starting pipeline..." << std::endl;
    
    // 파이프라인 시작
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR] Unable to set pipeline to playing state" << std::endl;
        gst_object_unref(pipeline);
        return -1;
    }
    
    // 통계 출력 스레드
    std::thread stats_thread([&]() {
        int last_count = 0;
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            int current = frame_count.load();
            int diff = current - last_count;
            double fps = diff / 5.0;
            std::cout << "[STATS] Total: " << current 
                      << ", Recent FPS: " << fps << std::endl;
            last_count = current;
        }
    });
    
    std::cout << "[INFO] Running... Press Ctrl+C to stop" << std::endl;
    
    // 메인 루프 실행
    g_main_loop_run(loop);
    
    running.store(false);
    if (stats_thread.joinable()) {
        stats_thread.join();
    }
    
    std::cout << "\n[INFO] Stopping..." << std::endl;
    gst_element_set_state(pipeline, GST_STATE_NULL);
    
    std::cout << "[FINAL] Total frames received: " << frame_count.load() << std::endl;
    
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);
    
    return 0;
}
