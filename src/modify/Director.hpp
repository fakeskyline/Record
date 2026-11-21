#pragma once
#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include "../record/FrameCapture.hpp"

using namespace geode::prelude;

class $modify(RecordDirector, cocos2d::CCDirector) {
    void drawScene() {
        cocos2d::CCDirector::drawScene();
        record::captureCurrentFramebuffer();
    }
};
