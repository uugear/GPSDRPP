#pragma once
#include <gui/widgets/main_view.h>
#include <string>
#include <cstdio>
#include <unordered_set>
#include <mutex>
#include <utils/opengl_include_code.h>

class MapView : public MainView::TabView {
public:
	static MapView& getInstance() {
        static MapView instance;
        return instance;
    }
	MapView(const MapView&) = delete;
    MapView& operator=(const MapView&) = delete;
	
	const char* getLabel();
	void init();
	void deinit();
    void draw();
	void setRootPath(std::string path);
	
private:
	MapView();
    ~MapView();
	
	class MapTileProvider {
	private:
		static std::mutex downloadingFilesMutex;
		static std::unordered_set<std::string> downloadingFiles;
		std::unordered_map<std::string, GLuint> textureMap;
		bool tileFileExists(const std::string& filename);
		bool isTileFileEmpty(const std::string& filename);
		bool addToDownloadingList(const std::string& filename);
		void removeFromDownloadingList(const std::string& filename);
		bool downloadTile(int tileX, int tileY, int zoom, const std::string& outputFile);
		static size_t writeFileCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
			return fwrite(ptr, size, nmemb, stream);
		}
	public:
		std::string tileServerURL;
		std::string tilesDir;
		int requestTile(int tileX, int tileY, int zoom, const std::string& outputFile);
		GLuint getTexture(std::string tileFile);
	};
	
    class OSMScaleBar {
    private:
        static constexpr double EARTH_RADIUS = 6378137.0; // WGS84 ellipsoid semi-major axis
        static constexpr double TILE_SIZE = 256.0;
        
        struct ScaleInfo {
            double distance;   // Actual distance
            int pixelLength;   // Pixel length
            std::string unit;  // Unit
            std::string label; // Display label
        };
        
        double calculateGroundResolution(double latitude, int zoomLevel);
        ScaleInfo calculateBestScale(double groundResolution, int maxPixelWidth = 150);
        
    public:
        void drawScaleBar(double latitude, int zoomLevel, ImVec2 position = ImVec2(-1, -1));
        double getGroundResolution(double latitude, int zoomLevel);
    };
    
	MapTileProvider provider;

    OSMScaleBar scaleBar;

	bool prevPosLocked;
	int zoom;
	double panLon = 0.0f;
	double panLat = 0.0f;
	double prevPanLon = 0.0f;
	double prevPanLat = 0.0f;
    double longitude = 0.0f;
	double latitude = 0.0f;
	
	void latLonToPixel(double lat, double lon, int &pixelX, int &pixelY);
	void latLonToTilePixel(double lat, double lon, int &tileX, int &tileY, int &offsetX, int &offsetY);
    void drawRotatedImage(ImDrawList* drawList, ImTextureID texture, ImVec2 center, ImVec2 size, float angleRadians, ImU32 tintColor);
};
