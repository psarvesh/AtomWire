#include "AtomWireSlave.h"
#include "pins_attiny85.h"

extern "C" {
// #include "WConstants.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
}

#define DIRECT_READ(base, mask)        (((*(base)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)  ((*(base+1)) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask) ((*(base+1)) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)   ((*(base+2)) &= ~(mask))
#define DIRECT_WRITE_HIGH(base, mask)  ((*(base+2)) |= (mask))

//#define microsecondsToClockCycles(a) ( (a) * clockCyclesPerMicrosecond() )

//ORIG: #define TIMESLOT_WAIT_RETRY_COUNT microsecondsToClockCycles(120) / 10L
//FULL: #define TIMESLOT_WAIT_RETRY_COUNT ( ((120) * (8000000L / 1000L)) / 1000L )
//WORKING: #define TIMESLOT_WAIT_RETRY_COUNT microsecondsToClockCycles(240000) / 10L

//These are the major change from original, we now wait quite a bit longer for some things
#define TIMESLOT_WAIT_RETRY_COUNT microsecondsToClockCycles(120) / 10L
//This TIMESLOT_WAIT_READ_RETRY_COUNT is new, and only used when waiting for the line to go low on a read
//It was derived from knowing that the Arduino based master may go up to 130 micros more than our wait after reset
#define TIMESLOT_WAIT_READ_RETRY_COUNT microsecondsToClockCycles(135)

OneWireSlave::OneWireSlave(uint8_t pin) {
	pin_bitmask = digitalPinToBitMask(pin);
  baseReg = portInputRegister(digitalPinToPort(pin));
}

void OneWireSlave::init(unsigned char rom[8]) {
	for (int i=0; i<7; i++)
    this->rom[i] = rom[i];
  this->rom[7] = crc8(this->rom, 7);
}

void OneWireSlave::setScratchpad(unsigned char scratchpad[9]) {
  for (int i=0; i<8; i++)
    this->scratchpad[i] = scratchpad[i];
  this->scratchpad[8] = crc8(this->scratchpad, 8);
}

void OneWireSlave::gpioRead(){
    uint8_t gpioRvalue = 0x00;
    uint8_t bitShift = 0x01;
    
    cli();
    for(int i=0; i<4; i++){
          DIRECT_MODE_INPUT(portInputRegister(digitalPinToPort(i)),digitalPinToBitMask(i));
          if(DIRECT_READ(portInputRegister(digitalPinToPort(i)),digitalPinToBitMask(i)))
           gpioRvalue |= bitShift;
           else
           gpioRvalue |= 0x00;
           bitShift <<= 1;          
    }
    sei();
    scratchpad[2] = gpioRvalue;
}

bool OneWireSlave::gpioWrite(uint8_t gpioWvalue){
    
    scratchpad[3] = (gpioWvalue & 0x0F);
    uint8_t wMask = 0x01;
      
    cli();
    for(int i=0; i<4; i++){
       uint8_t tMask = gpioWvalue & wMask;
       
          DIRECT_MODE_OUTPUT(portInputRegister(digitalPinToPort(i)),digitalPinToBitMask(i));
          if(tMask)
              DIRECT_WRITE_HIGH(portInputRegister(digitalPinToPort(i)),digitalPinToBitMask(i));
          else 
              DIRECT_WRITE_LOW(portInputRegister(digitalPinToPort(i)),digitalPinToBitMask(i));
          wMask <<= 1;          
    }
    sei();
    return TRUE;
}

bool OneWireSlave::waitForRequest(bool ignore_errors) {
  errno = ONEWIRE_NO_ERROR;

  for (;;) {
    //delayMicroseconds(40);
    //Once reset is done, it waits another 30 micros
    //Master wait is 65, so we have 35 more to send our presence now that reset is done
    if (!waitReset(0) ) {
      continue;
    }
    //Reset is complete, tell the master we are prsent
    // This will pull the line low for 125 micros (155 micros since the reset) and 
    //  then wait another 275 plus whatever wait for the line to go high to a max of 480
    // This has been modified from original to wait for the line to go high to a max of 480.
    if (!presence() ) {
      continue;
    }
    //Now that the master should know we are here, we will get a command from the line
    //Because of our changes to the presence code, the line should be guranteed to be high
    if (recvAndProcessCmd() ) {
      return TRUE;
    }
    else if ((errno == ONEWIRE_NO_ERROR) || ignore_errors) {
      continue;
    }
    else {
      return FALSE;
    }
  }
}

bool OneWireSlave::recvAndProcessCmd() {
	char addr[8];
  uint8_t oldSREG = 0;

  for (;;) {
    uint8_t cmd = recv();
    switch (cmd) {
      case 0xF0: // SEARCH ROM
        search();
        delayMicroseconds(6900);
        return FALSE;
      case 0x33: // READ ROM
        sendData(rom, 8);
        if (errno != ONEWIRE_NO_ERROR)
          return FALSE;
        break;
      case 0x55: // MATCH ROM - Choose/Select ROM
        recvData(addr, 8);
        if (errno != ONEWIRE_NO_ERROR)
          return FALSE;
        for (int i=0; i<8; i++){
          if (rom[i] != addr[i])
            return FALSE;
            resumeaddr[i] = addr[i];
        }
        duty();
      case 0xCC: // SKIP ROM
        return TRUE;
      case 0x69: // RESUME ROM
        recvData(resumeaddr, 8);
        if (errno != ONEWIRE_NO_ERROR)
          return FALSE;
        duty();
      default: // Unknow command
        if (errno == ONEWIRE_NO_ERROR)
          break; // skip if no error
        else
          return FALSE;
    }
  }
}

bool OneWireSlave::duty() {
	uint8_t gpio = 0x80;
	uint8_t done = recv();
  
  if((done & 0xF0) == gpio) //CHECK FOR GPIO WRITE COMMAND
      gpioWrite(done);
  else if((done & 0xF0) == 0x10){
      scratchpad[0] = 0x11;
      scratchpad[1] = 0x01;
      return TRUE;
  }
  else if((done & 0xF0) == 0x20){
    scratchpad[0] = 0x12;
      if((done & 0x0F) == 0x01)
          scratchpad[1] = 0x01;
      else 
          scratchpad[1] = 0x02;
      return TRUE;
  }
  else if((done & 0xF0) == 0x40){
    scratchpad[0] = 0x22;
       if((done & 0x03) == 0x03)
          scratchpad[1] = 0x03;
       else if((done & 0x02) == 0x02)
          scratchpad[1] = 0x02;
       else if((done & 0x01) == 0x01)
          scratchpad[1] = 0x01;
       else
          scratchpad[1] = 0x04;
       return TRUE;
  }
  else
  {
	switch (done) {
		case 0xBE: // READ SCRATCHPAD
			sendData(scratchpad, 9);
				if (errno != ONEWIRE_NO_ERROR)
					return FALSE;
			break;
    case 0xA1:  //READ GPIO PINS
      gpioRead();
      sendData(scratchpad, 9);
       if (errno != ONEWIRE_NO_ERROR)
          return FALSE;
      break;
		default:
			break;
			if (errno == ONEWIRE_NO_ERROR)
				break; // skip if no error
			else
				return FALSE;
	return TRUE;
	}
  }
}

inline void OneWireSlave::tunedDelay(volatile uint16_t delay) { 
  volatile uint8_t tmp=0;
  asm volatile("sbiw    %0, 0x01 \n\t"
    "ldi %1, 0xFF \n\t"
    "cpi %A0, 0xFF \n\t"
    "cpc %B0, %1 \n\t"
    "brne .-10 \n\t"
    : "+w" (delay), "+a" (tmp)
    : "0" (delay)
    );
}

void OneWireSlave::customDelay(uint16_t _tx_delay) {
  //uint8_t oldSREG = SREG;
  //cli();  // turn off interrupts
  tunedDelay(_tx_delay);
  //SREG = oldSREG; // turn interrupts back on
}

bool OneWireSlave::search() {
  uint8_t bitmask;
  uint8_t bit_send, bit_recv;

  for (int i=0; i<8; i++) {
    for (bitmask = 0x01; bitmask; bitmask <<= 1) {
      bit_send = (bitmask & rom[i])?1:0;
      sendBit(bit_send);
      sendBit(!bit_send);
      bit_recv = recvBit();
      if (errno != ONEWIRE_NO_ERROR)
        return FALSE;
      if (bit_recv != bit_send)
        return FALSE;
    }
  }
  return TRUE;
}

bool OneWireSlave::waitReset(uint16_t timeout_ms) {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;
    unsigned long time_stamp;

    errno = ONEWIRE_NO_ERROR;
    cli();
    DIRECT_MODE_INPUT(reg, mask);
    sei();

    //Wait for the line to fall
    if (timeout_ms != 0) {
        time_stamp = micros() + timeout_ms*1000;
        while (DIRECT_READ(reg, mask)) {
            if (micros() > time_stamp) {
                errno = ONEWIRE_WAIT_RESET_TIMEOUT;
                return FALSE;
            }
        }
    } else {
      //Will wait forever for the line to fall
      while (DIRECT_READ(reg, mask)) {};
    }
    
    //Set to wait for rise up to 540 micros
    //Master code sets the line low for 500 micros
    //TODO The actual documented max is 640, not 540
    time_stamp = micros() + 540;
    
    //Wait for the rise on the line up to 540 micros
    while (DIRECT_READ(reg, mask) == 0) {
        if (micros() > time_stamp) {
            errno = ONEWIRE_VERY_LONG_RESET;
            return FALSE;
        }
    }
    
    //If the master pulled low for exactly 500, then this will be 40 wait time
    // Recommended for master is 480, which would be 60 here then
    // Max is 640, which makes this negative, but it returns above as a "ONEWIRE_VERY_LONG_RESET"
    // this gives an extra 10 to 30 micros befor calling the reset invalid
    if ((time_stamp - micros()) > 70) {
        errno = ONEWIRE_VERY_SHORT_RESET;
        return FALSE;
    }
    
    //Master will now delay for 65 to 70 recommended or max of 75 before it's "presence" check
    // and then read the pin value (checking for a presence on the line)
    // then wait another 490 (so, 500 + 64 + 490 = 1054 total without consideration of actual op time) on Arduino, 
    // but recommended is 410 with total reset length of 480 + 70 + 410 (or 480x2=960)
    delayMicroseconds(30);
    //Master wait is 65, so we have 35 more to send our presence now that reset is done
    return TRUE;
}
bool OneWireSlave::waitReset() {
  return waitReset(1000);
}

bool OneWireSlave::presence(uint8_t delta) {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;

    //Reset code already waited 30 prior to calling this
    // Master will not read until 70 recommended, but could read as early as 60
    // so we should be well enough ahead of that. Arduino waits 65
    errno = ONEWIRE_NO_ERROR;
    cli();
    DIRECT_WRITE_LOW(reg, mask);
    DIRECT_MODE_OUTPUT(reg, mask);    // drive output low
    sei();

    //Delaying for another 125 (orignal was 120) with the line set low is a total of at least 155 micros
    // total since reset high depends on commands done prior, is technically a little longer
    delayMicroseconds(125);
    cli();
    DIRECT_MODE_INPUT(reg, mask);     // allow it to float
    sei();

    //Default "delta" is 25, so this is 275 in that condition, totaling to 155+275=430 since the reset rise
    // docs call for a total of 480 possible from start of rise before reset timing is completed
    //This gives us 50 micros to play with, but being early is probably best for timing on read later
    //delayMicroseconds(300 - delta);
    delayMicroseconds(250 - delta);
    
    //Modified to wait a while (roughly 50 micros) for the line to go high
    // since the above wait is about 430 micros, this makes this 480 closer
    // to the 480 standard spec and the 490 used on the Arduino master code
    // anything longer then is most likely something going wrong.
    uint8_t retries = 25;
    while (!DIRECT_READ(reg, mask));
    do {
		if ( retries-- == 0)
			return FALSE;
		delayMicroseconds(2); 
    } while(!DIRECT_READ(reg, mask));
    /*
    if ( !DIRECT_READ(reg, mask)) {
        errno = ONEWIRE_PRESENCE_LOW_ON_LINE;
        return FALSE;
    } else
        return TRUE;
    */
}
bool OneWireSlave::presence() {
  return presence(25);
}

uint8_t OneWireSlave::sendData(char buf[], uint8_t len) {
    uint8_t bytes_sended = 0;

    for (int i=0; i<len; i++) {
        send(buf[i]);
        if (errno != ONEWIRE_NO_ERROR)
            break;
        bytes_sended++;
    }
    return bytes_sended;
}

uint8_t OneWireSlave::recvData(char buf[], uint8_t len) {
    uint8_t bytes_received = 0;
    
    for (int i=0; i<len; i++) {
        buf[i] = recv();
        if (errno != ONEWIRE_NO_ERROR)
            break;
        bytes_received++;
    }
    return bytes_received;
}

void OneWireSlave::send(uint8_t v) {
    errno = ONEWIRE_NO_ERROR;
    for (uint8_t bitmask = 0x01; bitmask && (errno == ONEWIRE_NO_ERROR); bitmask <<= 1)
        sendBit((bitmask & v)?1:0);
}

uint8_t OneWireSlave::recv() {
    uint8_t r = 0;

    errno = ONEWIRE_NO_ERROR;
    for (uint8_t bitmask = 0x01; bitmask && (errno == ONEWIRE_NO_ERROR); bitmask <<= 1)
        if (recvBit())
            r |= bitmask;
    return r;
}

void OneWireSlave::sendBit(uint8_t v) {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;

    cli();
    DIRECT_MODE_INPUT(reg, mask);
    //waitTimeSlot waits for a low to high transition followed by a high to low within the time-out
    uint8_t wt = waitTimeSlot();
    if (wt != 1 ) { //1 is success, others are failure
      if (wt == 10) {
        errno = ONEWIRE_READ_TIMESLOT_TIMEOUT_LOW;
      } else {
        errno = ONEWIRE_READ_TIMESLOT_TIMEOUT_HIGH;
      }
      sei();
      return;
    }
    if (v & 1)
        delayMicroseconds(30);
    else {
        cli();
        DIRECT_WRITE_LOW(reg, mask);
        DIRECT_MODE_OUTPUT(reg, mask);
        delayMicroseconds(30);
        DIRECT_WRITE_HIGH(reg, mask);
        sei();
    }
    sei();
    return;
}

uint8_t OneWireSlave::recvBit(void) {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;
    uint8_t r;

    cli();
    DIRECT_MODE_INPUT(reg, mask);
    //waitTimeSlotRead is a customized version of the original which was also
    // used by the "write" side of things.
    uint8_t wt = waitTimeSlotRead();
    if (wt != 1 ) { //1 is success, others are failure
      if (wt == 10) {
        errno = ONEWIRE_READ_TIMESLOT_TIMEOUT_LOW;
      } else {
        errno = ONEWIRE_READ_TIMESLOT_TIMEOUT_HIGH;
      }
      sei();
      return 0;
    }
    delayMicroseconds(30);
    //TODO Consider reading earlier: delayMicroseconds(15);
    r = DIRECT_READ(reg, mask);
    sei();
    return r;
}

uint8_t OneWireSlave::waitTimeSlot() {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;
    uint16_t retries;

    //Wait for a 0 to rise to 1 on the line for timeout duration
    //If the line is already high, this is basically skipped
    retries = TIMESLOT_WAIT_RETRY_COUNT;
    //While line is low, retry
    while ( !DIRECT_READ(reg, mask))
        if (--retries == 0)
            return 10;
            
    //Wait for a fall form 1 to 0 on the line for timeout duration
    retries = TIMESLOT_WAIT_RETRY_COUNT;
    while ( DIRECT_READ(reg, mask));
        if (--retries == 0)
            return 20;

    return 1;
}

//This is a copy of what was orig just "waitTimeSlot"
// it is customized for the reading side of things
uint8_t OneWireSlave::waitTimeSlotRead() {
    uint8_t mask = pin_bitmask;
    volatile uint8_t *reg asm("r30") = baseReg;
    uint16_t retries;

    //Wait for a 0 to rise to 1 on the line for timeout duration
    //If the line is already high, this is basically skipped
    retries = TIMESLOT_WAIT_RETRY_COUNT;
    //While line is low, retry
    while ( !DIRECT_READ(reg, mask))
        if (--retries == 0)
            return 10;
            
    //TODO Seems to me that the above loop should drop out immediately because
    // The line is already high as our wait after presence is relatively short
    // So now it just waits a short period for the write of a bit to start
    // Unfortunately per "recommended" this is 55 micros to 130 micros more
    // more than what we may have already waited.
            
    //Wait for a fall form 1 to 0 on the line for timeout duration
    retries = TIMESLOT_WAIT_READ_RETRY_COUNT;
    while ( DIRECT_READ(reg, mask));
        if (--retries == 0)
            return 20;

    return 1;
}

#if ONEWIRESLAVE_CRC
// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
//

#if ONEWIRESLAVE_CRC8_TABLE
// This table comes from Dallas sample code where it is freely reusable,
// though Copyright (C) 2000 Dallas Semiconductor Corporation
static const uint8_t PROGMEM dscrc_table[] = {
      0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
    157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
     35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
    190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
     70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
    219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
    101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
    248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
    140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
     17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
    175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
     50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
    202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
     87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
    233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
    116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

//
// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (note: this might better be done without to
// table, it would probably be smaller and certainly fast enough
// compared to all those delayMicrosecond() calls.  But I got
// confused, so I use this table from the examples.)
//
uint8_t OneWireSlave::crc8(char addr[], uint8_t len)
{
    uint8_t crc = 0;

    while (len--) {
        crc = pgm_read_byte(dscrc_table + (crc ^ *addr++));
    }
    return crc;
}
#else
//
// Compute a Dallas Semiconductor 8 bit CRC directly.
//
uint8_t OneWireSlave::crc8(char addr[], uint8_t len)
{
    uint8_t crc = 0;
    
    while (len--) {
        uint8_t inbyte = *addr++;
        for (uint8_t i = 8; i; i--) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}
#endif

#endif
