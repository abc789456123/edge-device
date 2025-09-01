/*

※ How to Compile

g++ -std=c++17 -g -O2 -Wall -I/usr/include/libcamera \
`pkg-config --cflags gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0` \
-o zero_copy_rtsp_demo main.cpp ConfigManager.cpp ZeroCopyCapture.cpp RtspStreamer.cpp -lcamera -lcamera-base \
`pkg-config --libs gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0` -lpthread

*/

#include "main.h"
#include <iostream>

int main() {
    // 시그널 핸들러 등록
    signal(SIGINT, globalSignalHandler);
    signal(SIGTERM, globalSignalHandler);

    std::cout << "========================================================" << std::endl;
    std::cout << "   Zero-Copy Camera to RTSP Streamer (Refactored)" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        CameraStreamerApp app;
        g_app_instance = &app;

        if (!app.initialize()) {
            std::cerr << "[FATAL] Initialization failed. Exiting." << std::endl;
            return -1;
        }

        if (!app.start()) {
            std::cerr << "[FATAL] Application start failed. Exiting." << std::endl;
            return -1;
        }

        app.run();

        std::cout << "[INFO] Application stopped successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] An unhandled exception occurred: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
