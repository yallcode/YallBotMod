#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <string>

using namespace geode::prelude;

// A single recorded input event
struct MacroInput {
    int frame;        // Physics frame number when this input happened
    int button;       // 1 = jump (left), 2 = jump (right)
    bool player2;     // false = player 1, true = player 2
    bool down;        // true = press, false = release
};

enum class BotState {
    Idle,
    Recording,
    Playing
};

class MacroManager {
public:
    static MacroManager& get();

    // State
    BotState state = BotState::Idle;
    std::vector<MacroInput> inputs;
    int currentFrame = 0;
    int playbackIndex = 0;

    // Actions
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    void clearMacro();

    // Called every physics frame during recording/playback
    void onFrameAdvance(GJBaseGameLayer* layer);

    // Called when player presses/releases a button
    void onInput(int button, bool player2, bool down);

    // Save/load
    bool saveMacro(const std::string& filename);
    bool loadMacro(const std::string& filename);

    std::string getSaveDir();

private:
    MacroManager() = default;
};
