#include "RtspStreamer.h"
#include <iostream>

RtspStreamer::RtspStreamer(const VideoConfig& video_config, const RtspConfig& rtsp_config)
    : loop_(nullptr), server_(nullptr), mounts_(nullptr), factory_(nullptr), appsrc_(nullptr),
      is_running_(false), video_config_(video_config), rtsp_config_(rtsp_config), timestamp_(0) {
    std::cout << "[INFO] Initializing GStreamer..." << std::endl;
    gst_init(nullptr, nullptr);
}

RtspStreamer::~RtspStreamer() {
    stop();
    std::cout << "[INFO] Deinitializing GStreamer..." << std::endl;
    gst_deinit();
}

bool RtspStreamer::start() {
    loop_ = g_main_loop_new(NULL, FALSE);
    server_ = gst_rtsp_server_new();
    g_object_set(server_, "service", std::to_string(rtsp_config_.port).c_str(), NULL);
    mounts_ = gst_rtsp_server_get_mount_points(server_);
    factory_ = gst_rtsp_media_factory_new();

    std::cout << "[DEBUG] GStreamer Pipeline: " << rtsp_config_.pipeline << std::endl;
    gst_rtsp_media_factory_set_launch(factory_, rtsp_config_.pipeline.c_str());
    gst_rtsp_media_factory_set_shared(factory_, TRUE);

    g_signal_connect(factory_, "media-configure", (GCallback)media_configure_callback, this);
    
    gst_rtsp_mount_points_add_factory(mounts_, rtsp_config_.mount_point.c_str(), factory_);
    g_object_unref(mounts_);

    if (gst_rtsp_server_attach(server_, NULL) == 0) {
        std::cerr << "[ERROR] Failed to attach RTSP server. Ensure the port is not in use." << std::endl;
        return false;
    }

    is_running_.store(true);
    server_thread_ = std::thread([this]() {
        std::cout << "[INFO] RTSP stream ready at: rtsp://<your-ip-address>:" 
                  << rtsp_config_.port << rtsp_config_.mount_point << std::endl;
        g_main_loop_run(loop_);
        std::cout << "[INFO] GStreamer main loop finished." << std::endl;
    });

    return true;
}

void RtspStreamer::stop() {
    if (is_running_.exchange(false)) {
        std::cout << "[INFO] Stopping RTSP server..." << std::endl;
        if (loop_) {
            g_main_loop_quit(loop_);
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        if (server_) {
            g_object_unref(server_);
            server_ = nullptr;
        }
        if (loop_) {
            g_main_loop_unref(loop_);
            loop_ = nullptr;
        }
        std::cout << "[INFO] RTSP server stopped." << std::endl;
    }
}

void RtspStreamer::pushFrame(const FrameData& frame_data) {
    if (!is_running_.load() || !appsrc_) {
        return;
    }

    GstState state;
    GstStateChangeReturn ret_state = gst_element_get_state(GST_ELEMENT(appsrc_), &state, nullptr, GST_CLOCK_TIME_NONE);
    if (ret_state != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_PLAYING) {
        return;
    }

    GstBuffer* buffer = gst_buffer_new();
    GstMemory* memory = gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY, frame_data.data, frame_data.size, 0, frame_data.size, nullptr, nullptr);
    gst_buffer_append_memory(buffer, memory);

    GstFlowReturn ret;
    g_signal_emit_by_name(appsrc_, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);

    if (ret != GST_FLOW_OK) {
        std::cerr << "[WARN] Error pushing buffer to appsrc, flow return: " << gst_flow_get_name(ret) << std::endl;
    }
}

void RtspStreamer::media_configure_callback(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data) {
    RtspStreamer* self = static_cast<RtspStreamer*>(user_data);
    self->on_media_configure(media);
}

void RtspStreamer::on_media_configure(GstRTSPMedia* media) {
    std::cout << "[DEBUG] Media configure callback triggered." << std::endl;
    GstElement* pipeline = gst_rtsp_media_get_element(media);
    GstElement* appsrc_element = gst_bin_get_by_name(GST_BIN(pipeline), "mysrc");

    if (!appsrc_element) {
        std::cerr << "[ERROR] Could not find appsrc element 'mysrc' in pipeline" << std::endl;
        return;
    }
    
    // Appsrc Caps 설정 - BGR888과 RGB888 구분
    std::string format_str;
    if (video_config_.pixel_format == "BGR888") {
        format_str = "BGR";  // BGR888 데이터는 BGR 포맷으로 설정
    } else if (video_config_.pixel_format == "RGB888") {
        format_str = "RGB";
    } else {
        format_str = "BGR";  // 기본값을 BGR로 변경
    }
    
    GstCaps* caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, format_str.c_str(),
                                        "width", G_TYPE_INT, video_config_.width,
                                        "height", G_TYPE_INT, video_config_.height,
                                        "framerate", GST_TYPE_FRACTION, video_config_.fps, 1,
                                        "interlace-mode", G_TYPE_STRING, "progressive",
                                        "colorimetry", G_TYPE_STRING, "srgb",
                                        NULL);
    
    std::cout << "[DEBUG] Setting appsrc caps to: " << gst_caps_to_string(caps) << std::endl;

    g_object_set(G_OBJECT(appsrc_element),
                 "caps", caps,
                 "format", GST_FORMAT_TIME,
                 "is-live", TRUE,
                 "do-timestamp", TRUE,
                 NULL);
    gst_caps_unref(caps);

    appsrc_ = GST_APP_SRC(appsrc_element);
    g_object_unref(appsrc_element);
}
