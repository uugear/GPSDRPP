#include <gui/widgets/map_view.h>
#include <gui/icons.h>
#include <thread>
#include <ostream>
#include <utils/flog.h>
#include <curl/curl.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <imgui/stb_image.h>
#include <core.h>
#include <gui/widgets/mode_s_page.h>


#define TILE_INVALID			-1
#define TILE_EXISTS				0
#define TILE_DOWNLOAD_STARTED	1
#define TILE_DOWNLOADING		2

#define TILE_WIDTH				256
#define TILE_HALF_WIDTH			128
#define TILE_HEIGHT				256
#define TILE_HALF_HEIGHT		128

#define CONTROL_BUTTON_SIZE		40
#define CONTROL_BUTTON_MARGIN	30	

#define OSM_ICON_WIDTH			136
#define OSM_ICON_HEIGHT			40


std::mutex MapView::MapTileProvider::downloadingFilesMutex;
std::unordered_set<std::string> MapView::MapTileProvider::downloadingFiles;

bool MapView::MapTileProvider::tileFileExists(const std::string& filename) {
	std::ifstream f(filename.c_str());
	return f.good();
}

bool MapView::MapTileProvider::isTileFileEmpty(const std::string& filename) {
    try {
        return std::filesystem::file_size(filename) == 0;
    } catch (const std::filesystem::filesystem_error&) {
        return false;
    }
}

bool MapView::MapTileProvider::addToDownloadingList(const std::string& filename) {
	std::lock_guard<std::mutex> lock(downloadingFilesMutex);
	if (downloadingFiles.find(filename) != downloadingFiles.end()) {
		return false;
	}
	downloadingFiles.insert(filename);
	return true;
}

void MapView::MapTileProvider::removeFromDownloadingList(const std::string& filename) {
	std::lock_guard<std::mutex> lock(downloadingFilesMutex);
	downloadingFiles.erase(filename);
}

bool MapView::MapTileProvider::downloadTile(int tileX, int tileY, int zoom, const std::string& outputFile) {
    CURL* curl = curl_easy_init();
    if (!curl) {
		flog::warn("Failed to initialize CURL");
        return false;
    }

    std::ostringstream urlStream;
    urlStream << tileServerURL << zoom << "/" << tileX << "/" << tileY << ".png";
    std::string url = urlStream.str();
	
	std::string outputFilePath = tilesDir + "/" + outputFile;
    FILE* file = fopen(outputFilePath.c_str(), "wb");
    if (!file) {
		flog::warn("Failed to open file for writing: {0}", outputFile);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

    // User-Agent header
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "TileDownloader/1.0 (your_email@example.com)");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
		flog::warn("CURL error: {0}", curl_easy_strerror(res));
        fclose(file);
        curl_easy_cleanup(curl);
        return false;
    }

    fclose(file);
    curl_easy_cleanup(curl);
    return true;
}

int MapView::MapTileProvider::requestTile(int tileX, int tileY, int zoom, const std::string& outputFile) {
	
	int maxTileCoord = (1 << zoom) - 1;
	if (tileX < 0 || tileX > maxTileCoord || tileY < 0 || tileY > maxTileCoord) {
		return TILE_INVALID;
	}
	
	std::string tileFilePath = tilesDir + "/" + outputFile;
	if (tileFileExists(tileFilePath) && !isTileFileEmpty(tileFilePath)) {
		return TILE_EXISTS;
	}
	
	if (!addToDownloadingList(outputFile)) {
		return TILE_DOWNLOADING;
	}

	std::thread downloadThread([this, tileX, tileY, zoom, outputFile]() {
		auto cleanup = [this, outputFile]() {
			removeFromDownloadingList(outputFile);
		};
		try {
			bool success = downloadTile(tileX, tileY, zoom, outputFile);
			if (!success) {
				flog::warn("Failed to download tile: {0}", outputFile);
                std::error_code ec;
                std::filesystem::remove(tilesDir + "/" + outputFile, ec);
			} else {
				flog::info("Download OK: {0}", outputFile);
			}
		}
		catch (const std::exception& e) {
			flog::warn("Exception while downloading tile: {0}", e.what());
			std::error_code ec;
            std::filesystem::remove(tilesDir + "/" + outputFile, ec);
		}
		cleanup();
	});
	
	downloadThread.detach();
	
	return TILE_DOWNLOAD_STARTED;
}

GLuint MapView::MapTileProvider::getTexture(std::string tileFile) {
	auto it = textureMap.find(tileFile);
	if (it != textureMap.end()) {
		return it->second;
	}
	
    int w, h, ch;
	std::string path = tilesDir + "/" + tileFile;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
	if (!data) {
		return 0;
	}

    GLuint tex;
    glGenTextures(1, &tex);
	if (tex == 0) {
        stbi_image_free(data);
        return 0;
    }
	
    glBindTexture(GL_TEXTURE_2D, tex);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	
    stbi_image_free(data);
	
	if (tex != 0) {
		textureMap[tileFile] = tex;
	}
	
    return tex;
}


double MapView::OSMScaleBar::calculateGroundResolution(double latitude, int zoomLevel) {
    double latitudeRad = latitude * M_PI / 180.0;
    return (cos(latitudeRad) * 2.0 * M_PI * EARTH_RADIUS) / (TILE_SIZE * pow(2.0, zoomLevel));
}

MapView::OSMScaleBar::ScaleInfo MapView::OSMScaleBar::calculateBestScale(double groundResolution, int maxPixelWidth) {
    static const double scales[] = {	// Predefined standard scale lengths
        1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500,
        1000, 2000, 2500, 5000, 10000, 20000, 25000, 50000,
        100000, 200000, 250000, 500000, 1000000
    };
    ScaleInfo result;
    for (double scale : scales) {
        int pixelLength = static_cast<int>(scale / groundResolution);
        if (pixelLength > maxPixelWidth) break;
        result.distance = scale;
        result.pixelLength = pixelLength;
        if (scale >= 1000) {
            result.unit = "km";
            if (scale >= 1000000) {
                result.label = std::to_string(static_cast<int>(scale / 1000000)) + " Mkm";
            } else if (static_cast<int>(scale) % 1000 == 0) {
                result.label = std::to_string(static_cast<int>(scale / 1000)) + " km";
            } else {
                double kmValue = scale / 1000.0;
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%.1f km", kmValue);
                std::string formatted(buffer);
                if (formatted.find(".0 km") != std::string::npos) {
                    formatted = std::to_string(static_cast<int>(kmValue)) + " km";
                }
				result.label = formatted;
            }
        } else {
            result.unit = "m";
            result.label = std::to_string(static_cast<int>(scale)) + " m";
        }
    }
    return result;
}

void MapView::OSMScaleBar::drawScaleBar(double latitude, int zoomLevel, ImVec2 position) {
    double groundResolution = calculateGroundResolution(latitude, zoomLevel);
    ScaleInfo scale = calculateBestScale(groundResolution);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    
    // Default position: bottom left corner
    if (position.x < 0 || position.y < 0) {
        position = ImVec2(windowPos.x + 10, windowPos.y + windowSize.y - 15);
    } else {
        position = ImVec2(windowPos.x + position.x, windowPos.y + position.y);
    }
    
    // Scale bar styling
    const float lineThickness = 2.0f;
    const float tickHeight = 6.0f;
    const ImU32 lineColor = IM_COL32(0, 0, 0, 200);
    const ImU32 bgColor = IM_COL32(255, 255, 255, 160);
    const ImU32 textColor = IM_COL32(0, 0, 0, 255);
    
    // Calculate text size and positions
    ImVec2 textSize = ImGui::CalcTextSize(scale.label.c_str());
    const float textPadding = 5.0f; // Space between text and scale bar
    
    // Position text on the left, scale bar on the right
    ImVec2 textPos = ImVec2(position.x, position.y - textSize.y / 2);
    ImVec2 lineStart = ImVec2(position.x + textSize.x + textPadding, position.y);
    ImVec2 lineEnd = ImVec2(lineStart.x + scale.pixelLength, position.y);
    
    // Draw background (adjusted for new layout)
    ImVec2 bgMin = ImVec2(position.x - 5, position.y - textSize.y / 2 - 3);
    ImVec2 bgMax = ImVec2(lineEnd.x + 5, position.y + textSize.y / 2 + 3);
    drawList->AddRectFilled(bgMin, bgMax, bgColor, 3.0f);
    
    // Draw label text (on the left)
    drawList->AddText(textPos, textColor, scale.label.c_str());
    
    // Draw main line
    drawList->AddLine(lineStart, lineEnd, lineColor, lineThickness);
    
    // Draw start and end tick marks
    drawList->AddLine(
        ImVec2(lineStart.x, lineStart.y - tickHeight/2), 
        ImVec2(lineStart.x, lineStart.y + tickHeight/2), 
        lineColor, lineThickness
    );
    drawList->AddLine(
        ImVec2(lineEnd.x, lineEnd.y - tickHeight/2), 
        ImVec2(lineEnd.x, lineEnd.y + tickHeight/2), 
        lineColor, lineThickness
    );
}

double MapView::OSMScaleBar::getGroundResolution(double latitude, int zoomLevel) {
    return calculateGroundResolution(latitude, zoomLevel);
}


MapView::MapView() {
    
}

MapView::~MapView() {

}

const char* MapView::getLabel() {
	return "Map";
}

void MapView::init() {
	core::configManager.acquire();
	provider.tileServerURL = core::configManager.conf["tileServer"];
	zoom = core::configManager.conf["zoom"];
	panLon = core::configManager.conf["panLon"];
	panLat = core::configManager.conf["panLat"];
	prevPanLon = panLon;
	prevPanLat = panLat;
	longitude = core::configManager.conf["longitude"];
	latitude = core::configManager.conf["latitude"];
    core::configManager.release();
}

void MapView::deinit() {
	core::configManager.acquire();
	core::configManager.conf["longitude"] = longitude;
	core::configManager.conf["latitude"] = latitude;
	core::configManager.release(true);
}

void MapView::drawRotatedImage(ImDrawList* drawList, ImTextureID texture, 
                              ImVec2 center, ImVec2 size, float angleRadians, 
                              ImU32 tintColor = IM_COL32(255, 255, 255, 255)) {
    
    float halfWidth = size.x * 0.5f;
    float halfHeight = size.y * 0.5f;
    
    float cos_a = std::cos(angleRadians);
    float sin_a = std::sin(angleRadians);
    
    ImVec2 corners[4] = {
        ImVec2(-halfWidth, -halfHeight),
        ImVec2( halfWidth, -halfHeight),
        ImVec2( halfWidth,  halfHeight),
        ImVec2(-halfWidth,  halfHeight)
    };
    
    ImVec2 rotatedCorners[4];
    for (int i = 0; i < 4; i++) {
        float rotatedX = corners[i].x * cos_a - corners[i].y * sin_a;
        float rotatedY = corners[i].x * sin_a + corners[i].y * cos_a;
        rotatedCorners[i] = ImVec2(center.x + rotatedX, center.y + rotatedY);
    }
    
    ImVec2 uvs[4] = {
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 0.0f),
        ImVec2(1.0f, 1.0f),
        ImVec2(0.0f, 1.0f)
    };
    
    drawList->AddImageQuad(
        texture,
        rotatedCorners[0], rotatedCorners[1], rotatedCorners[2], rotatedCorners[3],
        uvs[0], uvs[1], uvs[2], uvs[3],
        tintColor
    );
}

void MapView::draw() {
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
	ImGui::BeginChild("Map View", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	
	bool posLocked = (core::gps.getFixQuality() != 0);
	if (posLocked) {
		prevPosLocked = true;
		longitude = core::gps.getLongitude();
		latitude = core::gps.getLatitude();
	} else {
	    // Save previously locked position to configuration
	    if (prevPosLocked) {
    		prevPosLocked = false;
    		core::configManager.acquire();
    		core::configManager.conf["longitude"] = longitude;
    		core::configManager.conf["latitude"] = latitude;
    		core::configManager.release(true);
    	}
	}

    int tileX, tileY;
	int pxOffsetX, pxOffsetY;
	latLonToTilePixel(latitude, longitude, tileX, tileY, pxOffsetX, pxOffsetY);
	
	std::string outputFile = "tile_" + std::to_string(zoom) + "_" + 
							std::to_string(tileX) + "_" + 
							std::to_string(tileY) + ".png";

	int result = provider.requestTile(tileX, tileY, zoom, outputFile);
	if (result == TILE_EXISTS) {
		// Center tile
		ImTextureID tile = (ImTextureID)(uintptr_t)provider.getTexture(outputFile);
		
		ImVec2 availableRegion = ImGui::GetContentRegionAvail();
		float offsetX = (availableRegion.x - TILE_WIDTH) * 0.5f;
		float offsetY = (availableRegion.y - TILE_HEIGHT) * 0.5f;
		
		// Panning
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
			if (drag.x != 0 || drag.y != 0) {
				panLon = prevPanLon + drag.x;
				panLat = prevPanLat + drag.y;
			}
		} else {
			prevPanLon = panLon;
			prevPanLat = panLat;
			core::configManager.acquire();
			core::configManager.conf["panLon"] = panLon;
			core::configManager.conf["panLat"] = panLat;
			core::configManager.release(true);
		}
		offsetX = offsetX + panLon;
		offsetY = offsetY + panLat;
		
		ImVec2 tileScreenPos = ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY);
		ImGui::SetCursorPos(tileScreenPos);
		ImVec2 tileGlobalPos = ImGui::GetCursorScreenPos();
		ImGui::Image(
			(void*)(intptr_t)tile, 
			ImVec2(TILE_WIDTH, TILE_HEIGHT)
		);
		
		// Other tiles around
		float tileCenterOffsetX = tileScreenPos.x - TILE_HALF_WIDTH;
		float tileCenterOffsetY = tileScreenPos.y - TILE_HALF_HEIGHT;
		int tilesNeededLeft = static_cast<int>(std::ceil(tileCenterOffsetX / TILE_WIDTH)) + 1;
		int tilesNeededRight = static_cast<int>(std::ceil((availableRegion.x - (tileCenterOffsetX + TILE_WIDTH)) / TILE_WIDTH));
		int tilesNeededUp = static_cast<int>(std::ceil(tileCenterOffsetY / TILE_HEIGHT)) + 1;
		int tilesNeededDown = static_cast<int>(std::ceil((availableRegion.y - (tileCenterOffsetY + TILE_HEIGHT)) / TILE_HEIGHT));
		int startX = tileX - tilesNeededLeft;
		int endX = tileX + tilesNeededRight;
		int startY = tileY - tilesNeededUp;
		int endY = tileY + tilesNeededDown;
		for (int y = startY; y <= endY; ++y) {
			for (int x = startX; x <= endX; ++x) {
				if (x != tileX || y != tileY) {
					std::string outputFile = "tile_" + std::to_string(zoom) + "_" + 
							std::to_string(x) + "_" + 
							std::to_string(y) + ".png";
					int res = provider.requestTile(x, y, zoom, outputFile);
					if (res == TILE_EXISTS) {
						ImVec2 tilePos = ImVec2(tileScreenPos.x + (x - tileX) * TILE_WIDTH, tileScreenPos.y + (y - tileY) * TILE_HEIGHT);
						ImGui::SetCursorPos(tilePos);
						ImTextureID tile = (ImTextureID)(uintptr_t)provider.getTexture(outputFile);
						ImGui::Image(
							(void*)(intptr_t)tile, 
							ImVec2(TILE_WIDTH, TILE_HEIGHT)
						);
					} else if (res == TILE_DOWNLOADING) {
						ImVec2 tilePos = ImVec2(tileScreenPos.x + (x - tileX) * TILE_WIDTH, tileScreenPos.y + (y - tileY) * TILE_HEIGHT);
						const char* text = "Downloading...";
						ImVec2 textSize = ImGui::CalcTextSize(text);
						ImVec2 textPos = ImVec2(
							tilePos.x + (TILE_WIDTH - textSize.x) * 0.5f,
							tilePos.y + (TILE_HEIGHT - textSize.y) * 0.5f
						);
						ImGui::SetCursorPos(textPos);
						ImGui::Text("%s", text);
					}
				}
			}
		}
		
		// Current position marker
		ImVec2 markerPos = ImVec2(tileGlobalPos.x + pxOffsetX, tileGlobalPos.y + pxOffsetY);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddCircleFilled(markerPos, 5.0f, posLocked ? IM_COL32(255, 0, 0, 255) : IM_COL32(128, 128, 128, 255));
		
		// Controls
		ImVec2 btnSize(CONTROL_BUTTON_SIZE, CONTROL_BUTTON_SIZE);
		ImVec2 ctrlPos = ImVec2(availableRegion.x - CONTROL_BUTTON_SIZE - CONTROL_BUTTON_MARGIN, CONTROL_BUTTON_MARGIN);
		ImGui::SetCursorPos(ctrlPos);

        // Center location
        if (ImGui::ImageButton(icons::LOCATE, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0))) {
            ImVec2 availableRegion = ImGui::GetContentRegionAvail();
            ImVec2 screenCenter = ImVec2(availableRegion.x * 0.5f, availableRegion.y * 0.5f);
            panLon = screenCenter.x - ((availableRegion.x - TILE_WIDTH) * 0.5f) - pxOffsetX;
            panLat = screenCenter.y - ((availableRegion.y - TILE_HEIGHT) * 0.5f) - pxOffsetY;
            prevPanLon = panLon;
            prevPanLat = panLat;
            core::configManager.acquire();
            core::configManager.conf["panLon"] = panLon;
            core::configManager.conf["panLat"] = panLat;
            core::configManager.release(true);
		}

        // Zoom in
        ctrlPos.y += CONTROL_BUTTON_SIZE + CONTROL_BUTTON_MARGIN;
        ImGui::SetCursorPos(ctrlPos);
        if (ImGui::ImageButton(icons::ZOOM_IN, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), zoom < 19 ? ImVec4(0, 0, 0, 1) : ImVec4(0, 0, 0, 0.3))) {
            if (zoom < 19) {
                ImVec2 screenCenter = ImVec2(availableRegion.x * 0.5f, availableRegion.y * 0.5f);
                ImVec2 tileScreenPos = ImVec2(offsetX, offsetY);
                float centerOffsetFromGPSX = screenCenter.x - (tileScreenPos.x + pxOffsetX);
                float centerOffsetFromGPSY = screenCenter.y - (tileScreenPos.y + pxOffsetY);
                zoom++;
                centerOffsetFromGPSX *= 2.0f;
                centerOffsetFromGPSY *= 2.0f;
                int newTileX, newTileY, newPxOffsetX, newPxOffsetY;
                latLonToTilePixel(latitude, longitude, newTileX, newTileY, newPxOffsetX, newPxOffsetY);
                float newTileDefaultX = (availableRegion.x - TILE_WIDTH) * 0.5f;
                float newTileDefaultY = (availableRegion.y - TILE_HEIGHT) * 0.5f;
                panLon = screenCenter.x - newTileDefaultX - newPxOffsetX - centerOffsetFromGPSX;
                panLat = screenCenter.y - newTileDefaultY - newPxOffsetY - centerOffsetFromGPSY;
                prevPanLon = panLon;
                prevPanLat = panLat;
                core::configManager.acquire();
                core::configManager.conf["zoom"] = zoom;
                core::configManager.conf["panLon"] = panLon;
                core::configManager.conf["panLat"] = panLat;
                core::configManager.release(true);
            }
        }

        // Zoom out
        ctrlPos.y += CONTROL_BUTTON_SIZE + CONTROL_BUTTON_MARGIN;
        ImGui::SetCursorPos(ctrlPos);
        if (ImGui::ImageButton(icons::ZOOM_OUT, btnSize, ImVec2(0, 0), ImVec2(1, 1), 5, ImVec4(0, 0, 0, 0), zoom > 1 ? ImVec4(0, 0, 0, 1) : ImVec4(0, 0, 0, 0.3))) {
            if (zoom > 1) {
                ImVec2 screenCenter = ImVec2(availableRegion.x * 0.5f, availableRegion.y * 0.5f);
                ImVec2 tileScreenPos = ImVec2(offsetX, offsetY);
                float centerOffsetFromGPSX = screenCenter.x - (tileScreenPos.x + pxOffsetX);
                float centerOffsetFromGPSY = screenCenter.y - (tileScreenPos.y + pxOffsetY);
                zoom--;
                centerOffsetFromGPSX *= 0.5f;
                centerOffsetFromGPSY *= 0.5f;
                int newTileX, newTileY, newPxOffsetX, newPxOffsetY;
                latLonToTilePixel(latitude, longitude, newTileX, newTileY, newPxOffsetX, newPxOffsetY);
                float newTileDefaultX = (availableRegion.x - TILE_WIDTH) * 0.5f;
                float newTileDefaultY = (availableRegion.y - TILE_HEIGHT) * 0.5f;
                panLon = screenCenter.x - newTileDefaultX - newPxOffsetX - centerOffsetFromGPSX;
                panLat = screenCenter.y - newTileDefaultY - newPxOffsetY - centerOffsetFromGPSY;
                prevPanLon = panLon;
                prevPanLat = panLat;
                core::configManager.acquire();
                core::configManager.conf["zoom"] = zoom;
                core::configManager.conf["panLon"] = panLon;
                core::configManager.conf["panLat"] = panLat;
                core::configManager.release(true);
            }
        }
		
		// OpenStreetMap attribution
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
		ImGui::SetCursorPos(ImVec2(availableRegion.x - OSM_ICON_WIDTH - 10, availableRegion.y - OSM_ICON_HEIGHT - 5));
		ImGui::ImageButton(icons::OPENSTREETMAP, ImVec2(OSM_ICON_WIDTH, OSM_ICON_HEIGHT), ImVec2(0, 0), ImVec2(1, 1));
		ImGui::PopStyleColor(3);
		
		// Scale bar
		scaleBar.drawScaleBar(latitude, zoom);
		
		// Draw airplanes, if ADS-B is on
		if (ModeSPage::getInstance().isRunning()) {
			struct mode_s_context * ctx = ModeSPage::getInstance().getContext();
			struct aircraft *a = ctx->aircrafts;
			time_t now = time(NULL);
			ImU32 color = IM_COL32(0, 70, 140, 255);
			while(a) {
				if(a->lat != 0 || a->lon != 0) {
					int acTileX, acTileY;
					int acPxOffsetX, acPxOffsetY;
					latLonToTilePixel(a->lat, a->lon, acTileX, acTileY, acPxOffsetX, acPxOffsetY);
					if (acTileX >= startX && acTileX <= endX && acTileY >= startY && acTileY <= endY) {
						ImVec2 acPos = ImVec2(tileGlobalPos.x + (acTileX - tileX) * TILE_WIDTH + acPxOffsetX, 
							tileGlobalPos.y + (acTileY - tileY) * TILE_HEIGHT + acPxOffsetY);
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						drawRotatedImage(drawList, icons::AIRCRAFT, acPos, ImVec2(24, 24), a->track, color);

						ImVec2 textPos = ImVec2(acPos.x - 12, acPos.y + 14);
						drawList->AddText(textPos, color, a->hexaddr);
						if (a->flight[0] != '\0') {
                            textPos = ImVec2(acPos.x - 12, acPos.y + 28);
                            drawList->AddText(textPos, color, a->flight);
                        }
					}
				}
				a = a->next;
			}
		}
	}
	
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

// Convert latitude and longitude to pixel coordinates in the Web Mercator projection
void MapView::latLonToPixel(double lat, double lon, int &pixelX, int &pixelY) {
    double lat_rad = lat * M_PI / 180.0;
    int n = 1 << zoom;  // Number of tiles
    double x = (lon + 180.0) / 360.0 * n * TILE_WIDTH;
    double y = (1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / M_PI) / 2.0 * n * TILE_HEIGHT;
    pixelX = static_cast<int>(x);
    pixelY = static_cast<int>(y);
}

// Calculate the pixel position of the current location within the current tile
void MapView::latLonToTilePixel(double lat, double lon, int &tileX, int &tileY, int &offsetX, int &offsetY) {
    int pixelX, pixelY;
    latLonToPixel(lat, lon, pixelX, pixelY);
    
    tileX = pixelX / TILE_WIDTH;
    tileY = pixelY / TILE_HEIGHT;

    offsetX = pixelX % TILE_WIDTH;
    offsetY = pixelY % TILE_HEIGHT;
}

void MapView::setRootPath(std::string path) {
	std::string tilesPath = path + "/tiles";
	std::filesystem::create_directory(tilesPath);
    provider.tilesDir = tilesPath;
}
