#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "MacroManager.hpp"

using namespace geode::prelude;

// ── Input hooks ───────────────────────────────────────────────────────────────

class $modify(GJBaseGameLayer) {
    void handleButton(bool push, int button, bool player1) {
        MacroManager::get().onInput(button, !player1, push);
        GJBaseGameLayer::handleButton(push, button, player1);
    }

    void processCommands(float dt) {
        GJBaseGameLayer::processCommands(dt);
        MacroManager::get().onFrameAdvance(this);
    }
};

// ── Reset on death ────────────────────────────────────────────────────────────

class $modify(PlayLayer) {
    void resetLevel() {
        auto& bot = MacroManager::get();
        if (bot.state == BotState::Playing) {
            bot.currentFrame = 0;
            bot.playbackIndex = 0;
        } else if (bot.state == BotState::Recording) {
            bot.startRecording();
        }
        PlayLayer::resetLevel();
    }

    void onQuit() {
        MacroManager::get().stopPlayback();
        MacroManager::get().stopRecording();
        PlayLayer::onQuit();
    }
};

// ── Pause menu UI ─────────────────────────────────────────────────────────────

class $modify(YallBotPauseLayer, PauseLayer) {
    struct Fields {
        CCLabelBMFont* statusLabel = nullptr;
    };

    bool init(bool unfocused) {
        if (!PauseLayer::init(unfocused)) return false;

        if (!Mod::get()->getSettingValue<bool>("show-ui-button")) return true;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // ── Background panel ─────────────────────────────────────────────────
        auto bg = CCScale9Sprite::create("square02_small.png");
        bg->setContentSize({ 180.f, 80.f });
        bg->setOpacity(180);
        bg->setColor({ 0, 0, 0 });
        bg->setPosition({ winSize.width / 2, 55.f });
        bg->setZOrder(10);
        this->addChild(bg);

        // ── Title ────────────────────────────────────────────────────────────
        auto title = CCLabelBMFont::create("YallBot.", "goldFont.fnt");
        title->setScale(0.55f);
        title->setPosition({ winSize.width / 2, 84.f });
        title->setZOrder(11);
        this->addChild(title);

        // ── Status label ─────────────────────────────────────────────────────
        m_fields->statusLabel = CCLabelBMFont::create(
            this->getStatusText().c_str(), "chatFont.fnt"
        );
        m_fields->statusLabel->setScale(0.5f);
        m_fields->statusLabel->setPosition({ winSize.width / 2, 63.f });
        m_fields->statusLabel->setZOrder(11);
        this->addChild(m_fields->statusLabel);

        // ── Buttons menu ─────────────────────────────────────────────────────
        auto menu = CCMenu::create();
        menu->setPosition({ winSize.width / 2, 40.f });
        menu->setZOrder(11);
        this->addChild(menu);

        // Record / Play / Stop button
        auto actionLbl = CCLabelBMFont::create(
            this->getActionText().c_str(), "bigFont.fnt"
        );
        actionLbl->setScale(0.4f);
        auto actionBtn = CCMenuItemLabel::create(
            actionLbl, this,
            menu_selector(YallBotPauseLayer::onToggle)
        );

        // Clear button
        auto clearLbl = CCLabelBMFont::create("Clear", "bigFont.fnt");
        clearLbl->setScale(0.4f);
        auto clearBtn = CCMenuItemLabel::create(
            clearLbl, this,
            menu_selector(YallBotPauseLayer::onClear)
        );
        clearBtn->setColor({ 255, 100, 100 });

        menu->addChild(actionBtn);
        menu->addChild(clearBtn);
        menu->alignItemsHorizontallyWithPadding(12.f);

        return true;
    }

    std::string getStatusText() {
        auto& bot = MacroManager::get();
        switch (bot.state) {
            case BotState::Idle:
                return "Idle | Inputs: " + std::to_string(bot.inputs.size());
            case BotState::Recording:
                return "REC | Frame: " + std::to_string(bot.currentFrame);
            case BotState::Playing:
                return "PLAY | Frame: " + std::to_string(bot.currentFrame);
        }
        return "Idle";
    }

    std::string getActionText() {
        auto& bot = MacroManager::get();
        switch (bot.state) {
            case BotState::Idle:
                return bot.inputs.empty() ? "Record" : "Play";
            case BotState::Recording:
                return "Stop Rec";
            case BotState::Playing:
                return "Stop Play";
        }
        return "Record";
    }

    void onToggle(CCObject*) {
        auto& bot = MacroManager::get();
        switch (bot.state) {
            case BotState::Idle:
                if (!bot.inputs.empty()) bot.startPlayback();
                else bot.startRecording();
                break;
            case BotState::Recording:
                bot.stopRecording();
                bot.saveMacro("macro_" + std::to_string(bot.inputs.size()));
                break;
            case BotState::Playing:
                bot.stopPlayback();
                break;
        }
        if (m_fields->statusLabel) {
            m_fields->statusLabel->setString(this->getStatusText().c_str());
        }
    }

    void onClear(CCObject*) {
        MacroManager::get().clearMacro();
        if (m_fields->statusLabel) {
            m_fields->statusLabel->setString(this->getStatusText().c_str());
        }
    }
};
