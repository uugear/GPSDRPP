#include "loading_animation.h"
#include <iostream>

// Static array definition - rotating spinner characters
const std::array<char, 4> LoadingAnimation::frames = { '-', '\\', '|', '/' };

char LoadingAnimation::getLoadingText() const {
    float currentTime = getCurrentTime();
    if (currentTime - lastUpdateTime >= updateInterval) {
        frameIndex = (frameIndex + 1) % 4;
        lastUpdateTime = currentTime;
    }
    return frames[frameIndex];
}

void LoadingAnimation::resetAnimation() {
    frameIndex = 0;
    lastUpdateTime = 0.0f;
}

int LoadingAnimation::getAnimationProgress() const {
    return frameIndex;
}

void LoadingAnimation::setUpdateInterval(float interval) {
    if (interval > 0.0f) {
        updateInterval = interval;
    }
}

float LoadingAnimation::getUpdateInterval() const {
    return updateInterval;
}

float LoadingAnimation::getCurrentTime() const {
    #ifdef IMGUI_VERSION
        return ImGui::GetTime();
    #else
        static auto start = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<float>(now - start).count();
    #endif
}
