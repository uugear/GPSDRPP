// ==========================================================================
//
// Mode S Message Decoder
//
// Most of the content in this file comes from from:
// dump1090 by Salvatore Sanfilippo (https://github.com/antirez/dump1090)
//
// 2025, Dun Cat B.V (UUGear)
//
// ==========================================================================

/* Mode1090, a Mode S messages decoder for RTLSDR devices.
 *
 * Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MODE_S_DECODER_H
#define MODE_S_DECODER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODE_S_DEFAULT_RATE             2000000
#define MODE_S_DEFAULT_FREQ             1090000000

#define MODE_S_PREAMBLE_US              8       // microseconds
#define MODE_S_LONG_MSG_BITS            112
#define MODE_S_SHORT_MSG_BITS           56
#define MODE_S_FULL_LEN                 (MODE_S_PREAMBLE_US+MODE_S_LONG_MSG_BITS)
#define MODE_S_LONG_MSG_BYTES           (112/8)
#define MODE_S_SHORT_MSG_BYTES          (56/8)

#define MODE_S_ICAO_CACHE_LEN           1024    // Power of two required.
#define MODE_S_ICAO_CACHE_TTL           60      // Time to live of cached addresses.
#define MODE_S_UNIT_FEET                0
#define MODE_S_UNIT_METERS              1

#define MODE_S_DEBUG_DEMOD              (1<<0)
#define MODE_S_DEBUG_DEMODERR           (1<<1)
#define MODE_S_DEBUG_BADCRC             (1<<2)
#define MODE_S_DEBUG_GOODCRC            (1<<3)
#define MODE_S_DEBUG_NOPREAMBLE         (1<<4)

// When debug is set to MODES_DEBUG_NOPREAMBLE, the first sample must be at
// least greater than a given level for us to dump the signal.
#define MODE_S_DEBUG_NOPREAMBLE_LEVEL   25

#define MODES_S_AIRCRAFT_INFO_TTL       60      //TTL for aircraft information before being removed


// Structure used to describe an aircraft in iteractive mode.
struct aircraft {
    uint32_t addr;      // ICAO address
    char hexaddr[7];    // Printable ICAO address
    char flight[9];     // Flight number
    int altitude;       // Altitude
    int speed;          // Velocity computed from EW and NS components.
    int track;          // Angle of flight.
    time_t seen;        // Time at which the last packet was received.
    long messages;      // Number of Mode S messages received.

    // Encoded latitude and longitude as extracted by odd and even CPR encoded messages.
    int odd_cprlat;
    int odd_cprlon;
    int even_cprlat;
    int even_cprlon;
    double lat, lon;    // Coordinated obtained from CPR encoded data.
    long long odd_cprtime, even_cprtime;
    
    bool highlighted;       // Whether it is highlighted
    struct aircraft *next;  // Next aircraft in our linked list.
};

// The struct we use to store information about a decoded message.
struct mode_s_message {
    // Generic fields
    unsigned char msg[MODE_S_LONG_MSG_BYTES]; // Binary message.
    int msgbits;                // Number of bits in message
    int msgtype;                // Downlink format #
    int crcok;                  // True if CRC was valid
    uint32_t crc;               // Message CRC
    int errorbit;               // Bit corrected. -1 if no bit corrected.
    int aa1, aa2, aa3;          // ICAO Address bytes 1 2 and 3
    int phase_corrected;        // True if phase correction was applied.

    // DF 11
    int ca;                     // Responder capabilities.

    // DF 17
    int metype;                 // Extended squitter message type.
    int mesub;                  // Extended squitter message subtype.
    int heading_is_valid;
    int heading;
    int aircraft_type;
    int fflag;                  // 1 = Odd, 0 = Even CPR message.
    int tflag;                  // UTC synchronized?
    int raw_latitude;           // Non decoded latitude
    int raw_longitude;          // Non decoded longitude
    char flight[9];             // 8 chars flight number.
    int ew_dir;                 // 0 = East, 1 = West.
    int ew_velocity;            // E/W velocity.
    int ns_dir;                 // 0 = North, 1 = South.
    int ns_velocity;            // N/S velocity.
    int vert_rate_source;       // Vertical rate source.
    int vert_rate_sign;         // Vertical rate sign.
    int vert_rate;              // Vertical rate.
    int velocity;               // Computed from EW and NS velocity.

    // DF4, DF5, DF20, DF21
    int fs;                     // Flight status for DF4,5,20,21
    int dr;                     // Request extraction of downlink request.
    int um;                     // Request extraction of downlink request.
    int identity;               // 13 bits identity (Squawk).

    // Fields used by multiple message types.
    int altitude;
    int unit;
};

typedef void (*ModeSHandler)(char *, struct mode_s_message *);

// Program global state.
struct mode_s_context {
    // Internal state
    uint32_t icao_cache[MODE_S_ICAO_CACHE_LEN * 2];    // Recently seen ICAO addresses cache. it's a addr / timestamp pair for every entry

    // Configuration
    int fix_errors;                 // Single bit error correction if true.
    int check_crc;                  // Only display messages with good CRC.
    int raw;                        // Raw output format.
    int debug;                      // Debugging mode.
    int onlyaddr;                   // Print only ICAO addresses.
    int aggressive;                 // Aggressive detection algorithm.

    // List of aircrafts
    struct aircraft *aircrafts;
    int aircraft_info_ttl;          // Aircraft informaation TTL before deletion.

    // Statistics
    long long stat_valid_preamble;
    long long stat_demodulated;
    long long stat_goodcrc;
    long long stat_badcrc;
    long long stat_fixed;
    long long stat_single_bit_fix;
    long long stat_two_bits_fix;
    long long stat_http_requests;
    long long stat_sbs_connections;
    long long stat_out_of_phase;
	
	// Output buffer
	char * buffer;
	size_t buffer_size;
	FILE * mem_stream;
	
	// Handler
	ModeSHandler handler;
};

/* Prepare the context and make it usable */
void prepareContext(struct mode_s_context * ctx, ModeSHandler handler);

/* Detect a Mode S messages inside the magnitude buffer pointed by 'm' and of
 * size 'mlen' bytes. Every detected Mode S message is convert it into a
 * stream of bits and passed to the function to display it. */
void detectModeS(struct mode_s_context * ctx, uint16_t *m, uint32_t mlen);

///* If we don't receive new nessages within MODES_S_AIRCRAFT_INFO_TTL seconds 
// * we remove the aircraft from the list. */
//void removeStaleAircrafts(struct mode_s_context * ctx);

#ifdef __cplusplus
}
#endif

#endif // MODE_S_DECODER_H