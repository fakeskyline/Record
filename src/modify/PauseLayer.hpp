#pragma once
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "../record/VideoRecorder.hpp"

using namespace geode::prelude;

class $modify(RecordPauseLayer, PauseLayer) {
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
        menu->setPosition({CCDirector::sharedDirector()->getWinSize().width - 35.f,
                           CCDirector::sharedDirector()->getWinSize().height - 38.f});
        this->addChild(menu, 1000);
    }

    void onRecordToggle(CCObject*) {
        auto& recorder = record::VideoRecorder::get();
        if (recorder.isRecording()) {
            recorder.stop();
            FLAlertLayer::create("Record", "Recording saved.", "OK")->show();
            return;
        }

        auto size = CCDirector::sharedDirector()->getWinSizeInPixels();
        int width = static_cast<int>(size.width);
        int height = static_cast<int>(size.height);
        std::string path =
            "/storage/emulated/0/Android/media/com.geode.launcher/game/renders/Record.mp4";

        if (!recorder.start(path, width, height, 60)) {
            FLAlertLayer::create("Record", recorder.error(), "OK")->show();
        } else {
            FLAlertLayer::create("Record",
                "Recording started. Resume the level to capture gameplay; open pause and tap the red icon to stop.",
                "OK")->show();
        }
    }
};
