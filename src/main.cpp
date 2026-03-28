#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include "MacroManager.hpp"

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// YallBotPopup
//
// This is the main UI window. It extends geode::Popup which gives us:
//   - A brown rounded background (m_mainLayer, 310×260 pixels)
//   - A title at the top
//   - An X close button at top-left
//
// COORDINATE SYSTEM inside m_mainLayer:
//   (0,0) = bottom-left corner
//   (310,260) = top-right corner
//   (155,130) = center
//
// HOW THE BOT WORKS:
//   Record  → every button press/release is saved with its frame number
//   Playback → on each physics frame, replays any saved inputs for that frame
//   Disabled → nothing is recorded or replayed
//
// WORKFLOW:
//   1. Enter a level
//   2. Pause → tap YallBot. button
//   3. Click Record → unpause → play the level → pause again
//   4. Click Disabled to stop recording
//   5. Click Save to save the macro to disk with a name
//   6. Next time: click Load to load it back, then click Playback
//   7. Unpause → the bot plays perfectly on its own
// ─────────────────────────────────────────────────────────────────────────────

class YallBotPopup : public geode::Popup {
protected:
    // UI elements we need to reference later (to update their text/state)
    CCLabelBMFont* m_replayLabel   = nullptr; // shows current replay file name
    CCLabelBMFont* m_disabledLbl   = nullptr; // "Disabled" mode button label
    CCLabelBMFont* m_recordLbl     = nullptr; // "Record" mode button label
    CCLabelBMFont* m_playbackLbl   = nullptr; // "Playback" mode button label
    CCMenuItemToggler* m_ignoreToggle = nullptr; // checkbox for ignore inputs
    int m_replayIndex = 0; // which replay file is currently selected

    bool init() {
        // Initialize the base Popup (310 wide, 260 tall)
        // This creates m_mainLayer and the close button automatically
        if (!Popup::init(310.f, 260.f)) return false;
        this->setTitle("YallBot.");

        // ── ROW 1: Mode buttons (y=210) ───────────────────────────────────
        // These three buttons switch the bot between Disabled/Record/Playback
        // They are just labels that change color based on the current state:
        //   White  = currently active mode
        //   Grey   = inactive mode
        //   Green  = recording active
        //   Yellow = playback active

        m_disabledLbl = CCLabelBMFont::create("Disabled", "bigFont.fnt");
        m_disabledLbl->setScale(0.38f);

        m_recordLbl = CCLabelBMFont::create("Record", "bigFont.fnt");
        m_recordLbl->setScale(0.38f);

        m_playbackLbl = CCLabelBMFont::create("Playback", "bigFont.fnt");
        m_playbackLbl->setScale(0.38f);

        auto disabledBtn  = CCMenuItemLabel::create(m_disabledLbl,  this, menu_selector(YallBotPopup::onDisabled));
        auto recordBtn    = CCMenuItemLabel::create(m_recordLbl,    this, menu_selector(YallBotPopup::onRecord));
        auto playbackBtn  = CCMenuItemLabel::create(m_playbackLbl,  this, menu_selector(YallBotPopup::onPlayback));

        auto modeMenu = CCMenu::create();
        modeMenu->addChild(disabledBtn);
        modeMenu->addChild(recordBtn);
        modeMenu->addChild(playbackBtn);
        modeMenu->alignItemsHorizontallyWithPadding(16.f);
        // Position at center-x, near top of popup content area
        modeMenu->setPosition({ 155.f, 213.f });
        m_mainLayer->addChild(modeMenu);

        // Set initial button colors based on current bot state
        this->updateModeColors();

        // ── ROW 2: Replay file picker (y=178) ────────────────────────────
        // Shows the name of the selected .ybot file on disk
        // Left/right arrow buttons cycle through all saved replays
        // Files are stored in: <geode save dir>/mods/yallcode.yallbot/macros/

        auto replaysTitleLbl = CCLabelBMFont::create("Replays", "bigFont.fnt");
        replaysTitleLbl->setScale(0.38f);
        // Anchor at left so it doesn't overflow left side
        replaysTitleLbl->setAnchorPoint({ 0.f, 0.5f });
        replaysTitleLbl->setPosition({ 22.f, 178.f });
        m_mainLayer->addChild(replaysTitleLbl);

        // Dark rounded box behind the replay name + arrows
        auto navBg = CCScale9Sprite::create("square02_small.png");
        navBg->setContentSize({ 152.f, 26.f });
        navBg->setOpacity(100);
        navBg->setColor({ 0, 0, 0 });
        navBg->setPosition({ 224.f, 178.f });
        m_mainLayer->addChild(navBg);

        // Left arrow, replay name label, right arrow — all in one CCMenu
        auto navMenu = CCMenu::create();
        navMenu->setPosition({ 224.f, 178.f });
        m_mainLayer->addChild(navMenu);

        auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        leftSpr->setScale(0.38f);
        auto leftBtn = CCMenuItemSpriteExtra::create(
            leftSpr, this, menu_selector(YallBotPopup::onPrev)
        );

        // The replay name in the middle — clicking it does nothing (nullptr selector)
        m_replayLabel = CCLabelBMFont::create(
            this->currentReplayName().c_str(), "chatFont.fnt"
        );
        m_replayLabel->setScale(0.48f);
        m_replayLabel->limitLabelWidth(88.f, 0.48f, 0.1f);
        auto replayLabelItem = CCMenuItemLabel::create(m_replayLabel, this, nullptr);

        auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        rightSpr->setFlipX(true);
        rightSpr->setScale(0.38f);
        auto rightBtn = CCMenuItemSpriteExtra::create(
            rightSpr, this, menu_selector(YallBotPopup::onNext)
        );

        navMenu->addChild(leftBtn);
        navMenu->addChild(replayLabelItem);
        navMenu->addChild(rightBtn);
        navMenu->alignItemsHorizontallyWithPadding(3.f);

        // ── ROW 3: Ignore Inputs toggle (y=148) ──────────────────────────
        // When ON (checked): your own taps are ignored during Playback
        //   → the bot plays perfectly without your accidental inputs
        // When OFF: your taps still register during Playback
        //   → useful if you want to assist the bot manually

        auto ignoreMenu = CCMenu::create();
        ignoreMenu->setPosition({ 130.f, 148.f });
        m_mainLayer->addChild(ignoreMenu);

        m_ignoreToggle = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(YallBotPopup::onIgnoreInputs), 0.6f
        );
        // Set checkbox to match current ignoreInputs setting
        m_ignoreToggle->toggle(MacroManager::get().ignoreInputs);

        auto ignoreLbl = CCLabelBMFont::create("Ignore Inputs", "bigFont.fnt");
        ignoreLbl->setScale(0.36f);
        // Clicking the label also toggles the checkbox
        auto ignoreLblItem = CCMenuItemLabel::create(
            ignoreLbl, this, menu_selector(YallBotPopup::onIgnoreInputsLabel)
        );

        ignoreMenu->addChild(m_ignoreToggle);
        ignoreMenu->addChild(ignoreLblItem);
        ignoreMenu->alignItemsHorizontallyWithPadding(4.f);

        // Small info (i) button — tapping it explains what Ignore Inputs does
        auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
        infoSpr->setScale(0.45f);
        auto infoBtn = CCMenuItemSpriteExtra::create(
            infoSpr, this, menu_selector(YallBotPopup::onIgnoreInfo)
        );
        auto infoMenu = CCMenu::create();
        infoMenu->addChild(infoBtn);
        infoMenu->setPosition({ 283.f, 148.f });
        m_mainLayer->addChild(infoMenu);

        // ── Thin divider line ─────────────────────────────────────────────
        auto divider = CCLayerColor::create({ 255, 255, 255, 40 }, 266.f, 1.f);
        divider->setPosition({ 22.f, 128.f });
        m_mainLayer->addChild(divider);

        // ── ROW 4: Action buttons (y=100) ────────────────────────────────
        // New    → clears the current macro from memory (does NOT delete from disk)
        // Save   → saves the current recorded macro to a .ybot file on disk
        // Load   → loads the selected replay file from disk into memory
        // Delete → permanently deletes the selected .ybot file from disk

        auto btnMenu = CCMenu::create();
        btnMenu->setPosition({ 155.f, 100.f });
        m_mainLayer->addChild(btnMenu);

        btnMenu->addChild(this->makeBtn("New",    menu_selector(YallBotPopup::onNew)));
        btnMenu->addChild(this->makeBtn("Save",   menu_selector(YallBotPopup::onSave)));
        btnMenu->addChild(this->makeBtn("Load",   menu_selector(YallBotPopup::onLoad)));
        btnMenu->addChild(this->makeBtn("Delete", menu_selector(YallBotPopup::onDelete)));
        btnMenu->alignItemsHorizontallyWithPadding(5.f);

        return true;
    }

    // Creates a rounded grey action button with a text label
    CCMenuItemSpriteExtra* makeBtn(const char* text, SEL_MenuHandler sel) {
        auto bg = CCScale9Sprite::create("GJ_button_04.png");
        bg->setContentSize({ 62.f, 26.f });
        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
        lbl->setScale(0.34f);
        lbl->setPosition(bg->getContentSize() / 2);
        bg->addChild(lbl);
        return CCMenuItemSpriteExtra::create(bg, this, sel);
    }

    // Returns the name of the currently selected replay file,
    // or "no replays" if the macros folder is empty
    std::string currentReplayName() {
        auto list = MacroManager::get().getReplayList();
        if (list.empty()) return "no replays";
        if (m_replayIndex >= (int)list.size()) m_replayIndex = 0;
        return list[m_replayIndex];
    }

    // Recolors the three mode buttons to show which one is active
    void updateModeColors() {
        auto& bot = MacroManager::get();
        ccColor3B grey   = { 130, 130, 130 }; // inactive
        ccColor3B white  = { 255, 255, 255 }; // disabled/idle active
        ccColor3B green  = {  80, 255,  80 }; // recording active
        ccColor3B yellow = { 255, 210,  40 }; // playback active

        m_disabledLbl->setColor (bot.state == BotState::Idle      ? white  : grey);
        m_recordLbl->setColor   (bot.state == BotState::Recording  ? green  : grey);
        m_playbackLbl->setColor (bot.state == BotState::Playing    ? yellow : grey);
    }

    // ── Mode button callbacks ─────────────────────────────────────────────────

    // Disabled: stops recording/playback, bot does nothing
    void onDisabled(CCObject*) {
        MacroManager::get().stopRecording();
        MacroManager::get().stopPlayback();
        this->updateModeColors();
    }

    // Record: starts recording — every input from now on is saved to memory
    // Note: closes automatically when you unpause and play
    // Recording resets automatically on death/restart
    void onRecord(CCObject*) {
        MacroManager::get().startRecording();
        this->updateModeColors();
        FLAlertLayer::create(
            "YallBot.",
            "Recording started!\nClose this and play.\nPause and click <cy>Disabled</c> to stop.",
            "OK"
        )->show();
    }

    // Playback: replays the macro currently in memory
    // You must Load a replay first, or have just recorded one
    void onPlayback(CCObject*) {
        auto& bot = MacroManager::get();
        if (bot.inputs.empty()) {
            // Can't play back nothing — tell the user to load first
            FLAlertLayer::create(
                "YallBot.",
                "No macro in memory!\nTap <cy>Load</c> to load a saved replay first.",
                "OK"
            )->show();
            return;
        }
        bot.startPlayback();
        this->updateModeColors();
        FLAlertLayer::create(
            "YallBot.",
            "Playback ready!\nClose and unpause.\nThe bot will play automatically.",
            "OK"
        )->show();
    }

    // ── Replay navigator callbacks ────────────────────────────────────────────

    // Go to previous replay file in the list (wraps around)
    void onPrev(CCObject*) {
        auto list = MacroManager::get().getReplayList();
        if (list.empty()) return;
        m_replayIndex = (m_replayIndex - 1 + (int)list.size()) % (int)list.size();
        m_replayLabel->setString(list[m_replayIndex].c_str());
    }

    // Go to next replay file in the list (wraps around)
    void onNext(CCObject*) {
        auto list = MacroManager::get().getReplayList();
        if (list.empty()) return;
        m_replayIndex = (m_replayIndex + 1) % (int)list.size();
        m_replayLabel->setString(list[m_replayIndex].c_str());
    }

    // ── Ignore Inputs callbacks ───────────────────────────────────────────────

    // Toggling the checkbox directly
    void onIgnoreInputs(CCObject*) {
        MacroManager::get().ignoreInputs = !MacroManager::get().ignoreInputs;
    }

    // Clicking the label text also toggles the checkbox
    void onIgnoreInputsLabel(CCObject*) {
        MacroManager::get().ignoreInputs = !MacroManager::get().ignoreInputs;
        m_ignoreToggle->toggle(MacroManager::get().ignoreInputs);
    }

    // Shows a popup explaining what Ignore Inputs does
    void onIgnoreInfo(CCObject*) {
        FLAlertLayer::create(
            "Ignore Inputs",
            "Prevents your inputs from being\nregistered when replaying.",
            "OK"
        )->show();
    }

    // ── Action button callbacks ───────────────────────────────────────────────

    // New: clears the macro from memory
    // Use this before recording a fresh run
    // Does NOT delete any files from disk
    void onNew(CCObject*) {
        MacroManager::get().clearMacro();
        FLAlertLayer::create(
            "YallBot.",
            "Macro cleared from memory.\nReady to record a new run!",
            "OK"
        )->show();
    }

    // Save: writes the current in-memory macro to a .ybot file
    // The file is named by how many inputs it has, e.g. macro_312.ybot
    // You must have recorded something first
    void onSave(CCObject*) {
        auto& bot = MacroManager::get();
        if (bot.inputs.empty()) {
            FLAlertLayer::create("YallBot.", "Nothing to save!\nRecord a run first.", "OK")->show();
            return;
        }
        std::string name = "macro_" + std::to_string(bot.inputs.size());
        bot.saveMacro(name);
        // Refresh the replay label in case this is a new file
        if (m_replayLabel)
            m_replayLabel->setString(this->currentReplayName().c_str());
        FLAlertLayer::create("YallBot.", ("Saved as: " + name + ".ybot").c_str(), "OK")->show();
    }

    // Load: reads the selected .ybot file from disk into memory
    // After loading, switch to Playback mode and unpause
    void onLoad(CCObject*) {
        auto list = MacroManager::get().getReplayList();
        if (list.empty()) {
            FLAlertLayer::create(
                "YallBot.",
                "No replays found!\nRecord and Save a run first.",
                "OK"
            )->show();
            return;
        }
        MacroManager::get().loadMacro(list[m_replayIndex]);
        FLAlertLayer::create(
            "YallBot.",
            ("Loaded: " + list[m_replayIndex] + "\n\nNow click <cy>Playback</c> to play it!").c_str(),
            "OK"
        )->show();
    }

    // Delete: permanently removes the selected .ybot file from disk
    void onDelete(CCObject*) {
        auto list = MacroManager::get().getReplayList();
        if (list.empty()) {
            FLAlertLayer::create("YallBot.", "No replays to delete!", "OK")->show();
            return;
        }
        std::string name = list[m_replayIndex];
        MacroManager::get().deleteMacro(name);
        m_replayIndex = 0;
        if (m_replayLabel)
            m_replayLabel->setString(this->currentReplayName().c_str());
        FLAlertLayer::create("YallBot.", ("Deleted: " + name).c_str(), "OK")->show();
    }

public:
    static YallBotPopup* create() {
        auto ret = new YallBotPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// GJBaseGameLayer hooks
//
// GJBaseGameLayer is the base class for PlayLayer (normal levels)
// We hook two functions here:
//
// handleButton: called every time the player presses OR releases a jump button
//   - button 1 = left side tap (or Space/Up arrow on PC)
//   - button 2 = right side tap (2-player mode)
//   - player1 = true means Player 1, false = Player 2
//
// processCommands: called every physics step (240 times/sec in GD 2.2)
//   - This is where we advance the frame counter
//   - And replay any inputs saved for the current frame
// ─────────────────────────────────────────────────────────────────────────────

class $modify(GJBaseGameLayer) {
    void handleButton(bool push, int button, bool player1) {
        auto& bot = MacroManager::get();

        // During playback with Ignore Inputs ON:
        // skip recording the player's tap but still let GD process it
        // (GD needs to receive the event for internal state, but the bot
        //  will override with its own inputs via processCommands)
        if (bot.state == BotState::Playing && bot.ignoreInputs) {
            GJBaseGameLayer::handleButton(push, button, player1);
            return;
        }

        // During recording: save this input with the current frame number
        // player1=true → !player1=false → player2=false (this is Player 1)
        bot.onInput(button, !player1, push);
        GJBaseGameLayer::handleButton(push, button, player1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        // Run the original GD physics first
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        // Then advance our frame counter and replay inputs if in Playback mode
        MacroManager::get().onFrameAdvance(this);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PlayLayer hooks
//
// resetLevel: called on death and on manual restart (tap the restart button)
//   - During Playback: reset frame counter so replay starts from frame 0 again
//   - During Recording: restart the recording fresh (clear old inputs)
//
// onQuit: called when leaving the level (back button, or level complete)
//   - Always stop recording/playback when leaving
// ─────────────────────────────────────────────────────────────────────────────

class $modify(PlayLayer) {
    void resetLevel() {
        auto& bot = MacroManager::get();
        if (bot.state == BotState::Playing) {
            // Restart replay from the beginning
            bot.currentFrame = 0;
            bot.playbackIndex = 0;
        } else if (bot.state == BotState::Recording) {
            // Clear inputs and restart recording fresh
            bot.startRecording();
        }
        PlayLayer::resetLevel();
    }

    void onQuit() {
        // Clean up bot state when leaving the level
        MacroManager::get().stopPlayback();
        MacroManager::get().stopRecording();
        PlayLayer::onQuit();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PauseLayer hook
//
// customSetup: called when the pause menu is built
//   - We add a "YallBot." button at the bottom of the pause screen
//   - Tapping it opens the YallBotPopup
//   - The button only appears if the "Show Macro Button" setting is ON
// ─────────────────────────────────────────────────────────────────────────────

class $modify(YallBotPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        // Check the mod setting — user can hide the button if they want
        if (!Mod::get()->getSettingValue<bool>("show-ui-button")) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Create a small button at the bottom center of the pause menu
        auto menu = CCMenu::create();
        menu->setPosition({ winSize.width / 2, 22.f });
        menu->setZOrder(10);
        this->addChild(menu);

        auto bg = CCScale9Sprite::create("GJ_button_04.png");
        bg->setContentSize({ 100.f, 26.f });

        auto lbl = CCLabelBMFont::create("YallBot.", "bigFont.fnt");
        lbl->setScale(0.4f);
        lbl->setPosition(bg->getContentSize() / 2);
        bg->addChild(lbl);

        auto btn = CCMenuItemSpriteExtra::create(
            bg, this, menu_selector(YallBotPauseLayer::onOpenYallBot)
        );
        menu->addChild(btn);
    }

    // Opens the YallBot popup when the button is tapped
    void onOpenYallBot(CCObject*) {
        YallBotPopup::create()->show();
    }
};
