#include "FrameCapture.hpp"
#include "VideoRecorder.hpp"

#include <GLES2/gl2.h>
#include <Geode/Geode.hpp>
#include <algorithm>
#include <chrono>

namespace record {
static std::vector<uint8_t> s_pixels;
static std::chrono::steady_clock::time_point s_lastCapture{};

void getFramebufferSize(int& width, int& height) {
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    width = viewport[2];
    height = viewport[3];
}

void prepareFramebufferCapture() {
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];
    if (width < 2 || height < 2) return;
    s_pixels.resize(static_cast<size_t>(width) * height * 4);

    s_lastCapture = {};
}

void captureCurrentFramebuffer() {
    auto& recorder = VideoRecorder::get();
    if (!recorder.isReady()) return;

    auto now = std::chrono::steady_clock::now();
    if (s_lastCapture.time_since_epoch().count() != 0 &&
        now - s_lastCapture < std::chrono::microseconds(16667))
        return;
    s_lastCapture = now;

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    int x = viewport[0];
    int y = viewport[1];
    int width = viewport[2];
    int height = viewport[3];
    if (width < 2 || height < 2) return;

    size_t required = static_cast<size_t>(width) * height * 4;
    if (s_pixels.size() != required) s_pixels.resize(required);

    GLint previousPack = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, s_pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, previousPack);

    if (glGetError() == GL_NO_ERROR)
        recorder.pushRGBA(s_pixels.data(), width, height);
}
}
