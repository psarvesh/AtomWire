#include <OneWire.h>

static const byte addr_list[10] = {11, 10, 9, 8,7,12,2,3,5,6 }; //Define the number of atomwire lines you want here. The pins specified here must be available on the Arduino or it might throw an error
//static const byte addr_list[2] = {12,7};
//OneWire awm(12);
OneWire **awm; 

static byte num[10];
//static byte num[2];

void setup() {
  Serial.begin(9600);

  
  
  awm = new OneWire*[10];
  //awm = new OneWire*[2];

  for (int i = 0; i < 10; i++) {
   //for (int i = 0; i < 2; i++) {
    awm[i] = new OneWire(addr_list[i]);
  }

  

  for (int i = 0; i < 10; i++) {
  //for (int i = 0; i < 2; i++) {
    num[i] = 0;
  }
  
 

  /*
  pinMode(47, OUTPUT);
  pinMode(49, OUTPUT);
  pinMode(51, OUTPUT);
  pinMode(53, OUTPUT);

  digitalWrite(47, HIGH);
  digitalWrite(49, HIGH);
  digitalWrite(51, HIGH);
  digitalWrite(53, HIGH);
  */
  
}

byte addr[8];
bool send_update = false;

void loop() {

  //static byte num[1]= {0}; 
  //byte addr[8];
  
  /*     
   *      This section loops through all the atomwire lines and gives the number of blocks in each line
   */
  
  
  for (int i = 0; i < 10; i++) {
   // for (int i = 0; i < 2; i++) {
    //Serial.print(i);
    //Serial.print(*awm[i]);
    /*
    ds[i]->reset();
    byte num_found = 0;
    
    while (ds[i]->search(addr)) {
      num_found++;
      Serial.print("1");
    }
    */
    //awm.reset_search();
    //awm[i]->reset_search();
    byte num_found = 0;

    while(awm[i]->search(addr)){
        //Serial.print("N");
        num_found++;
        //Serial.print("\nAddress = ");
        //for(int g = 0; g < 8; g++) {
        //Serial.print(addr[g], HEX);
        //Serial.print(" ");
        //}

      
    }
    
  
  //Serial.print(num_found);
    

    if (num_found > num[i]) {
        for (int j = 0; j < num_found - num[i]; j++) {
          //Serial.print(num_found);
          send_update = true;
          num[i]++;
          //Serial.print(num[i]);
        }
      } 

    else if (num_found < num[i]) {
        for (int j = 0; j < num[i] - num_found; j++) {
           //Serial.print("2");
          send_update = true;
          num[i]--;
        }
     }

     
    awm[i]->reset_search();
   
    
    //awm[i]->reset_search();
  }
  //Serial.print(num[0]);
  
  //Serial.flush();
  //if (send_update) {
    Serial.write('$');
    uint8_t sum = 0;

    //Write to the serial so that unity can capture it
    for (int i = 0; i < 10; i++) {
    //for (int i = 0; i < 2; i++) {
      //Serial.print(num[i]);
      Serial.write(num[i]);
      sum += num[i];
    }

    send_update = false;
    //Serial.print(sum);
    Serial.write(sum);
    Serial.flush();
  //}

  delay(75);
  
}
