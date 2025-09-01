#include "ConfigManager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

ConfigManager::ConfigManager() : loaded_(false) {
    // 기본값 설정
    video_config_ = {1920, 1080, 30, "BGR888", 8};
    rtsp_config_ = {8554, "/stream", 2000000, "v4l2h264enc", 
                   "appsrc name=mysrc ! queue ! v4l2convert ! video/x-raw,format=NV12 ! queue ! v4l2h264enc ! video/x-h264,level=(string)4 ! rtph264pay name=pay0 pt=96"};
}

bool ConfigManager::loadFromFile(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open config file: " << config_file << std::endl;
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // 간단한 JSON 파싱 (실제 프로젝트에서는 nlohmann/json 등을 사용 권장)
    try {
        // video 설정 파싱
        size_t video_pos = content.find("\"video\"");
        if (video_pos != std::string::npos) {
            size_t width_pos = content.find("\"width\"", video_pos);
            if (width_pos != std::string::npos) {
                size_t colon_pos = content.find(":", width_pos);
                size_t comma_pos = content.find(",", colon_pos);
                std::string width_str = content.substr(colon_pos + 1, comma_pos - colon_pos - 1);
                // 공백 제거
                width_str.erase(remove_if(width_str.begin(), width_str.end(), isspace), width_str.end());
                video_config_.width = std::stoi(width_str);
            }

            size_t height_pos = content.find("\"height\"", video_pos);
            if (height_pos != std::string::npos) {
                size_t colon_pos = content.find(":", height_pos);
                size_t comma_pos = content.find(",", colon_pos);
                std::string height_str = content.substr(colon_pos + 1, comma_pos - colon_pos - 1);
                height_str.erase(remove_if(height_str.begin(), height_str.end(), isspace), height_str.end());
                video_config_.height = std::stoi(height_str);
            }

            size_t fps_pos = content.find("\"fps\"", video_pos);
            if (fps_pos != std::string::npos) {
                size_t colon_pos = content.find(":", fps_pos);
                size_t comma_pos = content.find(",", colon_pos);
                std::string fps_str = content.substr(colon_pos + 1, comma_pos - colon_pos - 1);
                fps_str.erase(remove_if(fps_str.begin(), fps_str.end(), isspace), fps_str.end());
                video_config_.fps = std::stoi(fps_str);
            }

            size_t pixel_format_pos = content.find("\"pixel_format\"", video_pos);
            if (pixel_format_pos != std::string::npos) {
                size_t colon_pos = content.find(":", pixel_format_pos);
                size_t quote1_pos = content.find("\"", colon_pos);
                size_t quote2_pos = content.find("\"", quote1_pos + 1);
                video_config_.pixel_format = content.substr(quote1_pos + 1, quote2_pos - quote1_pos - 1);
            }

            size_t buffer_count_pos = content.find("\"buffer_count\"", video_pos);
            if (buffer_count_pos != std::string::npos) {
                size_t colon_pos = content.find(":", buffer_count_pos);
                size_t end_pos = content.find_first_of(",}", colon_pos);
                std::string buffer_str = content.substr(colon_pos + 1, end_pos - colon_pos - 1);
                buffer_str.erase(remove_if(buffer_str.begin(), buffer_str.end(), isspace), buffer_str.end());
                video_config_.buffer_count = std::stoi(buffer_str);
            }
        }

        // rtsp 설정 파싱
        size_t rtsp_pos = content.find("\"rtsp\"");
        if (rtsp_pos != std::string::npos) {
            size_t port_pos = content.find("\"port\"", rtsp_pos);
            if (port_pos != std::string::npos) {
                size_t colon_pos = content.find(":", port_pos);
                size_t comma_pos = content.find(",", colon_pos);
                std::string port_str = content.substr(colon_pos + 1, comma_pos - colon_pos - 1);
                port_str.erase(remove_if(port_str.begin(), port_str.end(), isspace), port_str.end());
                rtsp_config_.port = std::stoi(port_str);
            }

            size_t mount_pos = content.find("\"mount_point\"", rtsp_pos);
            if (mount_pos != std::string::npos) {
                size_t colon_pos = content.find(":", mount_pos);
                size_t quote1_pos = content.find("\"", colon_pos);
                size_t quote2_pos = content.find("\"", quote1_pos + 1);
                rtsp_config_.mount_point = content.substr(quote1_pos + 1, quote2_pos - quote1_pos - 1);
            }

            size_t bitrate_pos = content.find("\"bitrate\"", rtsp_pos);
            if (bitrate_pos != std::string::npos) {
                size_t colon_pos = content.find(":", bitrate_pos);
                size_t comma_pos = content.find(",", colon_pos);
                std::string bitrate_str = content.substr(colon_pos + 1, comma_pos - colon_pos - 1);
                bitrate_str.erase(remove_if(bitrate_str.begin(), bitrate_str.end(), isspace), bitrate_str.end());
                rtsp_config_.bitrate = std::stoi(bitrate_str);
            }

            size_t encoder_pos = content.find("\"encoder\"", rtsp_pos);
            if (encoder_pos != std::string::npos) {
                size_t colon_pos = content.find(":", encoder_pos);
                size_t quote1_pos = content.find("\"", colon_pos);
                size_t quote2_pos = content.find("\"", quote1_pos + 1);
                rtsp_config_.encoder = content.substr(quote1_pos + 1, quote2_pos - quote1_pos - 1);
            }

            size_t pipeline_pos = content.find("\"pipeline\"", rtsp_pos);
            if (pipeline_pos != std::string::npos) {
                size_t colon_pos = content.find(":", pipeline_pos);
                size_t quote1_pos = content.find("\"", colon_pos);
                size_t quote2_pos = content.find("\"", quote1_pos + 1);
                rtsp_config_.pipeline = content.substr(quote1_pos + 1, quote2_pos - quote1_pos - 1);
            }
        }

        loaded_ = true;
        std::cout << "[INFO] Configuration loaded successfully from: " << config_file << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse config file: " << e.what() << std::endl;
        return false;
    }
}

void ConfigManager::printConfig() const {
    std::cout << "========== Configuration ==========" << std::endl;
    std::cout << "Video Config:" << std::endl;
    std::cout << "  Resolution: " << video_config_.width << "x" << video_config_.height << std::endl;
    std::cout << "  FPS: " << video_config_.fps << std::endl;
    std::cout << "  Pixel Format: " << video_config_.pixel_format << std::endl;
    std::cout << "  Buffer Count: " << video_config_.buffer_count << std::endl;
    
    std::cout << "RTSP Config:" << std::endl;
    std::cout << "  Port: " << rtsp_config_.port << std::endl;
    std::cout << "  Mount Point: " << rtsp_config_.mount_point << std::endl;
    std::cout << "  Bitrate: " << rtsp_config_.bitrate << std::endl;
    std::cout << "  Encoder: " << rtsp_config_.encoder << std::endl;
    std::cout << "  Pipeline: " << rtsp_config_.pipeline << std::endl;
    std::cout << "===================================" << std::endl;
}
