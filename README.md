# AtomWire
Smart Energy for IoT

Steps to configure the Atomwire Master-Slave system. 

Contents: 
1. AtomWireMaster - This folder contains all the Arduino sketch, libraries and head files for configuring the 
Arduino UNO as the master. Note that the libraries and the header files must be in the same folder as the sketch so that
arduino IDE does not take the default files from its structure. 

2. AtomWire_Slave - This folder contains all the necessary Sketch, libraries, header files and pin mapping from Arduino to AVR that is required to configure Arduino slave as AVR Attiny85. Again,the libraries and the header files must be in the same folder as the sketch so that
arduino IDE does not take the default files from its structure. Again, the libraries and the header files must be in the same folder as the sketch so that
arduino IDE does not take the default files from its structure.

3. boards.txt - The modified fuse bits to reduce the turn on time of the AVR is configured in this file. 


Steps to configure the Atomwire Master.

1. Open the Arduino IDE. Make sure that Arduino UNO is connected to the system. 
2. Burn the code in AtomWireMaster.ino file onto the Arduino. 
3. Connect a the buffer circuit as explained in the blog/report. 
3. Open the serial terminal. 
4. If slaves are detected, the slave IDs will start showing up on the serial output. 
5. If there are no slaves "No futher slaves are detected" will be displayed. 

Steps to configure the Atomwire Slave.

1. You will need a AVR programmer. Put in the AVR Attiny85V into to programmer and plug in the programmer to the 
USB port of the system. 
2. Open Arduino IDE. 
3. Open the sketch AtomWire_Slave.ino
4. Follow the steps here to add AVRAttiny85 as a list of components to Arduino IDE. 
http://highlowtech.org/?p=1695
5. Go to Tools -> Programmer:USBTinyISP
6. Again Tools -> Processor:ATtiny85
7. Tools -> Clock:8Mhz "Internal"
8. Copy the "boards.txt" file and paste it in Sketchbook folder -> hardware -> attiny -> avr 
9. Select Burn Bootloader under Tools menu. This will burn the bootloader to the AVR with configuration in the boards.txt. 
10. Now burn the code into AVR ATtiny in the similar way as Arduino. 
11. Remove the IC from the programmer and connect it along with the accompanying circuitary. 
12. When the Slave is connected to the 1-wire line, its ID will appear on the serial terminal of the Master Arduino.
13. The ID of the slave can be changed as required from the sketch byu modifying "unsigned char rom[8]" variable. 
