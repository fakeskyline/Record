#pragma once
#include <cstdint>
#include <vector>

namespace record {
void getFramebufferSize(int& width, int& height);
void prepareFramebufferCapture();
void captureCurrentFramebuffer();
}
