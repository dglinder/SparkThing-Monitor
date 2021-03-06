//#define CAYENNE_DEBUG
#define CAYENNE_PRINT Serial
#include <CayenneMQTTESP8266.h>
#include <OneWire.h>

// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// http://milesburton.com/Dallas_Temperature_Control_Library

OneWire  ds(2);  // on pin 2 (a 4.7K resistor is necessary)

// WiFi network info.
char ssid[] = "xx";
char wifiPassword[] = "xx";

// How long to delay in milliseconds (10,000 == 10 seconds)
#define PUB_DELAY 10000

// Maximum number of temp probes to keep.
#define MAXPROBES 3

// Cayenne authentication info. This should be obtained from the Cayenne Dashboard.
char username[] = "xx";
char password[] = "xx";
char clientID[] = "xx";

byte probenum = 0;
unsigned long lastMillis = 20000;  // Wait ~20 seconds before starting to upload data.
float tempsC[MAXPROBES];
float tempsF[MAXPROBES];
byte MACaddr[MAXPROBES][8];  // Store the 8 bytes of each devices MAC address.

void setup(void) {
  Serial.begin(9600);
  Cayenne.begin(username, password, clientID, ssid, wifiPassword);
  for (int i = 0; i < MAXPROBES; i++) {
    tempsC[i]=-987.654;
    tempsF[i]=-987.654;
    for (int j = 0; j < 8; j++) {
      MACaddr[i][j]=255;
    }
  }
}

void loop(void) {
  byte i;
  byte present = 0;
  byte type_s = 0;    // Chip = DS18B20 or DS1822
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  if ( !ds.search(addr)) {
    // Each call to the ds.search() will find the next available
    // chip, or return 0 when it's done.  The ds.reset_search()
    // call then restart the walk at the beginning.
    // NOTE-DGL: For now assume that the order is the same over
    //           multiple runs.
    // TODO: Setup an array and gather these only during setup,
    //       then loop over that array polling each device in
    //       a specific order.
//    Serial.println("No more addresses.");
//    Serial.println("==================================");
//    Serial.println();
    ds.reset_search();
    delay(250);
    probenum = 0;
    return;
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  Serial.println();

  // Store the devices MAC address
  for( i = 0; i < 8; i++) {
    MACaddr[probenum][i] = addr[i];
  }

//  Serial.print("ROM =");
//  for( i = 0; i < 8; i++) {
//    Serial.write(' ');
//    Serial.print(addr[i], HEX);
//  }
//
//  // the first ROM byte indicates which chip
//  switch (addr[0]) {
//    case 0x10:
//      Serial.println("  Chip = DS18S20");  // or old DS1820
//      type_s = 1;
//      break;
//    case 0x28:
//      Serial.println("  Chip = DS18B20");
//      type_s = 0;
//      break;
//    case 0x22:
//      Serial.println("  Chip = DS1822");
//      type_s = 0;
//      break;
//    default:
//      Serial.println("Device is not a DS18x20 family device.");
//      return;
//  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

//  Serial.print("  Data = ");
//  Serial.print(present, HEX);
//  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
//    Serial.print(data[i], HEX);
//    Serial.print(" ");
  }
//  Serial.print(" CRC=");
//  Serial.print(OneWire::crc8(data, 8), HEX);
//  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
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
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
//  Serial.print("  Temperature = ");
//  Serial.print(celsius);
//  Serial.print(" Celsius, ");
//  Serial.print(fahrenheit);
//  Serial.println(" Fahrenheit");

  tempsC[probenum] = celsius;
  tempsF[probenum] = fahrenheit;
  // We're working on a new probe the next loop.
  probenum++;
  
  Cayenne.loop();

  //Publish data every 10 seconds (10000 milliseconds). Change this value to publish at a different interval.
  if (millis() - lastMillis > PUB_DELAY) {
    Serial.println("################################################");
    lastMillis = millis();
    for ( i = 0; i < MAXPROBES; i++) {
    //Write data to Cayenne here
    //Cayenne.virtualWrite(probenum, celsius, TEMPERATURE, CELSIUS);
    Cayenne.celsiusWrite(i, tempsF[i]);

      Serial.print(" Probe "); Serial.print(i);
      Serial.print(" Value "); Serial.print(tempsC[i]); Serial.print("C");
      Serial.print(" or ");    Serial.print(tempsF[i]); Serial.print("F");
      Serial.print("  ADDR "); Serial.print(MACaddr[i][6], HEX); Serial.print(MACaddr[i][7], HEX);
      Serial.println("");
    }
    //Some examples of other functions you can use to send data.
//    Cayenne.celsiusWrite(1, 22.0);
//    Cayenne.luxWrite(2, 700);
//    Cayenne.virtualWrite(3, 50, TYPE_PROXIMITY, UNIT_CENTIMETER);
  }

}
