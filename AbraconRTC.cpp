#include <util/delay.h>
#include <Wire.h>
#include "AbraconRTC.h"

// Initialize Class Variables //////////////////////////////////////////////////

RTCData AbraRTC::AbraRTCData = {0};

// Constructors ////////////////////////////////////////////////////////////////

AbraRTC::AbraRTC()
{
}

// Private Methods /////////////////////////////////////////////////////////////

/*
  Description
    write RTC register 8 bit value
  Input
    addr: address of register to write
    val: value to write to register (0 to 255 or 0x00 to 0xFF)
  Return
    true if success, false if error
*/
static bool AbraRTC::writeRegister(uint8_t addr, uint8_t val) {
	Wire.beginTransmission(RTC_ADDR);
	if (!Wire.write(addr)) {return 0;} // send address of register to write to
	if (!Wire.write(val)) {return 0;} // send updated register value
	Wire.endTransmission();

	return 1;
}

/*
  Description
    select RTC register (likely for reading from)
  Input
    addr: address of register to select
  Return
    true if success, false if error
*/
static bool AbraRTC::selectRegister(uint8_t addr) {
	Wire.beginTransmission(RTC_ADDR);
	if (!Wire.write(addr)) {return 0;} // send address of register to select
	Wire.endTransmission();

	return 1;
}

/*
  Description
    read RTC register 8 bit value
  Input
    addr: address of register to read from
    readVal: value read from register
  Return
    true if success, false if error
*/
static bool AbraRTC::readRegister(uint8_t addr, uint8_t &readVal) {
	// select register to read from
	if (!selectRegister(addr)) { return 0; }

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for register
	if (Wire.available() == 1) { // make sure 1 byte was returned
		readVal = Wire.read();
		return 1;
	}

	return 0;
}

/*
  Description
    write a specific bit in an RTC register
  Input
    addr: address of register to write
    bitPosition: position of the bit to write to (0-7)
    val: bit value to write (0 or 1)
  Return
    true if success, false if error
*/
static bool AbraRTC::writeBit(uint8_t addr, uint8_t bitPosition, bool val) {
	uint8_t regVal = 0;
	if (!readRegister(addr, regVal)) { return 0; }

	if (val) {
		regVal |= (1 << bitPosition);
	} else {
		regVal &= ~(1 << bitPosition);
	}

	return writeRegister(addr, regVal);
}

/*
  Description
    switch 12-hour format to 24-hour format
  Input
    RTCHourTimeOfDay: true for PM, false for AM for 12 hour format
    RTCHourVal1s: one's value for the 12-hour hour digit (0-9)
    RTCHourVal10s: ten's value for the 12-hour hour digit (0-1)
  Return
    new value to load into the RTC hour register for 24-hour format
*/
static uint8_t AbraRTC::hour12to24(uint8_t RTCHourTimeOfDay, uint8_t RTCHourVal1s, uint8_t RTCHourVal10s) {

	bool	newHourFormat    = 0; // true for 12-hour, false for 24-hour
	uint8_t newHourVal1s     = RTCHourVal1s;
	uint8_t newHourVal10s    = RTCHourVal10s;

	if (RTCHourTimeOfDay) { // PM
		if (RTCHourVal10s == 0) {
			if (RTCHourVal1s < 8) {
				newHourVal1s  = RTCHourVal1s + 2;
				newHourVal10s = 1;
			} else {
				newHourVal1s  = RTCHourVal1s -8;
				newHourVal10s = 2;
			}
		} else if (RTCHourVal1s < 2) {
			newHourVal1s  = RTCHourVal1s + 2;
			newHourVal10s = 2;
		}
	} else if ((RTCHourVal1s == 2) && (RTCHourVal10s == 1)) { // midnight
		newHourVal1s  = 0;
		newHourVal10s = 0;
	}

	// mask any overflow (though there shouldn't be)
	newHourVal1s  	 &= 0x0F;
	newHourVal10s 	 &= 0x03;

	return (newHourVal1s | (newHourVal10s << 4) | (newHourFormat << 6));
}

/*
  Description
    switch 24-hour format to 12-hour format
  Input
    RTCHourVal1s: one's value for the 24-hour hour digit (0-9)
    RTCHourVal10s: ten's value for the 24-hour hour digit (0-2)
  Return
    new value to load into the RTC hour register for 24-hour format
*/
static uint8_t AbraRTC::hour24to12(uint8_t RTCHourVal1s, uint8_t RTCHourVal10s) {

	bool	newHourFormat    = 1; // true for 12-hour, false for 24-hour
	bool	newHourTimeOfDay = 0; // true for PM, false for AM
	uint8_t newHourVal1s     = RTCHourVal1s;
	uint8_t newHourVal10s    = RTCHourVal10s;

	// if ((RTCHourVal1s > 9) || (RTCHourVal10s > 2)) {
	// 	return 0;
	// }

	if (RTCHourVal10s == 1) {
		if (RTCHourVal1s > 2) {
			newHourTimeOfDay = 1;
			newHourVal1s     = RTCHourVal1s - 2;
			newHourVal10s    = 0;
		} else if (RTCHourVal1s == 2) {
			newHourTimeOfDay = 1;
		}
	} else if (RTCHourVal10s == 2) {
		newHourTimeOfDay = 1;
		if (RTCHourVal1s >= 2) {
			newHourVal1s  = RTCHourVal1s - 2;
			newHourVal10s = 1;
		} else {
			newHourVal1s  = RTCHourVal1s + 8;
			newHourVal10s = 0;
		}
	} else if (RTCHourVal1s == 0) { // 0 o'clock
		newHourVal1s     = 2;
		newHourVal10s    = 1;
		newHourTimeOfDay = 0;
	}

	// mask any overflow (though there shouldn't be)
	newHourVal1s  	 &= 0x0F;
	newHourVal10s 	 &= 0x01;

	return (newHourVal1s | (newHourVal10s << 4) | (newHourTimeOfDay << 5) | (newHourFormat << 6));

}

/*
  Description
    check control status register to see if EEPROM is busy
  Return
    true if busy, false if not busy
*/
static bool AbraRTC::checkEEPROMBusy() {
	uint8_t ctlStatRegVal = 0;
	if(!readRegister(CTL_STAT_ADDR, ctlStatRegVal)) { return 1; } // can't read EEPROM stat register, try again
	return ((ctlStatRegVal & 0x80) >> 7);
}

// Public Methods //////////////////////////////////////////////////////////////

/*
  Description
    begin RTC by checking if power was lost and enabling tricklecharge if so
  Return
	true if success, false if error
*/
bool AbraRTC::begin() {
	// clear PON flag if set
	uint8_t ctlStatRegVal = 0;
	if (!readRegister(CTL_STAT_ADDR, ctlStatRegVal)) { return 0; }
	uint8_t PONFlag = (ctlStatRegVal & 0x20) >> 5;

	if (PONFlag) {
		// update EEPROM control register / reset PON flag
		ctlStatRegVal &= 0xDF; // set PON flag to 0
		if (!writeRegister(CTL_STAT_ADDR, ctlStatRegVal)) { return 0; }

		// enable trickle charger
		if (!setTrickleCharge(1)) { return 0; }

		if (!setTime()) { return 0; }
	}

	if (!updateRTC()) { return 0; }

	return 1;
}

/*
  Description
    get current data (time and temp) from RTC
  Return
    true if success, false if error
*/
bool AbraRTC::updateRTC() {
	// select seconds register to begin reading from
	selectRegister(SEC_ADDR);

	Wire.requestFrom(RTC_ADDR, 3); // request 3 bytes for sec, min, hour
	if (Wire.available() == 3) { // make sure 3 bytes were returned
		uint8_t RTCSecVal  = Wire.read();
		AbraRTCData.sec1s   = RTCSecVal & 0x0F;
		AbraRTCData.sec10s  = (RTCSecVal >> 4) & 0x0F;

		uint8_t RTCMinVal  = Wire.read();
		AbraRTCData.min1s   = RTCMinVal & 0x0F;
		AbraRTCData.min10s  = (RTCMinVal >> 4) & 0x0F;

		uint8_t RTCHourVal = Wire.read();
		AbraRTCData.hrFormat  = (RTCHourVal >> 6) & 0x01;
		if (AbraRTCData.hrFormat) { // 12-hour format
			AbraRTCData.timeOfDay = (RTCHourVal >> 5) & 0x01;
			AbraRTCData.hour1s    = RTCHourVal & 0x0F;
			AbraRTCData.hour10s   = (RTCHourVal >> 4) & 0x01;
		} else { // 24-hour format
			AbraRTCData.hour1s    = RTCHourVal & 0x0F;
			AbraRTCData.hour10s   = (RTCHourVal >> 4) & 0x03;
		}
	} else { // something went wrong and we didn't get all 3 time registers
		return 0;
	}

	// get temperature
	// select seconds register to begin reading from
	selectRegister(TEMP_ADDR);

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for temperature
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCTempC = Wire.read() - 60;
		AbraRTCData.tempF  = RTCTempC * 1.8 + 32;
	} else {
		return 0;
	}

	return 1;
}

/*
  Description
    set the time of the RTC
    default call sets RTC to midnight
  Input
    hour: hour to set to (1-12 for 12-hour format, 0-23 for 24-hour format)
    min: minute to set to (0-59)
    sec: second to set to (0-59)
    PM: true for PM, false for AM
  Return
    true if success, false if error
*/
bool AbraRTC::setTime(uint8_t hour=0, uint8_t min=0, uint8_t sec=0, bool PM=0) {
	// select hour register to get 12/24-hr preference
	if (!selectRegister(HOUR_ADDR)) {
		return 0;
	}

	uint8_t newHourVal    = 0;
	uint8_t newMinVal     = 0;
	uint8_t newSecVal     = 0;

	if ((min < 60 && min >= 0) && (sec < 60 && sec >= 0)) {
		uint8_t RTCMinVal1s   = min % 10;
		uint8_t RTCMinVal10s  = (min - RTCMinVal1s)/10;
		uint8_t RTCSecVal1s   = sec % 10;
		uint8_t RTCSecVal10s  = (sec - RTCSecVal1s)/10;
		newMinVal = RTCMinVal1s | (RTCMinVal10s << 4);
		newSecVal = RTCSecVal1s | (RTCSecVal10s << 4);
	} else {
		return 0;
	}

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for hour
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCHourVal = Wire.read();
		bool RTCHourFormat = (RTCHourVal >> 6) & 0x01;
		if (RTCHourFormat) { // 12-hour mode
			if (hour <= 12 && hour >= 0) {
				if (hour == 0) {
					hour = 12;
				}
				uint8_t RTCHourVal1s  = hour % 10;
				uint8_t RTCHourVal10s = (hour - RTCHourVal1s)/10;
				bool RTCHourTimeOfDay = 0;
				if (PM) { // afternoon
					RTCHourTimeOfDay = 1;
				}
				newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourTimeOfDay << 5) | (RTCHourFormat << 6);
			} else {
				return 0;
			}
		} else { // 24-hour mode
			if (hour < 24 && hour >= 0) {
				if (PM) { // weird case where someone puts in 12-hour format but time is set to 24-hour format
					if ((hour + 12) < 24) {
						hour += 12;
					}
				}
				uint8_t RTCHourVal1s  = hour % 10;
				uint8_t RTCHourVal10s = (hour - RTCHourVal1s)/10;
				newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourFormat << 6);
			} else {
				return 0;
			}
		}
	} else {
		return 0;
	}

	Wire.beginTransmission(RTC_ADDR);
	if (Wire.write(SEC_ADDR)  != 1) {return 0;} // send address of register to write to
	if (Wire.write(newSecVal) != 1) {return 0;} // send updated register value
	if (Wire.write(newMinVal) != 1) {return 0;} // send updated register value
	if (Wire.write(newHourVal) != 1) {return 0;} // send updated register value
	Wire.endTransmission();

	return 1;
}

/*
  Description
    toggle clock between 12-hour format and 24-hour format
  Return
    true if success, false if error
*/
static bool AbraRTC::toggleHrFormat() {
	uint8_t RTCHourVal = 0;
	if (!readRegister(HOUR_ADDR, RTCHourVal)) { return 0; }

	bool	RTCHourFormat = (RTCHourVal >> 6) & 0x01; // true for 12-hour, false for 24-hour

	bool	RTCHourTimeOfDay = 0; // true for PM, false for AM
	uint8_t RTCHourVal1s     = 0;
	uint8_t RTCHourVal10s    = 0;
	uint8_t newHourVal 		 = 0;

	if (RTCHourFormat) {
		RTCHourTimeOfDay = (RTCHourVal >> 5) & 0x01;
		RTCHourVal1s     = RTCHourVal & 0x0F;
		RTCHourVal10s    = (RTCHourVal >> 4) & 0x01;
	} else {
		RTCHourVal1s     = RTCHourVal & 0x0F;
		RTCHourVal10s    = (RTCHourVal >> 4) & 0x03;
	}

	if (RTCHourFormat) { // 12-hour to 24-hour
		newHourVal = hour12to24(RTCHourTimeOfDay, RTCHourVal1s, RTCHourVal10s);
	} else { // 24-hour to 12-hour
		newHourVal = hour24to12(RTCHourVal1s, RTCHourVal10s);
	}

	// update hour register
	if (!writeRegister(HOUR_ADDR, newHourVal)) {
		return 0;
	}

	return 1;
}

/*
  Description
    set clock to 12-hour format to 24-hour format
  Input
    newHrFormat: true for 12-hour format, false for 24-hour format
  Return
    true if success, false if error
*/
bool AbraRTC::setHrFormat(bool newHrFormat) {
	uint8_t RTCHourVal = 0;
	if (!readRegister(HOUR_ADDR, RTCHourVal)) { return 0; }

	bool	RTCHourFormat = (RTCHourVal >> 6) & 0x01; // true for 12-hour, false for 24-hour

	if (RTCHourFormat == newHrFormat) {
		return 1; // nothing to change
	}

	bool	RTCHourTimeOfDay = 0; // true for PM, false for AM
	uint8_t RTCHourVal1s     = 0;
	uint8_t RTCHourVal10s    = 0;
	uint8_t newHourVal 		 = 0;

	if (RTCHourFormat) {
		RTCHourTimeOfDay = (RTCHourVal >> 5) & 0x01;
		RTCHourVal1s     = RTCHourVal & 0x0F;
		RTCHourVal10s    = (RTCHourVal >> 4) & 0x01;
	} else {
		RTCHourVal1s     = RTCHourVal & 0x0F;
		RTCHourVal10s    = (RTCHourVal >> 4) & 0x03;
	}

	// reformat hour data for set format
	if (newHrFormat) { // 24-hour to 12-hour
		newHourVal = hour24to12(RTCHourVal1s, RTCHourVal10s);
	} else { // 12-hour to 24-hour
		newHourVal = hour12to24(RTCHourTimeOfDay, RTCHourVal1s, RTCHourVal10s);
	}

	// update hour register
	if (!writeRegister(HOUR_ADDR, newHourVal)) {
		return 0;
	}

	return 1;
}

/*
  Description
    increment RTC hour
  Return
    true if success, false if error
*/
static bool AbraRTC::incHour() {
	// get current hour
	uint8_t RTCHourVal = 0;
	if (!readRegister(HOUR_ADDR, RTCHourVal)) { return 0; }

	bool	RTCHourFormat = (RTCHourVal >> 6) & 0x01; // true for 12-hour, false for 24-hour
	bool	RTCHourTimeOfDay = 0; // true for PM, false for AM
	uint8_t RTCHourVal1s     = 0;
	uint8_t RTCHourVal10s    = 0;

	if (RTCHourFormat) {
		RTCHourTimeOfDay = (RTCHourVal >> 5) & 0x01;
		RTCHourVal1s     = RTCHourVal & 0x0F;
		RTCHourVal10s    = (RTCHourVal >> 4) & 0x01;
	} else {
		RTCHourVal1s     = RTCHourVal & 0x0F;
		RTCHourVal10s    = (RTCHourVal >> 4) & 0x03;
	}

	// increment hour
	uint8_t newHourVal = 0;
	if (RTCHourFormat) { // 12-hour format
		if (RTCHourVal10s == 1) {
			if (RTCHourVal1s == 2) { // 12 o'clock
				// reset to 1 o'clock
				RTCHourVal1s     = 1;
				RTCHourVal10s    = 0;
			} else {
				RTCHourVal1s++;
				if (RTCHourVal1s == 2) {
					RTCHourTimeOfDay = !RTCHourTimeOfDay;
				}
			}
		} else if (RTCHourVal1s >= 9) {
			// 10 o'clock
			RTCHourVal1s  = 0;
			RTCHourVal10s = 1;
		} else {
			RTCHourVal1s++;
		}

		// mask any overflow (though there shouldn't be)
		RTCHourVal1s  	 &= 0x0F;
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourTimeOfDay << 5) | (RTCHourFormat << 6);

	} else { // 24-hour format
		if (RTCHourVal10s == 2) {
			if (RTCHourVal1s >= 3) { // 23 o'clock or greater
				// reset to 0 o'clock
				RTCHourVal1s  = 0;
				RTCHourVal10s = 0;
			} else {
				RTCHourVal1s++;
			}
		} else if (RTCHourVal1s >= 9) {
			RTCHourVal1s  = 0;
			RTCHourVal10s++;
		} else {
			RTCHourVal1s++;
		}
		
		// mask any overflow (though there shouldn't be)
		RTCHourVal1s  	 &= 0x0F;
		RTCHourVal10s 	 &= 0x03;
		newHourVal = (RTCHourVal1s & 0x0F) | (RTCHourVal10s << 4) | (RTCHourFormat << 6);
	}

	// update hour register
	if (!writeRegister(HOUR_ADDR, newHourVal)) {
		return 0;
	}

	return 1;
}

/*
  Description
    decrement RTC hour
  Return
    true if success, false if error
*/
static bool AbraRTC::decHour() {
	// get hour register data
	if (!selectRegister(HOUR_ADDR)) {
		return 0;
	}

	bool	RTCHourFormat    = 0; // true for 12-hour, false for 24-hour
	bool	RTCHourTimeOfDay = 0; // true for PM, false for AM
	uint8_t RTCHourVal1s     = 0;
	uint8_t RTCHourVal10s    = 0;

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for hour
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCHourVal = Wire.read();
		RTCHourFormat = (RTCHourVal >> 6) & 0x01;
		if (RTCHourFormat) {
			RTCHourTimeOfDay = (RTCHourVal >> 5) & 0x01;
			RTCHourVal1s     = RTCHourVal & 0x0F;
			RTCHourVal10s    = (RTCHourVal >> 4) & 0x01;
		} else {
			RTCHourVal1s     = RTCHourVal & 0x0F;
			RTCHourVal10s    = (RTCHourVal >> 4) & 0x03;
		}
	} else {
		return 0;
	}

	// decrement hour
	uint8_t newHourVal = 0;
	if (RTCHourFormat) { // 12-hour format
		if (RTCHourVal10s) {
			if (RTCHourVal1s == 0) { // 10 o'clock
				// 9 o'clock
				RTCHourVal1s  = 9;
				RTCHourVal10s = 0;
			} else {
				RTCHourVal1s--;
				if (RTCHourVal1s == 1) {
					RTCHourTimeOfDay = !RTCHourTimeOfDay;
				}
			}
		} else if (RTCHourVal1s == 1) {
			// 12 o'clock
			RTCHourVal1s  = 2;
			RTCHourVal10s = 1;
		} else {
			RTCHourVal1s--;
		}

		// mask any overflow (though there shouldn't be)
		RTCHourVal1s  	 &= 0x0F;
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourTimeOfDay << 5) | (RTCHourFormat << 6);

	} else {
		if (RTCHourVal10s > 0) {
			if (RTCHourVal1s == 0) {
				RTCHourVal1s = 9;
				RTCHourVal10s--;
			} else {
				RTCHourVal1s--;
			}
		} else {
			if (RTCHourVal1s == 0) {
				RTCHourVal1s  = 3;
				RTCHourVal10s = 2;
			} else {
				RTCHourVal1s--;
			}
		}

		// mask any overflow (though there shouldn't be)
		RTCHourVal1s  	 &= 0x0F;
		RTCHourVal10s 	 &= 0x03;
		newHourVal = RTCHourVal1s | (RTCHourVal10s << 4) | (RTCHourFormat << 6);
	}

	// update hour register
	if (!writeRegister(HOUR_ADDR, newHourVal)) {
		return 0;
	}

	return 1;
}

/*
  Description
    increment RTC minute
  Return
    true if success, false if error
*/
static bool AbraRTC::incMin() {
	// get minute register data
	if (!selectRegister(MIN_ADDR)) {
		return 0;
	}

	uint8_t RTCMinVal1s  = 0;
	uint8_t RTCMinVal10s = 0;

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for minute
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCMinVal = Wire.read();
		RTCMinVal1s  = RTCMinVal & 0x0F;
		RTCMinVal10s = (RTCMinVal >> 4) & 0x0F;
	} else {
		return 0;
	}

	// increment min
	if (RTCMinVal10s == 5) {
		if (RTCMinVal1s == 9) {
			// reset to 0 min
			RTCMinVal1s  = 0;
			RTCMinVal10s = 0;
		} else {
			RTCMinVal1s++;
		}
	} else if (RTCMinVal1s == 9) {
		RTCMinVal1s = 0;
		RTCMinVal10s++;
	} else {
		RTCMinVal1s++;
	}
	uint8_t newMinVal = RTCMinVal1s | (RTCMinVal10s << 4);

	// update min register
	if (!writeRegister(MIN_ADDR, newMinVal)) {
		return 0;
	}

	return 1;
}

/*
  Description
    decrement RTC minute
  Return
    true if success, false if error
*/
static bool AbraRTC::decMin() {
	// select minute register data
	if (!selectRegister(MIN_ADDR)) {
		return 0;
	}

	uint8_t RTCMinVal1s  = 0;
	uint8_t RTCMinVal10s = 0;

	Wire.requestFrom(RTC_ADDR, 1); // request 1 byte for minute
	if (Wire.available() == 1) { // make sure 1 byte was returned
		uint8_t RTCMinVal = Wire.read();
		RTCMinVal1s  = RTCMinVal & 0x0F;
		RTCMinVal10s = (RTCMinVal >> 4) & 0x0F;
	} else {
		return 0;
	}

	// decrement min
	if (RTCMinVal1s == 0) {
		RTCMinVal1s  = 9;
		if (RTCMinVal10s == 0) {
			RTCMinVal10s = 5;
		} else {
			RTCMinVal10s--;
		}
	} else {
		RTCMinVal1s--;
	}
	uint8_t newMinVal = RTCMinVal1s | (RTCMinVal10s << 4);

	// update min register
	if (!writeRegister(MIN_ADDR, newMinVal)) {
		return 0;
	}

	return 1;
}

/*
  Description
    turn trickle charge on (1.5k Ohm internal resistance) or off
  Input
    enableTC: true to enable trickle charger, false to disable it
  Return
    true if success, false if error
*/
bool AbraRTC::setTrickleCharge(bool enableTC) {
	// disable EEPROM refresh
	if (!writeBit(CTL_1_ADDR, 3, 0)) { return 0; }

	// wait for EEPROM to not be busy
	while (checkEEPROMBusy());

	// write EEPROM trickle charge setting
	// 1.5k Ohm if enabled
	if (!writeBit(EE_CTL_ADDR, 4, enableTC)) { return 0; }
	_delay_ms(10);

	// renable EEPROM refresh
	if (!writeBit(CTL_1_ADDR, 3, 1)) { return 0; }

	return 1;
}
