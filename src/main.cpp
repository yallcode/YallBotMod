#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "MacroManager.hpp"

using namespace geode::prelude;

// ── Hook physics step to advance the macro frame counter ──────────────────────

class $modify(GJBaseGameLayer) {
    void processCommands(float dt) {
        GJBaseGameLayer::processCommands(dt);
        MacroManager::get().onFrameAdvance(this);
    }

    // Intercept button presses for recording
    void pushButton(int button, bool player1) {
        MacroManager::get().onInput(button, !player1, true);
        GJBaseGameLayer::pushButton(button, player1);
    }

    void releaseButton(int button, bool player1) {
        MacroManager::get().onInput(button, !player1, false);
        GJBaseGameLayer::releaseButton(button, player1);
    }
};

// ── Reset macro state when a level starts/restarts ────────────────────────────

class $modify(PlayLayer) {
    void resetLevel() {
        auto& bot = MacroManager::get();
        if (bot.state == BotState::Playing) {
            // Reset playback to beginning on death/restart
            bot.currentFrame = 0;
            bot.playbackIndex = 0;
        } else if (bot.state == BotState::Recording) {
            bot.startRecording(); // Clear and restart recording
        }
        PlayLayer::resetLevel();
    }

    void onQuit() {
        MacroManager::get().stopPlayback();
        MacroManager::get().stopRecording();
        PlayLayer::onQuit();
    }
};

// ── Add YallBot button to the pause menu ──────────────────────────────────────

class $modify(PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        if (!Mod::get()->getSettingValue<bool>("show-ui-button")) return;

        auto& bot = MacroManager::get();

        // Create the YallBot button panel
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Simple label button at the bottom
        auto label = CCLabelBMFont::create("YallBot.", "bigFont.fnt");
        label->setScale(0.5f);

        auto btn = CCMenuItemLabel::create(label, this,
            menu_selector(PauseLayerModify::onYallBotMenu));

        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition(winSize.width / 2, 20.f);
        this->addChild(menu, 10);
    }

    void onYallBotMenu(CCObject*) {
        auto& bot = MacroManager::get();
        auto alert = FLAlertLayer::create(nullptr, "YallBot.", this->getBotStatusText(),
            "Close", "Toggle", 300.f);
        alert->m_button2->setUserObject(this);
        alert->show();
    }

    const char* getBotStatusText() {
        auto& bot = MacroManager::get();
        std::string txt;

        switch (bot.state) {
            case BotState::Idle:
                txt = "<cr>Status: Idle</c>\n\n";
                txt += "Inputs in memory: " + std::to_string(bot.inputs.size()) + "\n\n";
                txt += "Press <cy>Toggle</c> to start <cg>Recording</c>.";
                break;
            case BotState::Recording:
                txt = "<cg>Status: Recording...</c>\n\n";
                txt += "Frame: " + std::to_string(bot.currentFrame) + "\n";
                txt += "Inputs: " + std::to_string(bot.inputs.size()) + "\n\n";
                txt += "Press <cy>Toggle</c> to <cr>Stop Recording</c>.";
                break;
            case BotState::Playing:
                txt = "<cy>Status: Playing back...</c>\n\n";
                txt += "Frame: " + std::to_string(bot.currentFrame) + " / ";
                txt += bot.inputs.empty() ? "0" : std::to_string(bot.inputs.back().frame);
                txt += "\n\nPress <cy>Toggle</c> to <cr>Stop Playback</c>.";
                break;
        }

        // Geode's alert takes const char* — use a static so pointer stays valid
        static std::string s;
        s = txt;
        return s.c_str();
    }

    void FLAlert_Clicked(FLAlertLayer* alert, bool btn2) {
        if (!btn2) return;

        auto& bot = MacroManager::get();
        switch (bot.state) {
            case BotState::Idle:
                // If we have a macro, start playback; otherwise start recording
                if (!bot.inputs.empty()) {
                    bot.startPlayback();
                } else {
                    bot.startRecording();
                }
                break;
            case BotState::Recording:
                bot.stopRecording();
                // Auto-save with a timestamp name
                bot.saveMacro("macro_" + std::to_string(bot.inputs.size()));
                break;
            case BotState::Playing:
                bot.stopPlayback();
                break;
        }
    }
};
