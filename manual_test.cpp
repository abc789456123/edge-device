#include <gst/gst.h>
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

static std::atomic<int> frame_count{0};
static std::atomic<bool> running{true};
static GMainLoop* loop = nullptr;

static void signal_handler(int sig) {
    std::cout << "\n[INFO] Signal received, stopping..." << std::endl;
    running.store(false);
    if (loop) {
        g_main_loop_quit(loop);
    }
}

// 프레임 카운터 프로브
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

// 동적 패드 연결 콜백
static void on_pad_added(GstElement* element, GstPad* pad, gpointer data) {
    GstElement* depay = GST_ELEMENT(data);
    GstPad* sinkpad;
    
    gchar* name = gst_pad_get_name(pad);
    std::cout << "[INFO] New pad: " << name << std::endl;
    
    if (g_str_has_prefix(name, "recv_rtp_src_")) {
        sinkpad = gst_element_get_static_pad(depay, "sink");
        if (gst_pad_link(pad, sinkpad) == GST_PAD_LINK_OK) {
            std::cout << "[INFO] Successfully linked RTSP src to depay" << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to link pads" << std::endl;
        }
        gst_object_unref(sinkpad);
    }
    
    g_free(name);
}

// 버스 메시지 핸들러
static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data) {
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
                g_free(debug);
            }
            g_error_free(err);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data)) {
                GstState old_state, new_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
                std::cout << "[INFO] Pipeline state: " 
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
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    gst_init(&argc, &argv);
    
    std::string uri = "rtsp://192.168.0.4:8554/stream";
    if (argc > 1) {
        uri = argv[1];
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "   Manual Pipeline RTSP Test Client" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Connecting to: " << uri << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 수동으로 파이프라인 구성
    GstElement* pipeline = gst_pipeline_new("test-pipeline");
    GstElement* rtspsrc = gst_element_factory_make("rtspsrc", "source");
    GstElement* depay = gst_element_factory_make("rtph264depay", "depay");
    GstElement* parse = gst_element_factory_make("h264parse", "parse");
    GstElement* decoder = gst_element_factory_make("avdec_h264", "decoder");
    GstElement* convert = gst_element_factory_make("videoconvert", "convert");
    GstElement* sink = gst_element_factory_make("fakesink", "sink");
    
    if (!pipeline || !rtspsrc || !depay || !parse || !decoder || !convert || !sink) {
        std::cerr << "[ERROR] Failed to create pipeline elements" << std::endl;
        return -1;
    }
    
    // RTSP 소스 설정
    g_object_set(G_OBJECT(rtspsrc), 
                 "location", uri.c_str(),
                 "protocols", 0x00000004,  // TCP only
                 "latency", 100,
                 NULL);
    
    // Fakesink 설정
    g_object_set(G_OBJECT(sink), "sync", TRUE, NULL);
    
    // 파이프라인에 요소 추가
    gst_bin_add_many(GST_BIN(pipeline), rtspsrc, depay, parse, decoder, convert, sink, NULL);
    
    // 정적 연결 (rtspsrc는 동적 패드이므로 제외)
    if (!gst_element_link_many(depay, parse, decoder, convert, sink, NULL)) {
        std::cerr << "[ERROR] Failed to link elements" << std::endl;
        return -1;
    }
    
    // 동적 패드 연결 설정
    g_signal_connect(rtspsrc, "pad-added", G_CALLBACK(on_pad_added), depay);
    
    // 프레임 카운터 프로브 추가
    GstPad* sink_pad = gst_element_get_static_pad(sink, "sink");
    gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER, frame_probe, NULL, NULL);
    gst_object_unref(sink_pad);
    
    // 버스 설정
    GstBus* bus = gst_element_get_bus(pipeline);
    loop = g_main_loop_new(NULL, FALSE);
    gst_bus_add_watch(bus, bus_call, pipeline);
    gst_object_unref(bus);
    
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
            std::this_thread::sleep_for(std::chrono::seconds(3));
            int current = frame_count.load();
            int diff = current - last_count;
            double fps = diff / 3.0;
            std::cout << "[STATS] Total: " << current 
                      << ", Recent FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;
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
