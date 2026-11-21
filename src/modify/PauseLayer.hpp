#pragma once
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include "../record/VideoRecorder.hpp"
#include "../record/FrameCapture.hpp"
#include "../record/AudioBridge.hpp"

#include <vector>
#include <algorithm>

using namespace geode::prelude;

class $modify(RecordPauseLayer, PauseLayer) {
    static void collectButtonRects(CCNode* root, CCNode* target, std::vector<CCRect>& out) {
        auto children = root->getChildren();
        if (!children) return;
        for (unsigned int i = 0; i < children->count(); ++i) {
            auto child = static_cast<CCNode*>(children->objectAtIndex(i));
            if (!child || !child->isVisible()) continue;

            if (dynamic_cast<CCMenuItem*>(child) && child->getParent()) {
                CCRect b = child->boundingBox();
                CCPoint p1 = target->convertToNodeSpace(child->getParent()->convertToWorldSpace(b.origin));
                CCPoint p2 = target->convertToNodeSpace(child->getParent()->convertToWorldSpace(
                    ccp(b.getMaxX(), b.getMaxY())));
                out.emplace_back(std::min(p1.x, p2.x), std::min(p1.y, p2.y),
                                 std::abs(p2.x - p1.x), std::abs(p2.y - p1.y));
            }
            collectButtonRects(child, target, out);
        }
    }

    void customSetup() {
        PauseLayer::customSetup();

        auto menu = CCMenu::create();
        menu->setID("record-control-menu"_spr);
        menu->setPosition({0.f, 0.f});

        auto icon = CCLayerColor::create(ccc4(230, 48, 55, 255), 30.f, 30.f);
        auto button = CCMenuItemSpriteExtra::create(
            icon, this, menu_selector(RecordPauseLayer::onRecordToggle)
        );
        button->setID("record-toggle"_spr);
        menu->addChild(button);

        auto win = CCDirector::sharedDirector()->getWinSize();
        std::vector<CCRect> occupied;
        collectButtonRects(this, this, occupied);

        const float w = win.width;
        const float h = win.height;
        const float margin = 24.f;
        std::vector<CCPoint> candidates = {
            ccp(w * 0.50f, margin + 22.f),
            ccp(w * 0.25f, margin + 22.f),
            ccp(w * 0.75f, margin + 22.f),
            ccp(margin + 22.f, h * 0.50f),
            ccp(w - margin - 22.f, h * 0.50f),
            ccp(w * 0.50f, h - margin - 22.f),
            ccp(w * 0.25f, h - margin - 22.f),
            ccp(w * 0.75f, h - margin - 22.f)
        };

        CCPoint chosen = candidates.front();
        float bestOverlap = 1e30f;
        for (auto const& point : candidates) {
            CCRect candidate = CCRectMake(point.x - 19.f, point.y - 19.f, 38.f, 38.f);
            float overlap = 0.f;
            for (auto const& rect : occupied) {
                float x = std::max(0.f, std::min(candidate.getMaxX(), rect.getMaxX()) -
                                         std::max(candidate.getMinX(), rect.getMinX()));
                float y = std::max(0.f, std::min(candidate.getMaxY(), rect.getMaxY()) -
                                         std::max(candidate.getMinY(), rect.getMinY()));
                overlap += x * y;
            }
            if (overlap < bestOverlap) {
                bestOverlap = overlap;
                chosen = point;
            }
            if (overlap == 0.f) break;
        }

        menu->setPosition(chosen);
        this->addChild(menu, 1000);
    }

    void onRecordToggle(CCObject*) {
        auto& recorder = record::VideoRecorder::get();
        if (recorder.isRecording()) {
            recorder.stop();
            FLAlertLayer::create("Record", "Recording saved.", "OK")->show();
            return;
        }

        int sourceWidth = 0;
        int sourceHeight = 0;
        record::getFramebufferSize(sourceWidth, sourceHeight);
        int width = sourceWidth;
        int height = std::max(2, (sourceHeight * width / std::max(1, sourceWidth)) & ~1);
        width &= ~1;

        std::string path =
            "/storage/emulated/0/Android/media/com.geode.launcher/game/renders/Record.mp4";

        record::prepareFramebufferCapture();
        if (!recorder.start(path, width, height, 60)) {
            FLAlertLayer::create("Record", recorder.error(), "OK")->show();
        } else {
            FLAlertLayer::create("Record",
                recordaudio_bridge::available() ? "Recording is initializing at full resolution, up to 60 FPS. Game audio is saved to a separate WAV file." : "Recording is initializing at full resolution, up to 60 FPS. Install RecordAudio to capture game audio.",
                "OK")->show();
        }
    }
};
