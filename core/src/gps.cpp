#include "gps.h"
#include <iostream>
#include <string>
#include <sstream>
#include <utils/flog.h>
#include <core.h>
#include <signal_path/signal_path.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>


// Constructor
Gps::Gps() {
	lock_fd = open(GPS_I2C_LOCK, O_CREAT | O_RDWR, 0666);

	utc_hour = 0xFF;
    utc_minute = 0xFF;
    utc_second = 0xFF;
    latitude_md = 0;
    longitude_md = 0;
    fix_quality = 0;
    used_satellites = 0;

    satellites.reserve(MAX_SATELLITES);

	readerBuffer.resize(BUFFER_SIZE);

    initialize();
}

bool Gps::initialize() {
	initialized = false;

	flog::info("Initialize GPS...");

    // Connect to STM8
    stm8_fd = open(GPS_I2C_BUS, O_RDWR);
    if (stm8_fd < 0) {
        flog::error("Gps: failed to open the i2c bus");
		return false;
    }
	if (ioctl(stm8_fd, I2C_SLAVE, GPS_I2C_STM8_ADDRESS) < 0) {
		flog::error("Gps: failed to set GPS STM8 I2C address");
		close(stm8_fd);
		stm8_fd = -1;
		return false;
	}
	uint8_t test_byte;
	if (read(stm8_fd, &test_byte, 1) < 0) {
		flog::warn("GPS STM8 is not connected");
		close(stm8_fd);
		stm8_fd = -1;
		return false;
	} else {
		flog::info("Connected to GPS STM8");
	}

    // Connect to M8N
    m8n_fd = open(GPS_I2C_BUS, O_RDWR);
    if (m8n_fd < 0) {
        flog::error("Gps: failed to open the i2c bus");
		return false;
    }
	if (ioctl(m8n_fd, I2C_SLAVE, GPS_I2C_M8N_ADDRESS) < 0) {
		flog::error("Gps: failed to set GPS M8N I2C address");
		close(m8n_fd);
		m8n_fd = -1;
		return false;
	}
	if (read(m8n_fd, &test_byte, 1) < 0) {
		flog::warn("GPS M8N is not connected");
		close(m8n_fd);
		m8n_fd = -1;
		return false;
	} else {
		flog::info("Connected to GPS M8N");
	}

	pollerRunning = true;
    pollingThread = std::thread(&Gps::pollState, this);

	initialized = true;

	return true;
}

// Destructor
Gps::~Gps() {
    if (pollerRunning) {
        pollerRunning = false;
        if (pollingThread.joinable()) {
            pollingThread.join();
        }
    }
    if (stm8_fd >= 0) {
        close(stm8_fd);
    }
    stopReader();
    if (m8n_fd >= 0) {
        close(m8n_fd);
    }
    close(lock_fd);
}

uint8_t Gps::getUtcHour() {
    return utc_hour;
}

uint8_t Gps::getUtcMinute() {
    return utc_minute;
}

uint8_t Gps::getUtcSecond() {
    return utc_second;
}

int32_t Gps::getLatitudeMicrodegrees() {
    return latitude_md;
}

int32_t Gps::getLongitudeMicrodegrees() {
    return longitude_md;
}

double Gps::getLatitude() {
    return (double)latitude_md / 1000000.0f;
}

double Gps::getLongitude() {
    return (double)longitude_md / 1000000.0f;
}

uint8_t Gps::getFixQuality() {
    return fix_quality;
}

uint8_t Gps::getUsedSatellites() {
    return used_satellites;
}

int Gps::acquireLock() {
    if (flock(lock_fd, LOCK_EX) < 0) {
        flog::error("Failed to acquire GPS I2C lock");
        close(lock_fd);
        return -1;
    }
    return lock_fd;
}

void Gps::releaseLock() {
    flock(lock_fd, LOCK_UN);
}

void Gps::pollState() {
    while (pollerRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(GPS_POLL_INTERVAL_MS));

        readRegister(stm8_fd, GPS_I2C_GPS_UTC_HOUR, &utc_hour);
        readRegister(stm8_fd, GPS_I2C_GPS_UTC_MINUTE, &utc_minute);
        readRegister(stm8_fd, GPS_I2C_GPS_UTC_SECOND, &utc_second);

        readRegister(stm8_fd, GPS_I2C_GPS_FIX_QUALITY, &fix_quality);

        if (fix_quality != 0) {
            uint8_t lat_0, lat_1, lat_2, lat_3;
            if (readRegister(stm8_fd, GPS_I2C_GPS_LATITUDE_L, &lat_0)
                && readRegister(stm8_fd, GPS_I2C_GPS_LATITUDE_ML, &lat_1)
                && readRegister(stm8_fd, GPS_I2C_GPS_LATITUDE_MH, &lat_2)
                && readRegister(stm8_fd, GPS_I2C_GPS_LATITUDE_H, &lat_3)) {
                    latitude_md = (((int32_t)lat_0) | ((int32_t)lat_1 << 8) | ((int32_t)lat_2 << 16) | ((int32_t)lat_3 << 24));
            }
            uint8_t lon_0, lon_1, lon_2, lon_3;
            if (readRegister(stm8_fd, GPS_I2C_GPS_LONGITUDE_L, &lon_0)
                && readRegister(stm8_fd, GPS_I2C_GPS_LONGITUDE_ML, &lon_1)
                && readRegister(stm8_fd, GPS_I2C_GPS_LONGITUDE_MH, &lon_2)
                && readRegister(stm8_fd, GPS_I2C_GPS_LONGITUDE_H, &lon_3)) {
                    longitude_md = (((int32_t)lon_0) | ((int32_t)lon_1 << 8) | ((int32_t)lon_2 << 16) | ((int32_t)lon_3 << 24));
            }
        }

        readRegister(stm8_fd, GPS_I2C_GPS_USED_SATELLITES, &used_satellites);
    }
}

bool Gps::sendData(unsigned char * data, uint8_t size) {
	if (!initialized && !initialize()) {
		flog::warn("sendData: GPS STM8 is not connected yet");
		return false;
	}
    if (acquireLock() < 0) {
        flog::warn("sendData: failed to lock GPS I2C device");
        return false;
    }
    uint8_t reg = GPS_I2C_TO_UART;
    if (write(stm8_fd, &reg, 1) != 1) {
        flog::error("sendData: failed to write register index");
        close(stm8_fd);
        releaseLock();
        return false;
    }
    if (write(stm8_fd, &size, 1) != 1) {
        flog::error("sendData: failed to write data length");
        close(stm8_fd);
        releaseLock();
        return false;
    }
    if (write(stm8_fd, data, size) != size) {
        flog::error("sendData: failed to write data");
        close(stm8_fd);
        releaseLock();
        return false;
    }
    releaseLock();
    return true;
}

int Gps::readLine(unsigned char * line) {
	if (!initialized && !initialize()) {
		flog::warn("readLine: GPS STM8 is not connected yet");
		return false;
	}
    if (acquireLock() < 0) {
        flog::warn("readLine: failed to lock GPS I2C device");
        return -1;
    }
    uint8_t reg = GPS_I2C_FROM_UART;
    int pos = 0;
    while (true) {
        if (write(stm8_fd, &reg, 1) != 1) {
            flog::error("Failed to write to the i2c bus");
            return 1;
        }
        unsigned char data;
        if (read(stm8_fd, &data, 1) != 1) {
            flog::error("Failed to read from the i2c bus");
            return 1;
        }
        if (data != 0xFF) {
            line[pos ++] = data;
            if (data == 0x0A) {
                break;
            }
        } else {
            // 0xFF means no data for now, retry later
            std::this_thread::sleep_for(std::chrono::milliseconds(GPS_READ_INTERVAL_MS));
        }
    }
    releaseLock();
    return pos;
}

std::vector<Satellite> Gps::getSatellites() {
    return satellites;
}

unsigned char Gps::calculateNmeaChecksum(const std::string& sentence) {
    unsigned char checksum = 0;
    size_t i = 1;   // skip '$' at the beginning
    while (i < sentence.length() && sentence[i] != '*') {
        checksum ^= sentence[i];
        i++;
    }
    return checksum;
}

bool Gps::verifyNmeaChecksum(const std::string& sentence) {
    size_t asterisk_pos = sentence.find('*');
    if (asterisk_pos == std::string::npos || asterisk_pos + 3 > sentence.length()) {
        return false;
    }
    std::string checksum_str = sentence.substr(asterisk_pos + 1, 2);
    if (!std::isxdigit(checksum_str[0]) || !std::isxdigit(checksum_str[1])) {
        return false;
    }
    unsigned char expected_checksum = static_cast<unsigned char>(std::strtol(checksum_str.c_str(), nullptr, 16));
    unsigned char calculated_checksum = calculateNmeaChecksum(sentence);
    return (expected_checksum == calculated_checksum);
}

void Gps::parseGSV(const std::string& sentence) {
    std::vector<std::string> fields;
    int field_count = splitFieldsPreserveEmpty(sentence, fields);

    if (field_count < 4) return;

    std::lock_guard<std::mutex> lock(dataMutex);
    auto current_time = std::chrono::steady_clock::now();

    int total_sats = std::atoi(fields[3].c_str());
    int sats_in_msg = (total_sats > 4) ? 4 : total_sats;

    // extract information for each satellite
    for (int i = 0; i < sats_in_msg && i < 4; i++) {
        int prn_idx = 4 + i * 4;
        int cno_idx = 7 + i * 4;

        if (prn_idx >= field_count || fields[prn_idx].empty()) break;

        int prn = std::atoi(fields[prn_idx].c_str());
        if (prn <= 0) continue;

        int cno = (cno_idx < field_count && !fields[cno_idx].empty()) ? std::atoi(fields[cno_idx].c_str()) : -1;

        // find or create satellite record
        auto it = std::find_if(satellites.begin(), satellites.end(), [prn](const Satellite& s) { return s.prn == prn; });
        if (it == satellites.end() && satellites.size() < MAX_SATELLITES) {
            Satellite new_sat;
            new_sat.prn = prn;
            new_sat.system = getSatelliteSystem(prn);
            new_sat.cno = cno;
            new_sat.valid = (cno > 0);
            new_sat.last_update = current_time;
            satellites.push_back(new_sat);
        } else if (it != satellites.end()) {
            it->cno = cno;
            it->valid = (cno > 0);
            it->last_update = current_time;
        }
    }
}

int Gps::splitFieldsPreserveEmpty(const std::string& sentence, std::vector<std::string>& fields) {
    fields.clear();
    if (sentence.empty()) return 0;
    size_t start = (sentence[0] == '$') ? 1 : 0;
    size_t star_pos = sentence.find('*', start);
    size_t end = (star_pos == std::string::npos) ? sentence.length() : star_pos;
    size_t pos = start;
    while (pos <= end) {
        size_t next_comma = sentence.find(',', pos);
        if (next_comma == std::string::npos || next_comma > end) {
            next_comma = end;
        }
        fields.emplace_back(sentence.substr(pos, next_comma - pos));
        if (next_comma == end) break;
        pos = next_comma + 1;
    }
    return static_cast<int>(fields.size());
}

void Gps::cleanupExpiredSatellites() {
    std::lock_guard<std::mutex> lock(dataMutex);
    auto current_time = std::chrono::steady_clock::now();
    satellites.erase(
        std::remove_if(satellites.begin(), satellites.end(),
                      [current_time](const Satellite& sat) {
                          auto age = std::chrono::duration_cast<std::chrono::seconds>(
                              current_time - sat.last_update).count();
                          return age > SATELLITE_TIMEOUT;
                      }),
        satellites.end()
    );
}

std::string Gps::getSatelliteSystem(int prn) {
    if (prn >= 1 && prn <= 32) {
        return "GPS";
    } else if (prn >= 33 && prn <= 64) {
        return "SBAS";
    } else if (prn >= 65 && prn <= 96) {
        return "GLONASS";
    } else if (prn >= 120 && prn <= 158) {
        return "BEIDOU";
    } else if (prn >= 159 && prn <= 163) {
        return "BEIDOU";
    } else if (prn >= 193 && prn <= 197) {
        return "QZSS";
    } else if (prn >= 201 && prn <= 237) {
        return "GALILEO";
    } else if (prn >= 301 && prn <= 337) {
        return "GALILEO";
    } else {
        return "UNKNOWN";
    }
}

void Gps::registerNmeaCallback(std::function<void(const std::string&)> callback) {
    nmeaReceivedCallbacks.push_back(callback);
}

bool Gps::startReader() {
    if (m8n_fd < 0) {
        flog::error("M8N device not initialized yet.");
        return false;
    }
    if (readerRunning.load()) {
        flog::warn("M8N reader is already running.");
        return false;
    }
    readerRunning = true;
    readingThread = std::thread(&Gps::readerLoop, this);
    flog::info("M8N reader started.");
    return true;
}

void Gps::stopReader() {
    if (readerRunning.load()) {
        readerRunning = false;
        if (readingThread.joinable()) {
            readingThread.join();
        }
        flog::info("M8N reader stopped.");
    }
}

bool Gps::readRegister(int fd, uint8_t reg, uint8_t* data) {
    if (fd < 0) {
        flog::warn("readRegister: invalid fd");
        return false;
    }
    if (acquireLock() < 0) {
        flog::warn("readRegister: failed to lock GPS I2C device");
        return false;
    }
    if (write(fd, &reg, 1) != 1) {
        flog::warn("readRegister: failed to write register address");
        releaseLock();
        return false;
    }
    if (read(fd, data, 1) != 1) {
        flog::warn("readRegister: failed to read data");
        releaseLock();
        return false;
    }
    releaseLock();
    return true;
}

int Gps::getAvailableBytes() {
    uint8_t high_byte, low_byte;
    if (readRegister(m8n_fd, DATA_LENGTH_HIGH_REG, &high_byte) < 0) {
        return -1;
    }
    if (readRegister(m8n_fd, DATA_LENGTH_LOW_REG, &low_byte) < 0) {
        return -1;
    }
    int available = (high_byte << 8) | low_byte;
    if (available == NO_DATA_VALUE) {
        return 0;
    }
    return available;
}

int Gps::readDataStream(uint8_t* buffer, int count) {
    if (acquireLock() < 0) {
        flog::warn("readDataStream: failed to lock GPS I2C device");
        return -1;
    }
    uint8_t reg = DATA_STREAM_REG;
    if (write(m8n_fd, &reg, 1) != 1) {
        flog::warn("readDataStream: failed to set address");
        releaseLock();
        return -1;
    }
    int bytes_read = read(m8n_fd, buffer, count);
    if (bytes_read < 0) {
        flog::warn("readDataStream: failed to read data stream");
        releaseLock();
        return -1;
    }
    releaseLock();
    return bytes_read;
}

void Gps::processNmeaBuffer() {
    std::lock_guard<std::mutex> lock(bufferMutex);

    while (true) {
        // Find start of NMEA sentence ($ character)
        auto start_it = std::find(readerBuffer.begin(), readerBuffer.begin() + readerBufferLength, '$');
        // No NMEA start found, clear buffer
        if (start_it == readerBuffer.begin() + readerBufferLength) {
            readerBufferLength = 0;
            return;
        }
        // Remove data before NMEA start
        size_t start_pos = std::distance(readerBuffer.begin(), start_it);
        if (start_pos > 0) {
            std::memmove(readerBuffer.data(), readerBuffer.data() + start_pos,
                       readerBufferLength - start_pos);
            readerBufferLength -= start_pos;
        }
        // Find end of NMEA sentence (\r or \n)
        auto end_it = std::find_if(readerBuffer.begin() + 1,
                                 readerBuffer.begin() + readerBufferLength,
                                 [](uint8_t c) { return c == '\r' || c == '\n'; });
        // No complete sentence found yet
        if (end_it == readerBuffer.begin() + readerBufferLength) {
            return;
        }
        // Extract complete NMEA sentence
        size_t sentence_end = std::distance(readerBuffer.begin(), end_it);
        std::string nmea_sentence(readerBuffer.begin(), readerBuffer.begin() + sentence_end);

        // Print NMEA sentence
        //std::cout << "NMEA: " << nmea_sentence << "\n";

        // Process GSV
        if (nmea_sentence[0] == '$') {
            if (verifyNmeaChecksum(nmea_sentence)) {
                processedNMEA ++;
                if (nmea_sentence.find("GSV") != std::string::npos) {
                    parseGSV(nmea_sentence);
                }
            }
        }

        // Call callback if registered
        for (const auto& callback : nmeaReceivedCallbacks) {
            if (callback) {
                callback(nmea_sentence);
            }
        }
        // Remove processed sentence from buffer
        size_t sentence_len = sentence_end + 1;
        // Skip additional \r or \n characters
        while (sentence_len < readerBufferLength &&
               (readerBuffer[sentence_len] == '\r' || readerBuffer[sentence_len] == '\n')) {
            sentence_len++;
        }

        std::memmove(readerBuffer.data(), readerBuffer.data() + sentence_len, readerBufferLength - sentence_len);
        readerBufferLength -= sentence_len;
    }
}

void Gps::readerLoop() {
    using namespace std::chrono;
    auto last_cleanup = steady_clock::now();
    while (readerRunning.load()) {
        try {
            int available_bytes = getAvailableBytes();
            if (available_bytes <= 0) {
                std::this_thread::sleep_for(milliseconds(50));
                continue;
            }
            // Avoid overflowing buffer
            int bytes_to_read = available_bytes;
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                if (readerBufferLength + bytes_to_read > BUFFER_SIZE - 1) {
                    bytes_to_read = BUFFER_SIZE - 1 - readerBufferLength;
                    if (bytes_to_read <= 0) {
                        std::cout << "Buffer full, resetting...\n";
                        readerBufferLength = 0;
                        continue;
                    }
                }
            }
            // Read available data
            std::vector<uint8_t> temp_buffer(bytes_to_read);
            int bytes_read = readDataStream(temp_buffer.data(), bytes_to_read);

            if (bytes_read > 0) {
                {
                    std::lock_guard<std::mutex> lock(bufferMutex);
                    std::memcpy(readerBuffer.data() + readerBufferLength,
                              temp_buffer.data(), bytes_read);
                    readerBufferLength += bytes_read;
                }
                processNmeaBuffer();
            } else {
                std::this_thread::sleep_for(milliseconds(10));
            }
            // Cleanup expired satellites
            auto current_time = steady_clock::now();
            if (duration_cast<seconds>(current_time - last_cleanup).count() >= 5) {
                cleanupExpiredSatellites();
                last_cleanup = current_time;
            }
            std::this_thread::sleep_for(milliseconds(1));
        } catch (const std::exception& e) {
            std::cerr << "Exception in reader loop: " << e.what() << "\n";
            std::this_thread::sleep_for(milliseconds(100));
        }
    }
}

int32_t Gps::getProcessedNMEA() {
    return processedNMEA;
}

void Gps::outputReferenceClock(bool lockToGpsFreq) {
    sendData((unsigned char *)(lockToGpsFreq ? UBX_CFG_TP5 : UBX_CFG_TP5_NO_SYNC), UBX_CFG_TP5_SIZE);
}