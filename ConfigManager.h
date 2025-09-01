#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <memory>

struct VideoConfig {
    int width;
    int height;
    int fps;
    std::string pixel_format;
    int buffer_count;
};

struct RtspConfig {
    int port;
    std::string mount_point;
    int bitrate;
    std::string encoder;
    std::string pipeline;
};

class ConfigManager {
private:
    VideoConfig video_config_;
    RtspConfig rtsp_config_;
    bool loaded_;

public:
    ConfigManager();
    ~ConfigManager() = default;

    bool loadFromFile(const std::string& config_file);
    
    const VideoConfig& getVideoConfig() const { return video_config_; }
    const RtspConfig& getRtspConfig() const { return rtsp_config_; }
    
    bool isLoaded() const { return loaded_; }
    
    void printConfig() const;
};

#endif // CONFIG_MANAGER_H
