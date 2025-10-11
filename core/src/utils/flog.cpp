#include "flog.h"
#include <mutex>
#include <chrono>
#include <string.h>
#include <inttypes.h>

#define FORMAT_BUF_SIZE 16
#define ESCAPE_CHAR     '\\'

namespace flog {
    std::mutex outMtx;

    const char* TYPE_STR[_TYPE_COUNT] = {
        "DEBUG",
        "INFO",
        "WARN",
        "ERROR"
    };

#define COLOR_WHITE "\x1B[0m"
    const char* TYPE_COLORS[_TYPE_COUNT] = {
        "\x1B[36m",
        "\x1B[32m",
        "\x1B[33m",
        "\x1B[31m",
    };

    void __log__(Type type, const char* fmt, const std::vector<std::string>& args) {
        // Reserve a buffer for the final output
        int argCount = args.size();
        int fmtLen = strlen(fmt) + 1;
        int totSize = fmtLen;
        for (const auto& a : args) { totSize += a.size(); }
        std::string out;
        out.reserve(totSize);
        
        // Get output stream depending on type
        FILE* outStream = (type == TYPE_ERROR) ? stderr : stdout;

        // Parse format string
        bool escaped = false;
        int formatCounter = 0;
        bool inFormat = false;
        int formatLen = 0;
        char formatBuf[FORMAT_BUF_SIZE+1];
        for (int i = 0; i < fmtLen; i++) {
            // Get char
            const char c = fmt[i];

            // If this character is escaped, don't try to parse it
            if (escaped) {
                escaped = false;
                out += c;
                continue;
            }

            // State machine
            if (!inFormat && c != '{') {
                // Write to formatted output if not escape character
                if (c == ESCAPE_CHAR) {
                    escaped = true;
                }
                else {
                    out += c;
                }
            }
            else if (!inFormat) {
                // Start format mode
                inFormat = true;
            }
            else if (c == '}') {
                // Stop format mode
                inFormat = false;

                // Insert string value or error
                if (!formatLen) {
                    // Use format counter as ID if available or print wrong format string
                    if (formatCounter < argCount) {
                        out += args[formatCounter++];
                    }
                    else {
                        out += "{}";
                    }
                }
                else {
                    // Parse number
                    formatBuf[formatLen] = 0;
                    formatCounter = std::atoi(formatBuf);

                    // Use ID if available or print wrong format string
                    if (formatCounter < argCount) {
                        out += args[formatCounter];
                    }
                    else {
                        out += '{';
                        out += formatBuf;
                        out += '}';
                    }

                    // Increment format counter
                    formatCounter++;
                }

                // Reset format counter
                formatLen = 0;
            }
            else {
                // Add to format buffer 
                if (formatLen < FORMAT_BUF_SIZE) { formatBuf[formatLen++] = c; }
            }
        }

        // Get time
        auto now = std::chrono::system_clock::now();
        auto nowt = std::chrono::system_clock::to_time_t(now);
        auto nowc = std::localtime(&nowt); // TODO: This is not threadsafe

        // Write to output
        {
            std::lock_guard<std::mutex> lck(outMtx);
            // Print format string
            fprintf(outStream, COLOR_WHITE "[%02d/%02d/%02d %02d:%02d:%02d.%03d] [%s%s" COLOR_WHITE "] %s\n",
                    nowc->tm_mday, nowc->tm_mon + 1, nowc->tm_year + 1900, nowc->tm_hour, nowc->tm_min, nowc->tm_sec, 0, TYPE_COLORS[type], TYPE_STR[type], out.c_str());
        }
    }

    std::string __toString__(bool value) {
        return value ? "true" : "false";
    }

    std::string __toString__(char value) {
        return std::string("")+value;
    }

    std::string __toString__(int8_t value) {
        char buf[8];
        sprintf(buf, "%" PRId8, value);
        return buf;
    }

    std::string __toString__(int16_t value) {
        char buf[16];
        sprintf(buf, "%" PRId16, value);
        return buf;
    }

    std::string __toString__(int32_t value) {
        char buf[32];
        sprintf(buf, "%" PRId32, value);
        return buf;
    }

    std::string __toString__(int64_t value) {
        char buf[64];
        sprintf(buf, "%" PRId64, value);
        return buf;
    }

    std::string __toString__(uint8_t value) {
        char buf[8];
        sprintf(buf, "%" PRIu8, value);
        return buf;
    }

    std::string __toString__(uint16_t value) {
        char buf[16];
        sprintf(buf, "%" PRIu16, value);
        return buf;
    }

    std::string __toString__(uint32_t value) {
        char buf[32];
        sprintf(buf, "%" PRIu32, value);
        return buf;
    }

    std::string __toString__(uint64_t value) {
        char buf[64];
        sprintf(buf, "%" PRIu64, value);
        return buf;
    }

    std::string __toString__(float value) {
        char buf[256];
        sprintf(buf, "%f", value);
        return buf;
    }

    std::string __toString__(double value) {
        char buf[256];
        sprintf(buf, "%lf", value);
        return buf;
    }

    std::string __toString__(const char* value) {
        return value;
    }

    std::string __toString__(const void* value) {
        char buf[32];
        sprintf(buf, "0x%p", value);
        return buf;
    }
}