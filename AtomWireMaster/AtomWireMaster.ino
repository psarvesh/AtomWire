#include "AtomWire.h"


OneWire awm(12); 
int c = 0;


void setup(void) {
  Serial.begin(9600); 
  
}

void loop(void) {
  
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  
       
  if (!awm.search(addr)) {
      Serial.print("\nNo further Slaves Detected.\n");
      awm.reset_search();
      return;
  }

  Serial.print("\nAddress = ");
  for(i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX);
    Serial.print(" ");
  }

  if ( addr[0] == 0x3A) 
      Serial.print("\nDevice belongs to AtomWire family.");

  else {
      Serial.print("\nDevice family is not recognized.");
      return;
  }
   
  awm.reset();
  awm.select(addr);
  
  //Writing data into the scratchpad - Block, Node IDs as well as to GPIO output pins.
  if(addr[1] == 0xA1)    
  awm.write(0x22);      // Write Block and Node ID into scratchpad of selected slave. 
  else if(addr[1] == 0xA2) {
    
    if (c%4 == 0)
      awm.write(0x8A); // Write "E" to the GPIO pins. 
  //awm.write(0x21);
   else if (c%4 == 1)
   awm.write(0x8C);
   
   else if(c%4 == 2)
   awm.write(0x86);
   
   else
   awm.write(0x8E);
  }
  else
  awm.write(0x43);
  //awm.depower();
  
  delay(1500);     

  //Reading scratchpad data - Block, Node IDs as well as GPIO input pins.
  present = awm.reset();
  awm.select(addr);  
  //if(addr[1] == 0xA1)  
  //awm.write(0x8D);  // Read Scratchpad - 0xBE for just read & 0xA1 for GPIO read.
  if(addr[1] == 0xA1)
  awm.write(0xA1);
  else
  awm.write(0xBE);


  Serial.print("\nP = ");
  Serial.print(present,HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = awm.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print("\nCRC = ");
  Serial.print( OneWire::crc8( data, 8), HEX);
  Serial.println();

  c++ ; 
}




