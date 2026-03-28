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
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <sys/stat.h>
#include <imgui/stb_image.h>
#include <core.h>
#include <version.h>
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

namespace {
constexpr int DEFAULT_TILE_CACHE_MIN_DAYS = 7;
constexpr int DEFAULT_TILE_DOWNLOAD_CONCURRENCY = 2;
constexpr const char* DEFAULT_TILE_USER_AGENT = "GPSDR++/" VERSION_STR " (support@uugear.com)";
constexpr const char* OSM_ATTRIBUTION_TEXT = "© OpenStreetMap contributors";

struct TileHeaderCapture {
    std::string etag;
    std::string lastModified;
    long maxAgeSeconds = -1;
    std::time_t expiresAt = 0;
};

std::string trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool startsWithCaseInsensitive(const std::string& value, const std::string& prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

long parseMaxAgeSeconds(const std::string& cacheControl) {
    std::string lower = toLower(cacheControl);
    const std::string key = "max-age=";
    size_t pos = lower.find(key);
    if (pos == std::string::npos) {
        return -1;
    }
    pos += key.size();
    size_t end = pos;
    while (end < lower.size() && std::isdigit(static_cast<unsigned char>(lower[end]))) {
        ++end;
    }
    if (end == pos) {
        return -1;
    }
    try {
        return std::stol(lower.substr(pos, end - pos));
    } catch (...) {
        return -1;
    }
}

std::time_t computeExpiryTime(std::time_t now,
                              const TileHeaderCapture& headers,
                              int fallbackCacheMinDays) {
    if (headers.maxAgeSeconds >= 0) {
        return now + headers.maxAgeSeconds;
    }
    if (headers.expiresAt > 0) {
        return headers.expiresAt;
    }
    return now + static_cast<std::time_t>(std::max(fallbackCacheMinDays, 1) * 24 * 60 * 60);
}

size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    if (!userdata || total == 0) {
        return total;
    }

    auto* capture = static_cast<TileHeaderCapture*>(userdata);
    std::string line(buffer, total);
    line = trim(line);
    if (line.empty()) {
        return total;
    }

    if (startsWithCaseInsensitive(line, "etag:")) {
        capture->etag = trim(line.substr(5));
    } else if (startsWithCaseInsensitive(line, "last-modified:")) {
        capture->lastModified = trim(line.substr(14));
    } else if (startsWithCaseInsensitive(line, "cache-control:")) {
        capture->maxAgeSeconds = parseMaxAgeSeconds(trim(line.substr(14)));
    } else if (startsWithCaseInsensitive(line, "expires:")) {
        std::string expiresValue = trim(line.substr(8));
        time_t parsed = curl_getdate(expiresValue.c_str(), nullptr);
        if (parsed != -1) {
            capture->expiresAt = parsed;
        }
    }

    return total;
}
} // namespace

std::mutex MapView::MapTileProvider::downloadingFilesMutex;
std::unordered_set<std::string> MapView::MapTileProvider::downloadingFiles;
std::mutex MapView::MapTileProvider::workerMutex;
std::condition_variable MapView::MapTileProvider::workerCv;
std::queue<MapView::MapTileProvider::TileJob> MapView::MapTileProvider::tileJobs;
std::vector<std::thread> MapView::MapTileProvider::workers;
bool MapView::MapTileProvider::workersStarted = false;
bool MapView::MapTileProvider::stopWorkers = false;

MapView::MapTileProvider::~MapTileProvider() {
    shutdownWorkers();
}

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

std::string MapView::MapTileProvider::getTileFilePath(const std::string& outputFile) const {
    return tilesDir + "/" + outputFile;
}

std::string MapView::MapTileProvider::getMetadataFilePath(const std::string& outputFile) const {
    return getTileFilePath(outputFile) + ".meta";
}

bool MapView::MapTileProvider::loadTileMetadata(const std::string& metadataFilePath, TileCacheMetadata& metadata) {
    std::ifstream input(metadataFilePath);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("fetched_at=", 0) == 0) {
            try {
                metadata.fetchedAt = static_cast<std::time_t>(std::stoll(line.substr(11)));
            } catch (...) {}
        } else if (line.rfind("expiry_at=", 0) == 0) {
            try {
                metadata.expiryAt = static_cast<std::time_t>(std::stoll(line.substr(10)));
            } catch (...) {}
        } else if (line.rfind("etag=", 0) == 0) {
            metadata.etag = line.substr(5);
        } else if (line.rfind("last_modified=", 0) == 0) {
            metadata.lastModified = line.substr(14);
        }
    }

    return true;
}

bool MapView::MapTileProvider::saveTileMetadata(const std::string& metadataFilePath, const TileCacheMetadata& metadata) {
    std::ofstream output(metadataFilePath, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "fetched_at=" << static_cast<long long>(metadata.fetchedAt) << "\n";
    output << "expiry_at=" << static_cast<long long>(metadata.expiryAt) << "\n";
    output << "etag=" << metadata.etag << "\n";
    output << "last_modified=" << metadata.lastModified << "\n";
    return true;
}

bool MapView::MapTileProvider::isTileFresh(const std::string& tileFilePath,
        const std::string& metadataFilePath,
        TileCacheMetadata* metadata) {
    TileCacheMetadata localMetadata;
    bool hasMetadata = loadTileMetadata(metadataFilePath, localMetadata);
    std::time_t now = std::time(nullptr);

    if (hasMetadata && localMetadata.expiryAt > 0) {
        if (metadata) {
            *metadata = localMetadata;
        }
        return now < localMetadata.expiryAt;
    }

    struct stat st;
    if (stat(tileFilePath.c_str(), &st) == 0) {
        localMetadata.fetchedAt = st.st_mtime;
        localMetadata.expiryAt = st.st_mtime + static_cast<std::time_t>(std::max(fallbackCacheMinDays, 1) * 24 * 60 * 60);
        if (metadata) {
            *metadata = localMetadata;
        }
        return now < localMetadata.expiryAt;
    }

    if (metadata) {
        *metadata = TileCacheMetadata();
    }
    return false;
}

void MapView::MapTileProvider::ensureWorkersStarted() {
    std::lock_guard<std::mutex> lock(workerMutex);
    if (workersStarted) {
        return;
    }

    stopWorkers = false;
    const int workerCount = std::max(maxConcurrentDownloads, 1);
    workers.reserve(workerCount);
    for (int i = 0; i < workerCount; ++i) {
        workers.emplace_back(&MapView::MapTileProvider::workerLoop);
    }
    workersStarted = true;
}

void MapView::MapTileProvider::shutdownWorkers() {
    {
        std::lock_guard<std::mutex> lock(workerMutex);
        if (!workersStarted) {
            return;
        }
        stopWorkers = true;
    }
    workerCv.notify_all();

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers.clear();
    std::queue<TileJob> empty;
    std::swap(tileJobs, empty);
    workersStarted = false;
    stopWorkers = false;
}

void MapView::MapTileProvider::queueTileDownload(const TileJob& job) {
    ensureWorkersStarted();
    {
        std::lock_guard<std::mutex> lock(workerMutex);
        tileJobs.push(job);
    }
    workerCv.notify_one();
}

void MapView::MapTileProvider::workerLoop() {
    while (true) {
        TileJob job;
        {
            std::unique_lock<std::mutex> lock(workerMutex);
            workerCv.wait(lock, []() {
                return stopWorkers || !tileJobs.empty();
            });

            if (stopWorkers && tileJobs.empty()) {
                return;
            }

            job = tileJobs.front();
            tileJobs.pop();
        }

        auto& provider = MapView::getInstance().provider;
        bool success = false;
        try {
            success = provider.downloadTile(job);
        } catch (const std::exception& e) {
            flog::warn("Exception while downloading tile {0}: {1}", job.outputFile, e.what());
        } catch (...) {
            flog::warn("Unknown exception while downloading tile {0}", job.outputFile);
        }

        if (!success) {
            flog::warn("Failed to download/revalidate tile: {0}", job.outputFile);
        }
        provider.removeFromDownloadingList(job.outputFile);
    }
}

bool MapView::MapTileProvider::downloadTile(const TileJob& job) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        flog::warn("Failed to initialize CURL");
        return false;
    }

    std::ostringstream urlStream;
    urlStream << tileServerURL << job.zoom << "/" << job.tileX << "/" << job.tileY << ".png";
    std::string url = urlStream.str();

    std::string outputFilePath = getTileFilePath(job.outputFile);
    std::string metadataFilePath = getMetadataFilePath(job.outputFile);
    std::string tempFilePath = outputFilePath + ".part";

    FILE* file = fopen(tempFilePath.c_str(), "wb");
    if (!file) {
        flog::warn("Failed to open file for writing: {0}", tempFilePath);
        curl_easy_cleanup(curl);
        return false;
    }

    TileHeaderCapture headerCapture;
    curl_slist* requestHeaders = nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerCapture);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.empty() ? DEFAULT_TILE_USER_AGENT : userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    if (job.revalidate) {
        if (!job.metadata.etag.empty()) {
            requestHeaders = curl_slist_append(requestHeaders, ("If-None-Match: " + job.metadata.etag).c_str());
        }
        if (!job.metadata.lastModified.empty()) {
            requestHeaders = curl_slist_append(requestHeaders, ("If-Modified-Since: " + job.metadata.lastModified).c_str());
        }
        if (requestHeaders != nullptr) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, requestHeaders);
        }
    }

    CURLcode res = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    if (requestHeaders != nullptr) {
        curl_slist_free_all(requestHeaders);
    }

    fclose(file);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        flog::warn("CURL error for {0}: {1}", url, curl_easy_strerror(res));
        std::error_code ec;
        std::filesystem::remove(tempFilePath, ec);
        return false;
    }

    std::time_t now = std::time(nullptr);

    if (responseCode == 304 && job.revalidate) {
        std::error_code ec;
        std::filesystem::remove(tempFilePath, ec);

        TileCacheMetadata updatedMetadata = job.metadata;
        updatedMetadata.fetchedAt = now;
        updatedMetadata.expiryAt = computeExpiryTime(now, headerCapture, fallbackCacheMinDays);
        if (!headerCapture.etag.empty()) {
            updatedMetadata.etag = headerCapture.etag;
        }
        if (!headerCapture.lastModified.empty()) {
            updatedMetadata.lastModified = headerCapture.lastModified;
        }
        saveTileMetadata(metadataFilePath, updatedMetadata);
        flog::info("Tile revalidated: {0}", job.outputFile);
        return true;
    }

    if (responseCode != 200) {
        flog::warn("Unexpected HTTP status for {0}: {1}", url, responseCode);
        std::error_code ec;
        std::filesystem::remove(tempFilePath, ec);
        return false;
    }

    if (isTileFileEmpty(tempFilePath)) {
        flog::warn("Downloaded tile is empty: {0}", job.outputFile);
        std::error_code ec;
        std::filesystem::remove(tempFilePath, ec);
        return false;
    }

    std::error_code ec;
    std::filesystem::remove(outputFilePath, ec);
    ec.clear();
    std::filesystem::rename(tempFilePath, outputFilePath, ec);
    if (ec) {
        flog::warn("Failed to move downloaded tile into place {0}: {1}", job.outputFile, ec.message());
        std::filesystem::remove(tempFilePath, ec);
        return false;
    }

    TileCacheMetadata metadata;
    metadata.fetchedAt = now;
    metadata.expiryAt = computeExpiryTime(now, headerCapture, fallbackCacheMinDays);
    metadata.etag = headerCapture.etag;
    metadata.lastModified = headerCapture.lastModified;
    saveTileMetadata(metadataFilePath, metadata);

    flog::info(job.revalidate ? "Tile refreshed: {0}" : "Tile downloaded: {0}", job.outputFile);
    return true;
}

int MapView::MapTileProvider::requestTile(int tileX, int tileY, int zoom, const std::string& outputFile) {
    int maxTileCoord = (1 << zoom) - 1;
    if (tileX < 0 || tileX > maxTileCoord || tileY < 0 || tileY > maxTileCoord) {
        return TILE_INVALID;
    }

    std::string tileFilePath = getTileFilePath(outputFile);
    std::string metadataFilePath = getMetadataFilePath(outputFile);
    TileCacheMetadata metadata;

    if (tileFileExists(tileFilePath) && !isTileFileEmpty(tileFilePath)) {
        if (isTileFresh(tileFilePath, metadataFilePath, &metadata)) {
            return TILE_EXISTS;
        }

        if (addToDownloadingList(outputFile)) {
            TileJob job { tileX, tileY, zoom, outputFile, true, metadata };
            queueTileDownload(job);
        }
        return TILE_EXISTS;
    }

    std::error_code ec;
    std::filesystem::remove(tileFilePath, ec);
    std::filesystem::remove(metadataFilePath, ec);

    if (!addToDownloadingList(outputFile)) {
        return TILE_DOWNLOADING;
    }

    TileJob job { tileX, tileY, zoom, outputFile, false, TileCacheMetadata() };
    queueTileDownload(job);
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
    provider.userAgent = std::string(DEFAULT_TILE_USER_AGENT);
    provider.fallbackCacheMinDays = DEFAULT_TILE_CACHE_MIN_DAYS;
    provider.maxConcurrentDownloads = DEFAULT_TILE_DOWNLOAD_CONCURRENCY;
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
        ImVec2 attributionTextSize = ImGui::CalcTextSize(OSM_ATTRIBUTION_TEXT);
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 attributionPos = ImVec2(
                                    windowPos.x + availableRegion.x - attributionTextSize.x - 14.0f,
                                    windowPos.y + availableRegion.y - attributionTextSize.y - 10.0f
                                );
        drawList->AddRectFilled(
            ImVec2(attributionPos.x - 4.0f, attributionPos.y - 2.0f),
            ImVec2(attributionPos.x + attributionTextSize.x + 4.0f, attributionPos.y + attributionTextSize.y + 2.0f),
            IM_COL32(255, 255, 255, 180),
            3.0f
        );
        drawList->AddText(attributionPos, IM_COL32(0, 0, 0, 255), OSM_ATTRIBUTION_TEXT);

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
