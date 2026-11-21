#include "VideoRecorder.hpp"

#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <utility>

#define REC_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Record", __VA_ARGS__)
#define REC_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Record", __VA_ARGS__)

namespace record {
struct VideoRecorder::State {
    AMediaCodec* codec = nullptr;
    AMediaMuxer* muxer = nullptr;
    int track = -1;
    bool muxerStarted = false;
    bool inputEos = false;
    int outputFd = -1;
};

static bool makeDirectories(std::string const& path) {
    size_t pos = 1;
    while ((pos = path.find('/', pos)) != std::string::npos) {
        auto part = path.substr(0, pos++);
        if (!part.empty() && mkdir(part.c_str(), 0775) != 0 && errno != EEXIST)
            return false;
    }
    return true;
}

VideoRecorder& VideoRecorder::get() {
    static VideoRecorder instance;
    return instance;
}

VideoRecorder::~VideoRecorder() {
    stop();
}

bool VideoRecorder::start(std::string const& path, int width, int height, int fps) {
    if (m_recording) {
        m_error = "Recording is already active";
        return false;
    }

    m_error.clear();
    width &= ~1;
    height &= ~1;
    if (width < 2 || height < 2 || fps < 1 || fps > 120) {
        m_error = "Invalid video dimensions or frame rate";
        return false;
    }

    if (!makeDirectories(path)) {
        m_error = "Cannot create output directory: " + path;
        return false;
    }

    m_state = new State();
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_yuv.resize(static_cast<size_t>(width) * height * 3 / 2);

    auto* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 21);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, std::min(width * height * fps, 24000000));
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 2);

    m_state->codec = AMediaCodec_createEncoderByType("video/avc");
    if (!m_state->codec) {
        AMediaFormat_delete(format);
        m_error = "H.264 encoder is unavailable on this device";
        closeEncoder();
        return false;
    }

    media_status_t status = AMediaCodec_configure(
        m_state->codec, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE
    );
    AMediaFormat_delete(format);
    if (status != AMEDIA_OK || AMediaCodec_start(m_state->codec) != AMEDIA_OK) {
        m_error = "Could not configure the H.264 encoder";
        closeEncoder();
        return false;
    }

    m_state->outputFd = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0664);
    if (m_state->outputFd < 0) {
        m_error = "Could not open MP4 output file: " + path;
        closeEncoder();
        return false;
    }

    m_state->muxer = AMediaMuxer_new(m_state->outputFd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    if (!m_state->muxer) {
        m_error = "Could not create MP4 output file";
        closeEncoder();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_frames.clear();
        m_stopping = false;
    }
    m_startTime = std::chrono::steady_clock::now();
    m_recording.store(true);
    m_worker = std::thread(&VideoRecorder::workerLoop, this);
    REC_LOGI("Video recording started: %dx%d @ %d", width, height, fps);
    return true;
}

void VideoRecorder::convertRGBA(uint8_t const* rgba, int sourceWidth, int sourceHeight) {
    auto* yPlane = m_yuv.data();
    auto* uvPlane = yPlane + static_cast<size_t>(m_width) * m_height;
    for (int y = 0; y < m_height; ++y) {
        int sy = (y * sourceHeight) / m_height;
        int row = (sourceHeight - 1 - sy) * sourceWidth * 4;
        for (int x = 0; x < m_width; ++x) {
            int sx = (x * sourceWidth) / m_width;
            auto p = rgba + row + sx * 4;
            int r = p[0], g = p[1], b = p[2];
            int yy = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yPlane[static_cast<size_t>(y) * m_width + x] =
                static_cast<uint8_t>(std::clamp(yy, 0, 255));

            if ((y & 1) == 0 && (x & 1) == 0) {
                int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                size_t uv = static_cast<size_t>(y / 2) * m_width + x;
                uvPlane[uv] = static_cast<uint8_t>(std::clamp(u, 0, 255));
                uvPlane[uv + 1] = static_cast<uint8_t>(std::clamp(v, 0, 255));
            }
        }
    }
}

void VideoRecorder::pushRGBA(uint8_t const* rgba, int width, int height) {
    if (!m_recording.load() || !rgba || width < 2 || height < 2)
        return;

    Frame frame;
    frame.width = width;
    frame.height = height;
    frame.ptsUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - m_startTime).count();
    frame.pixels.assign(rgba, rgba + static_cast<size_t>(width) * height * 4);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_stopping || m_frames.size() >= 2)
            return;
        m_frames.emplace_back(std::move(frame));
    }
    m_queueCondition.notify_one();
}

void VideoRecorder::encodeFrame(Frame const& frame) {
    if (!m_state || !m_state->codec)
        return;

    convertRGBA(frame.pixels.data(), frame.width, frame.height);
    ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(m_state->codec, 10000);
    if (inputIndex < 0) {
        m_error = "H.264 encoder did not provide an input buffer";
        return;
    }

    size_t capacity = 0;
    uint8_t* input = AMediaCodec_getInputBuffer(m_state->codec, inputIndex, &capacity);
    if (!input || capacity < m_yuv.size()) {
        AMediaCodec_queueInputBuffer(m_state->codec, inputIndex, 0, 0, frame.ptsUs, 0);
        m_error = "Encoder input buffer is too small";
        return;
    }

    std::memcpy(input, m_yuv.data(), m_yuv.size());
    AMediaCodec_queueInputBuffer(m_state->codec, inputIndex, 0, m_yuv.size(), frame.ptsUs, 0);
    drain(false);
}

void VideoRecorder::workerLoop() {
    for (;;) {
        Frame frame;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] {
                return m_stopping || !m_frames.empty();
            });
            if (m_frames.empty() && m_stopping)
                break;
            frame = std::move(m_frames.front());
            m_frames.pop_front();
        }
        encodeFrame(frame);
    }
    drain(true);
}

void VideoRecorder::drain(bool endOfStream) {
    if (!m_state || !m_state->codec || !m_state->muxer)
        return;

    if (endOfStream && !m_state->inputEos) {
        auto index = AMediaCodec_dequeueInputBuffer(m_state->codec, 10000);
        if (index >= 0) {
            AMediaCodec_queueInputBuffer(m_state->codec, index, 0, 0,
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - m_startTime).count(),
                AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
            m_state->inputEos = true;
        }
    }

    AMediaCodecBufferInfo info{};
    for (;;) {
        auto index = AMediaCodec_dequeueOutputBuffer(m_state->codec, &info, endOfStream ? 10000 : 0);
        if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
            break;
        if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            auto* outputFormat = AMediaCodec_getOutputFormat(m_state->codec);
            m_state->track = AMediaMuxer_addTrack(m_state->muxer, outputFormat);
            AMediaFormat_delete(outputFormat);
            if (m_state->track >= 0 && AMediaMuxer_start(m_state->muxer) == AMEDIA_OK)
                m_state->muxerStarted = true;
            else
                m_error = "Could not start MP4 muxer";
            continue;
        }
        if (index < 0)
            break;

        if (info.size > 0 && m_state->muxerStarted) {
            size_t capacity = 0;
            auto* output = AMediaCodec_getOutputBuffer(m_state->codec, index, &capacity);
            if (output && static_cast<size_t>(info.offset + info.size) <= capacity) {
                AMediaMuxer_writeSampleData(m_state->muxer, m_state->track, output, &info);
            }
        }

        bool eos = (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0;
        AMediaCodec_releaseOutputBuffer(m_state->codec, index, false);
        if (eos)
            break;
    }
}

void VideoRecorder::stop() {
    if (!m_state) {
        m_recording.store(false);
        return;
    }

    m_recording.store(false);
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopping = true;
    }
    m_queueCondition.notify_all();
    if (m_worker.joinable())
        m_worker.join();

    closeEncoder();
    m_yuv.clear();
    if (m_error.empty())
        REC_LOGI("Video recording stopped");
}

void VideoRecorder::closeEncoder() {
    if (!m_state)
        return;
    if (m_state->codec) {
        AMediaCodec_stop(m_state->codec);
        AMediaCodec_delete(m_state->codec);
    }
    if (m_state->muxer) {
        if (m_state->muxerStarted)
            AMediaMuxer_stop(m_state->muxer);
        AMediaMuxer_delete(m_state->muxer);
    }
    if (m_state->outputFd >= 0)
        ::close(m_state->outputFd);
    delete m_state;
    m_state = nullptr;
}

bool VideoRecorder::isRecording() const {
    return m_recording.load();
}

std::string const& VideoRecorder::error() const {
    return m_error;
}
}
