#include "MacroManager.hpp"
#include <fstream>
#include <filesystem>

MacroManager& MacroManager::get() {
    static MacroManager instance;
    return instance;
}

void MacroManager::startRecording() {
    inputs.clear();
    currentFrame = 0;
    state = BotState::Recording;
    log::info("[YallBot] Recording started");
}

void MacroManager::stopRecording() {
    state = BotState::Idle;
    log::info("[YallBot] Recording stopped. {} inputs recorded.", inputs.size());
}

void MacroManager::startPlayback() {
    if (inputs.empty()) {
        log::warn("[YallBot] No macro loaded!");
        return;
    }
    currentFrame = 0;
    playbackIndex = 0;
    state = BotState::Playing;
    log::info("[YallBot] Playback started. {} inputs to replay.", inputs.size());
}

void MacroManager::stopPlayback() {
    state = BotState::Idle;
    log::info("[YallBot] Playback stopped.");
}

void MacroManager::clearMacro() {
    inputs.clear();
    currentFrame = 0;
    playbackIndex = 0;
    state = BotState::Idle;
    log::info("[YallBot] Macro cleared.");
}

void MacroManager::onFrameAdvance(GJBaseGameLayer* layer) {
    if (state == BotState::Playing) {
        // Replay all inputs for the current frame
        while (playbackIndex < (int)inputs.size() &&
               inputs[playbackIndex].frame == currentFrame) {
            auto& inp = inputs[playbackIndex];
            if (inp.down) {
                layer->pushButton(inp.button, !inp.player2);
            } else {
                layer->releaseButton(inp.button, !inp.player2);
            }
            playbackIndex++;
        }
    }

    if (state == BotState::Recording || state == BotState::Playing) {
        currentFrame++;
    }
}

void MacroManager::onInput(int button, bool player2, bool down) {
    if (state == BotState::Recording) {
        inputs.push_back({ currentFrame, button, player2, down });
    }
}

std::string MacroManager::getSaveDir() {
    auto dir = Mod::get()->getSaveDir() / "macros";
    std::filesystem::create_directories(dir);
    return dir.string();
}

bool MacroManager::saveMacro(const std::string& filename) {
    std::string path = getSaveDir() + "/" + filename + ".ybot";
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write header
    uint32_t count = (uint32_t)inputs.size();
    file.write(reinterpret_cast<char*>(&count), sizeof(count));

    // Write each input
    for (auto& inp : inputs) {
        file.write(reinterpret_cast<char*>(&inp.frame), sizeof(inp.frame));
        file.write(reinterpret_cast<char*>(&inp.button), sizeof(inp.button));
        file.write(reinterpret_cast<char*>(&inp.player2), sizeof(inp.player2));
        file.write(reinterpret_cast<char*>(&inp.down), sizeof(inp.down));
    }

    log::info("[YallBot] Macro saved to {}", path);
    return true;
}

bool MacroManager::loadMacro(const std::string& filename) {
    std::string path = getSaveDir() + "/" + filename + ".ybot";
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    inputs.clear();
    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t i = 0; i < count; i++) {
        MacroInput inp;
        file.read(reinterpret_cast<char*>(&inp.frame), sizeof(inp.frame));
        file.read(reinterpret_cast<char*>(&inp.button), sizeof(inp.button));
        file.read(reinterpret_cast<char*>(&inp.player2), sizeof(inp.player2));
        file.read(reinterpret_cast<char*>(&inp.down), sizeof(inp.down));
        inputs.push_back(inp);
    }

    log::info("[YallBot] Macro loaded from {} ({} inputs)", path, inputs.size());
    return true;
}
