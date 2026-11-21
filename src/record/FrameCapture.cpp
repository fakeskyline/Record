#include "FrameCapture.hpp"
#include "VideoRecorder.hpp"

#include <GLES2/gl2.h>
#include <Geode/Geode.hpp>
#include <algorithm>

namespace record {
void captureCurrentFramebuffer() {
    auto& recorder = VideoRecorder::get();
    if (!recorder.isRecording())
        return;

    auto size = cocos2d::CCDirector::sharedDirector()->getWinSizeInPixels();
    int width = static_cast<int>(size.width);
    int height = static_cast<int>(size.height);
    if (width < 2 || height < 2)
        return;

    static std::vector<uint8_t> pixels;
    pixels.resize(static_cast<size_t>(width) * height * 4);

    GLint previousPack = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, previousPack);

    if (glGetError() == GL_NO_ERROR)
        recorder.pushRGBA(pixels.data(), width, height);
}
}
