#pragma once
#include <imgui/imgui.h>
#include <string>

#include <utils/opengl_include_code.h>

namespace icons {
    extern ImTextureID LOGO;
    extern ImTextureID PLAY;
    extern ImTextureID STOP;
    extern ImTextureID MENU;
    extern ImTextureID MUTED;
    extern ImTextureID UNMUTED;
    extern ImTextureID NORMAL_TUNING;
    extern ImTextureID CENTER_TUNING;
    
    extern ImTextureID GPU_ENABLED;
    extern ImTextureID UP_CONV;
    extern ImTextureID SAT_FIXED;
    
    extern ImTextureID PREV;
    extern ImTextureID NEXT;
	
	extern ImTextureID LOCATE;
	extern ImTextureID ZOOM_IN;
	extern ImTextureID ZOOM_OUT;
	extern ImTextureID OPENSTREETMAP;
	
	extern ImTextureID AIRCRAFT;

    GLuint loadTexture(std::string path);
    bool load(std::string resDir);
}