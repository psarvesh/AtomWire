#include "AtomWireSlave.h"
//                     {Fami, ----, ----, ----, -ID-, ----, ----,  CRC} 
unsigned char rom[8] = {0x3A, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00};
//                            {TLSB, TMSB, THRE, TLRE, Conf, 0xFF, Rese, 0x10,  CRC}
unsigned char scratchpad[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x10, 0x00};

OneWireSlave aws(4);

void setup() {
  //pinMode(2,OUTPUT); 
  aws.init(rom);
  aws.setScratchpad(scratchpad);
}

void loop() {
  //digitalWrite(2, HIGH);
  
  aws.waitForRequest(false);
}

