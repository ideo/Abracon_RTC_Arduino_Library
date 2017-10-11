#ifndef ABRACONRTC_H_
#define ABRACONRTC_H_

#include <inttypes.h>

// RTC register addresses
#define RTC_ADDR   0x56 // 7 bit, without least sig R/W bit
#define SEC_ADDR   0x08
#define MIN_ADDR   0x09
#define HOUR_ADDR  0x0A
#define TEMP_ADDR  0x20

struct RTCData {
	bool 	hrFormat; // true for 12-hour format, false for 24-hour format
	bool	timeOfDay; // if 12-hour format, true for PM, false for AM
	uint8_t hour10s;
	uint8_t hour1s;
	uint8_t min10s;
	uint8_t min1s;
	uint8_t sec10s;
	uint8_t sec1s;
	uint8_t tempF;
};

RTCData getRTCData();
bool toggleHrFormat();
bool incHour();
bool decHour();
bool incMinute();
bool decMinute();

#endif
