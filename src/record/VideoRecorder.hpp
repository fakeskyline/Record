#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
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

    struct State;
    struct Frame {
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
        int64_t ptsUs = 0;
    };

    bool initializeEncoder(std::string const& path);
    void closeEncoder();
    void drain(bool endOfStream);
    void convertRGBA(uint8_t const* rgba, int sourceWidth, int sourceHeight);
    void encodeFrame(Frame const& frame);
    void workerLoop();

    State* m_state = nullptr;
    std::string m_error;
    std::vector<uint8_t> m_yuv;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::deque<Frame> m_frames;
    std::thread m_worker;
    std::chrono::steady_clock::time_point m_startTime;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 30;
    std::atomic_bool m_recording{false};
    std::atomic_bool m_ready{false};
    std::string m_outputPath;
    bool m_stopping = false;
};
}
