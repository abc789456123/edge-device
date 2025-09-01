#include "RtspClient.h"
#include <iostream>
#include <iomanip>

RtspClient::RtspClient(const std::string& rtsp_url) 
    : pipeline_(nullptr), source_(nullptr), depay_(nullptr), decoder_(nullptr), 
      converter_(nullptr), sink_(nullptr), loop_(nullptr), running_(false), 
      rtsp_url_(rtsp_url), frame_count_(0) {
    
    gst_init(nullptr, nullptr);
}

RtspClient::~RtspClient() {
    stop();
    gst_deinit();
}

bool RtspClient::initialize() {
    std::cout << "[INFO] Initializing RTSP client for: " << rtsp_url_ << std::endl;
    
    // GStreamer 요소들 생성 (더 안정적인 디코더 사용)
    pipeline_ = gst_pipeline_new("rtsp-client");
    source_ = gst_element_factory_make("rtspsrc", "source");
    depay_ = gst_element_factory_make("rtph264depay", "depay");
    
    // 하드웨어 디코더 시도, 실패시 소프트웨어 디코더 사용
    decoder_ = gst_element_factory_make("v4l2h264dec", "decoder");
    if (!decoder_) {
        std::cout << "[WARN] Hardware H264 decoder not available, using software decoder" << std::endl;
        decoder_ = gst_element_factory_make("avdec_h264", "decoder");
    }
    
    converter_ = gst_element_factory_make("videoconvert", "converter");
    sink_ = gst_element_factory_make("fakesink", "sink");
    
    if (!pipeline_ || !source_ || !depay_ || !decoder_ || !converter_ || !sink_) {
        std::cerr << "[ERROR] Failed to create GStreamer elements" << std::endl;
        return false;
    }
    
    // rtspsrc 설정 (더 안정적인 설정)
    g_object_set(G_OBJECT(source_), 
                 "location", rtsp_url_.c_str(),
                 "protocols", 0x00000004,  // TCP만 사용 (더 안정적)
                 "latency", 0,             // 지연시간 최소화
                 "timeout", 5000000,       // 5초 타임아웃
                 "tcp-timeout", 5000000,
                 "retry", 5,
                 "drop-on-latency", TRUE,
                 NULL);
    
    // fakesink 설정 (실제 출력하지 않고 프레임만 카운트)
    g_object_set(G_OBJECT(sink_), 
                 "sync", TRUE,
                 "signal-handoffs", TRUE,
                 NULL);
    
    // 파이프라인에 요소들 추가
    gst_bin_add_many(GST_BIN(pipeline_), source_, depay_, decoder_, converter_, sink_, NULL);
    
    // 요소들 연결 (rtspsrc는 동적 패드이므로 나중에 연결)
    if (!gst_element_link_many(depay_, decoder_, converter_, sink_, NULL)) {
        std::cerr << "[ERROR] Failed to link elements" << std::endl;
        return false;
    }
    
    // rtspsrc 동적 패드 연결을 위한 신호 연결
    g_signal_connect(source_, "pad-added", G_CALLBACK(+[](GstElement* src, GstPad* new_pad, gpointer data) {
        RtspClient* client = static_cast<RtspClient*>(data);
        
        GstCaps* new_pad_caps = gst_pad_get_current_caps(new_pad);
        if (!new_pad_caps) {
            new_pad_caps = gst_pad_query_caps(new_pad, NULL);
        }
        
        if (new_pad_caps) {
            GstStructure* new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
            const gchar* new_pad_type = gst_structure_get_name(new_pad_struct);
            
            std::cout << "[DEBUG] New pad type: " << new_pad_type << std::endl;
            
            if (g_str_has_prefix(new_pad_type, "application/x-rtp")) {
                GstPad* sink_pad = gst_element_get_static_pad(client->depay_, "sink");
                if (gst_pad_link(new_pad, sink_pad) == GST_PAD_LINK_OK) {
                    std::cout << "[INFO] Successfully linked rtspsrc to depayloader" << std::endl;
                } else {
                    std::cerr << "[ERROR] Failed to link rtspsrc to depayloader" << std::endl;
                }
                gst_object_unref(sink_pad);
            }
            gst_caps_unref(new_pad_caps);
        }
    }), this);
    
    // 프레임 카운트를 위한 프로브 추가
    GstPad* sink_pad = gst_element_get_static_pad(sink_, "sink");
    gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER, probe_callback, this, NULL);
    gst_object_unref(sink_pad);
    
    return true;
}

bool RtspClient::start() {
    if (running_.load()) {
        std::cout << "[WARN] Client is already running" << std::endl;
        return false;
    }
    
    loop_ = g_main_loop_new(NULL, FALSE);
    
    // 버스 메시지 핸들링 설정
    GstBus* bus = gst_element_get_bus(pipeline_);
    gst_bus_add_watch(bus, bus_callback, this);
    gst_object_unref(bus);
    
    // 파이프라인 시작
    std::cout << "[INFO] Starting pipeline..." << std::endl;
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR] Failed to start pipeline" << std::endl;
        return false;
    }
    
    running_.store(true);
    start_time_ = std::chrono::steady_clock::now();
    
    // 메인 루프를 별도 스레드에서 실행
    main_thread_ = std::thread([this]() {
        std::cout << "[INFO] Main loop started" << std::endl;
        g_main_loop_run(loop_);
        std::cout << "[INFO] Main loop finished" << std::endl;
    });
    
    return true;
}

void RtspClient::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    
    std::cout << "[INFO] Stopping RTSP client..." << std::endl;
    
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    
    if (loop_) {
        g_main_loop_quit(loop_);
    }
    
    if (main_thread_.joinable()) {
        main_thread_.join();
    }
    
    if (loop_) {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }
    
    std::cout << "[INFO] RTSP client stopped" << std::endl;
}

double RtspClient::getElapsedTime() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    return duration.count() / 1000.0;
}

gboolean RtspClient::bus_callback(GstBus* bus, GstMessage* message, gpointer data) {
    RtspClient* client = static_cast<RtspClient*>(data);
    client->handleMessage(message);
    return TRUE;
}

GstPadProbeReturn RtspClient::probe_callback(GstPad* pad, GstPadProbeInfo* info, gpointer user_data) {
    RtspClient* client = static_cast<RtspClient*>(user_data);
    
    if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
        client->frame_count_++;
        
        int count = client->frame_count_.load();
        if (count % 30 == 0) {  // 매초마다 출력
            double elapsed = client->getElapsedTime();
            double fps = count / elapsed;
            std::cout << "[STATS] Frames: " << count 
                      << ", Elapsed: " << std::fixed << std::setprecision(1) << elapsed << "s"
                      << ", FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;
        }
    }
    
    return GST_PAD_PROBE_OK;
}

void RtspClient::handleMessage(GstMessage* message) {
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug_info;
            gst_message_parse_error(message, &err, &debug_info);
            std::cerr << "[ERROR] " << err->message << std::endl;
            if (debug_info) {
                std::cerr << "[DEBUG] " << debug_info << std::endl;
                g_free(debug_info);
            }
            g_error_free(err);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err;
            gchar* debug_info;
            gst_message_parse_warning(message, &err, &debug_info);
            std::cout << "[WARN] " << err->message << std::endl;
            if (debug_info) {
                std::cout << "[DEBUG] " << debug_info << std::endl;
                g_free(debug_info);
            }
            g_error_free(err);
            break;
        }
        case GST_MESSAGE_INFO: {
            GError* err;
            gchar* debug_info;
            gst_message_parse_info(message, &err, &debug_info);
            std::cout << "[INFO] " << err->message << std::endl;
            if (debug_info) {
                std::cout << "[DEBUG] " << debug_info << std::endl;
                g_free(debug_info);
            }
            g_error_free(err);
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "[INFO] End of stream" << std::endl;
            if (loop_) {
                g_main_loop_quit(loop_);
            }
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline_)) {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
                std::cout << "[INFO] Pipeline state changed from " 
                          << gst_element_state_get_name(old_state) << " to " 
                          << gst_element_state_get_name(new_state) << std::endl;
            }
            break;
        }
        default:
            break;
    }
}
