#include <gui/icons.h>
#include <stdint.h>
#include <config.h>

#define STB_IMAGE_IMPLEMENTATION
#include <imgui/stb_image.h>
#include <filesystem>
#include <utils/flog.h>

namespace icons {
    ImTextureID LOGO;
    ImTextureID PLAY;
    ImTextureID STOP;
    ImTextureID MENU;
    ImTextureID MUTED;
    ImTextureID UNMUTED;
    ImTextureID NORMAL_TUNING;
    ImTextureID CENTER_TUNING;
    
    ImTextureID GPU_ENABLED;
    ImTextureID UP_CONV;
    ImTextureID SAT_FIXED;
    
    ImTextureID PREV;
    ImTextureID NEXT;
	
	ImTextureID LOCATE;
	ImTextureID ZOOM_IN;
	ImTextureID ZOOM_OUT;
	ImTextureID OPENSTREETMAP;
	
	ImTextureID AIRCRAFT;

    GLuint loadTexture(std::string path) {
        int w, h, n;
        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 0);
        GLuint texId;
        glGenTextures(1, &texId);

        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (uint8_t*)data);
        stbi_image_free(data);
        return texId;
    }

    bool load(std::string resDir) {
        if (!std::filesystem::is_directory(resDir)) {
            flog::error("Invalid resource directory: {0}", resDir);
            return false;
        }

        LOGO = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/gpsdrpp.png");
        PLAY = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/play.png");
        STOP = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/stop.png");
        MENU = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/menu.png");
        MUTED = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/muted.png");
        UNMUTED = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/unmuted.png");
        NORMAL_TUNING = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/normal_tuning.png");
        CENTER_TUNING = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/center_tuning.png");
        
        GPU_ENABLED = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/gpu_accel.png");
        UP_CONV = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/up_conv.png");
        SAT_FIXED = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/sat_fixed.png");

        PREV = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/prev.png");
        NEXT = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/next.png");
		
		LOCATE = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/locate.png");
		ZOOM_IN = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/zoom_in.png");
		ZOOM_OUT = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/zoom_out.png");
		OPENSTREETMAP = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/openstreetmap.png");
		
		AIRCRAFT = (ImTextureID)(uintptr_t)loadTexture(resDir + "/icons/aircraft.png");

        return true;
    }
}
