#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace record {
class VideoRecorder {
public:
    static VideoRecorder& get();
    bool start(std::string const& path, int width, int height, int fps = 60);
    void pushRGBA(uint8_t const* rgba, int width, int height);
    void stop();
    bool isRecording() const;
    std::string const& error() const;

private:
    VideoRecorder() = default;
    ~VideoRecorder();
    VideoRecorder(VideoRecorder const&) = delete;
    VideoRecorder& operator=(VideoRecorder const&) = delete;

    bool openEncoder(int width, int height, int fps);
    void closeEncoder();
    void drain(bool endOfStream);
    void convertRGBA(uint8_t const* rgba, int sourceWidth, int sourceHeight);

    struct State;
    State* m_state = nullptr;
    std::string m_error;
    std::vector<uint8_t> m_yuv;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 60;
    int64_t m_frameIndex = 0;
    bool m_recording = false;
};
}
