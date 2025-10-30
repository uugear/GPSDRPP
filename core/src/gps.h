#ifndef GPS_H
#define GPS_H

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <functional>
#include <string>
#include <chrono>

/*
 * This abstractive GPS is implemented with a NEO M8N module and an STM8S003F3
 * micro-controller. The STM8 emulates an I2C slave device, which allows 
 * interaction with the M8N module via I2C interface.
 */

#define GPS_POLL_INTERVAL_MS        1000
#define GPS_READ_INTERVAL_MS        1000

#define GPS_I2C_LOCK                "/var/lock/vu_gpsdr_gps_i2c.lock"
#define GPS_I2C_BUS                 "/dev/i2c-2"
#define GPS_I2C_STM8_ADDRESS        0x0B
#define GPS_I2C_M8N_ADDRESS         0x42

#define MAX_SATELLITES 64
#define MAX_LINE_LEN 256
#define SATELLITE_TIMEOUT 15

#define GPS_I2C_FIRMWARE_VERSION    0
#define GPS_I2C_FROM_UART           1
#define GPS_I2C_TO_UART             2
#define GPS_I2C_GPS_UTC_HOUR        3
#define GPS_I2C_GPS_UTC_MINUTE      4
#define GPS_I2C_GPS_UTC_SECOND      5
#define GPS_I2C_GPS_LATITUDE_L      6
#define GPS_I2C_GPS_LATITUDE_ML     7
#define GPS_I2C_GPS_LATITUDE_MH     8
#define GPS_I2C_GPS_LATITUDE_H      9
#define GPS_I2C_GPS_LONGITUDE_L     10
#define GPS_I2C_GPS_LONGITUDE_ML    11
#define GPS_I2C_GPS_LONGITUDE_MH    12
#define GPS_I2C_GPS_LONGITUDE_H     13
#define GPS_I2C_GPS_FIX_QUALITY     14
#define GPS_I2C_GPS_USED_SATELLITES 15


typedef struct {
    int prn;
    int cno;
    int valid;
    std::string system;
    std::chrono::steady_clock::time_point last_update;
} Satellite;

class Gps {
public:
    Gps();
    ~Gps();
	
    bool initialize();
    
    static constexpr uint8_t UBX_CFG_TP5_SIZE = 40;
    static constexpr unsigned char UBX_CFG_TP5[40] = {
        0xB5, 0x62, 0x06, 0x31, 0x20, 0x00, 0x00, 0x01, 
        0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x36, 
        0x6E, 0x01, 0x00, 0x36, 0x6E, 0x01, 0x00, 0x00, 
        0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 
        0x00, 0x00, 0x6F, 0x00, 0x00, 0x00, 0x43, 0x50
    };
    static constexpr unsigned char UBX_CFG_TP5_NO_SYNC[40] = {
        0xB5, 0x62, 0x06, 0x31, 0x20, 0x00, 0x00, 0x01, 
        0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x36,
        0x6E, 0x01, 0x00, 0x36, 0x6E, 0x01, 0x00, 0x00, 
        0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x6D, 0x00, 0x00, 0x00, 0xC1, 0xC8
    };

    uint8_t getUtcHour();
    uint8_t getUtcMinute();
    uint8_t getUtcSecond();
    
    int32_t getLatitudeMicrodegrees();
    int32_t getLongitudeMicrodegrees();
    double getLatitude();
    double getLongitude(); 
    uint8_t getFixQuality();
    uint8_t getUsedSatellites();
    
    void outputReferenceClock(bool lockToGpsFreq);
    
    int32_t processedNMEA = 0;
    std::vector<Satellite> getSatellites();
    
    bool sendData(unsigned char * data, uint8_t size);
    int readLine(unsigned char * line);
	
	void registerNmeaCallback(std::function<void(const std::string&)> callback);
    bool startReader();
    void stopReader();
    int32_t getProcessedNMEA();
	
private:
    bool initialized = false;

    int lock_fd = -1;
    int stm8_fd = -1;
    std::thread pollingThread;
    std::atomic<bool> pollerRunning{false};
    
    uint8_t utc_hour;     // UTC hour
    uint8_t utc_minute;   // UTC minute
    uint8_t utc_second;   // UTC second
    int32_t latitude_md;  // Latitude in microdegrees (1e-6 degrees)
    int32_t longitude_md; // Longitude in microdegrees (1e-6 degrees)
    uint8_t fix_quality;  // Fix quality
    uint8_t used_satellites;   // Used satellites
    
    bool readRegister(int fd, uint8_t reg, uint8_t* data);
    
    int acquireLock();
	void releaseLock();
	void pollState();
	
	std::vector<Satellite> satellites;
	std::mutex dataMutex;

    static constexpr int DATA_STREAM_REG = 0xFF;
    static constexpr int DATA_LENGTH_HIGH_REG = 0xFD;
    static constexpr int DATA_LENGTH_LOW_REG = 0xFE;
    static constexpr int NO_DATA_VALUE = 0xFFFF;
    static constexpr int BUFFER_SIZE = 2048;
    int m8n_fd = -1;
    std::thread readingThread;
    std::atomic<bool> readerRunning{false};
    std::vector<uint8_t> readerBuffer;
    size_t readerBufferLength = 0;
    std::mutex bufferMutex;
    std::vector<std::function<void(const std::string&)>> nmeaReceivedCallbacks;
    
    int getAvailableBytes();
    int readDataStream(uint8_t* buffer, int count);
    void processNmeaBuffer();
    void readerLoop();

    unsigned char calculateNmeaChecksum(const std::string& sentence);
    bool verifyNmeaChecksum(const std::string& sentence);
    void parseGSV(const std::string& sentence);
    int splitFieldsPreserveEmpty(const std::string& sentence, std::vector<std::string>& fields);
    void cleanupExpiredSatellites();
    std::string getSatelliteSystem(int prn);
};

#endif // GPS_H
