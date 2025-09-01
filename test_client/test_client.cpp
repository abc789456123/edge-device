#include "RtspClient.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>

static std::atomic<bool> should_exit{false};
static RtspClient* client_instance = nullptr;

void signalHandler(int signal) {
    std::cout << "\n[INFO] Signal " << signal << " received. Stopping client..." << std::endl;
    should_exit.store(true);
    if (client_instance) {
        client_instance->stop();
    }
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [RTSP_URL]" << std::endl;
    std::cout << "Default URL: rtsp://localhost:8554/stream" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << std::endl;
    std::cout << "  " << program_name << " rtsp://192.168.1.100:8554/stream" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::string rtsp_url = "rtsp://localhost:8554/stream";
    
    if (argc == 2) {
        if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        rtsp_url = argv[1];
    } else if (argc > 2) {
        std::cerr << "[ERROR] Too many arguments" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "        RTSP Stream Test Client" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Target URL: " << rtsp_url << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        RtspClient client(rtsp_url);
        client_instance = &client;
        
        if (!client.initialize()) {
            std::cerr << "[FATAL] Failed to initialize RTSP client" << std::endl;
            return 1;
        }
        
        if (!client.start()) {
            std::cerr << "[FATAL] Failed to start RTSP client" << std::endl;
            return 1;
        }
        
        std::cout << "[INFO] Client started. Press Ctrl+C to stop." << std::endl;
        
        // 통계 출력 루프
        auto last_stats_time = std::chrono::steady_clock::now();
        int last_frame_count = 0;
        
        while (!should_exit.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto now = std::chrono::steady_clock::now();
            auto stats_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time);
            
            if (stats_elapsed.count() >= 5) {  // 5초마다 통계 출력
                int current_frames = client.getFrameCount();
                double total_elapsed = client.getElapsedTime();
                
                if (total_elapsed > 0) {
                    double avg_fps = current_frames / total_elapsed;
                    double recent_fps = (current_frames - last_frame_count) / stats_elapsed.count();
                    
                    std::cout << "[SUMMARY] Total frames: " << current_frames 
                              << ", Avg FPS: " << std::fixed << std::setprecision(1) << avg_fps
                              << ", Recent FPS: " << std::fixed << std::setprecision(1) << recent_fps << std::endl;
                }
                
                last_stats_time = now;
                last_frame_count = current_frames;
            }
        }
        
        client.stop();
        
        // 최종 통계
        int total_frames = client.getFrameCount();
        double total_time = client.getElapsedTime();
        
        std::cout << "\n========== Final Statistics ==========" << std::endl;
        std::cout << "Total frames received: " << total_frames << std::endl;
        std::cout << "Total time: " << std::fixed << std::setprecision(1) << total_time << " seconds" << std::endl;
        if (total_time > 0) {
            std::cout << "Average FPS: " << std::fixed << std::setprecision(1) << (total_frames / total_time) << std::endl;
        }
        std::cout << "=======================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
