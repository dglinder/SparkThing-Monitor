//#define CAYENNE_DEBUG
#define CAYENNE_PRINT Serial
#include <CayenneMQTTESP8266.h>
#include <OneWire.h>
//
// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// http://milesburton.com/Dallas_Temperature_Control_Library

// Wiring setup on a SparkFun Thing Dev board:
// * Ground == Blue
// * 3V     == Blue/White
// * Pin 2  == Green
#define DATAPIN 14
OneWire  ds(DATAPIN);  // a 4.7K resistor is necessary

// Wiring to the Dallas Semiconductor DS18B20 sensors:
// * Pin 1  == Ground == Blue       +-------+
// * Pin 2  == Data   == Green      \ 1 2 3 /
// * Pin 3  == 3V     == Blue/White  \-----/

// Include local WiFi and Cayenne credentials from separate file.
// Must include:
// A: WiFi network info.
//   char ssid[] = "xx";
//   char wifiPassword[] = "xx";
// B: Cayenne authentication info. This should be obtained from the Cayenne Dashboard.
//   char username[] = "xx";
//   char password[] = "xx";
//   char clientID[] = "xx";
#include "creds.h"

// How long to delay in milliseconds (10,000 == 10 seconds, 120,000 == 120 seconds == 2 minutes)
#define PUB_DELAY 120000

// Maximum number of temp probes to keep track of.
#define MAXPROBES 3

// Default starting temp
#define DEFTEMP -200.00

byte probenum = 0;
byte probecnt = 0;

unsigned long lastMillis = 20000;  // Wait ~20 seconds before starting to upload data.
float tempsC[MAXPROBES];
float tempsF[MAXPROBES];
byte MACaddr[MAXPROBES][8];  // Store the 8 bytes of each devices MAC address.
byte type_s[MAXPROBES];    // Chip type selector
byte addr[8];  // The Chip MAC address
byte i,j;

void setup(void) {
  Serial.begin(9600);
  Cayenne.begin(username, password, clientID, ssid, wifiPassword);

  // Enumerate each device at start-up, each
  // address is in "addr" when they are found.
  // Store the addresses in MACaddr for reference
  while (ds.search(addr)) {
    // Store the devices MAC address
    Serial.print("Device: "); Serial.print(probecnt);
    Serial.print(" MAC address: ");
    for( i = 0; i < 8; i++) {
      MACaddr[probecnt][i] = addr[i];
      Serial.print(addr[i], HEX);
    }
    Serial.println();

    // Store a starting temp.
    tempsC[probecnt]=DEFTEMP;
    tempsF[probecnt]=DEFTEMP;
    // the first ROM byte indicates which chip
    switch (addr[0]) {
      case 0x10: 
//        Serial.println("  Chip = DS18S20");  // or old DS1820
        type_s[probecnt] = 1;
        break;
      case 0x28:
//        Serial.println("  Chip = DS18B20");
        type_s[probecnt] = 0;
        break;
      case 0x22:
//        Serial.println("  Chip = DS1822");
        type_s[probecnt] = 0;
        break;
      default:
        Serial.println("Device is not a DS18x20 family device.");
        return;
    } 

    // Register we have this new probe.
    probecnt++;
  }

  if (probecnt == 0) {
    Serial.println("ERROR: Did not detect any devices.");
    while (1) {
      delay(1000);
    }
  }

  Serial.print("Sending data every ");
  Serial.print(PUB_DELAY/1000);
  Serial.println(" seconds.");

  // Done enumerating, reset the BUS.
  ds.reset_search();
  delay(250);
  probenum = 0;
}

void loop(void) {
  byte present = 0;
  byte data[12];
  float celsius, fahrenheit;

  // Publish data every PUB_DELAY milliseconds. 
  // Change this value above to publish at a different interval.
  if (millis() - lastMillis > PUB_DELAY) {
    Serial.println("################################################");
    lastMillis = millis();
    for ( i = 0; i < MAXPROBES; i++) {
      //Cayenne.celsiusWrite(i, tempsC[i]);
      Cayenne.fahrenheitWrite(i, tempsF[i]);
      Serial.print(" Probe "); Serial.print(i);
      Serial.print(" Value "); Serial.print(tempsC[i]); Serial.print("C");
      Serial.print(" or ");    Serial.print(tempsF[i]); Serial.print("F");
      Serial.print("  ADDR "); Serial.print(MACaddr[i][6], HEX); Serial.print(MACaddr[i][7], HEX);
      Serial.println("");
      delay(1000); // Small delay for the Cayenne library to catch its breath.
    }
  }

  // Start back at probe #0 when we get to the end of the line.
  if (probenum >= probecnt) {
    probenum = 0;
  }

  // Select the device based on the probenum counter
  for (i = 0; i < 8; i++) {
    addr[i] = MACaddr[probenum][i];
  }
  ds.reset();

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s[probenum]) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    // default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;

  tempsC[probenum] = celsius;
  tempsF[probenum] = fahrenheit;
  // We're registering a new probe the next loop.
  probenum++;
  
  Cayenne.loop();
}


