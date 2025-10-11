#ifndef LOADING_ANIMATION_H
#define LOADING_ANIMATION_H

#include <array>
#include <chrono>

#ifdef IMGUI_VERSION
#include <imgui.h>
#endif

/**
 * A lightweight loading animation class that displays a rotating spinner
 * with automatic time control to manage frame updates.
 */
class LoadingAnimation {
private:
    mutable int frameIndex = 0;
    mutable float lastUpdateTime = 0.0f;
    mutable float updateInterval = 0.2f;  // Update interval in seconds
    static const std::array<char, 4> frames;

public:
    /**
     * Get current loading animation frame with internal time control.
     * Only updates the frame if enough time has passed since last update.
     * @return Current spinner character ('-', '\', '|', or '/')
     */
    char getLoadingText() const;
    
    /**
     * Reset animation to initial state.
     * Sets frame index to 0 and resets internal timer.
     */
    void resetAnimation();
    
    /**
     * Get current animation progress.
     * @return Current frame index (0-3)
     */
    int getAnimationProgress() const;
    
    /**
     * Set custom update interval for animation speed.
     * @param interval Time interval between frames in seconds
     */
    void setUpdateInterval(float interval);
    
    /**
     * Get current update interval.
     * @return Current update interval in seconds
     */
    float getUpdateInterval() const;

private:
    /**
     * Get current time based on available framework.
     * Uses ImGui::GetTime() if ImGui is available, otherwise uses chrono.
     * @return Current time in seconds
     */
    float getCurrentTime() const;
};

#endif // LOADING_ANIMATION_H