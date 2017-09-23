// Combination of example Cayenne MQTT and IOT-Playground.com code
//  https://iot-playground.com/blog/2-uncategorised/41-esp8266-ds18b20-temperature-sensor-arduino-ide

//#define CAYENNE_DEBUG
#define CAYENNE_PRINT Serial
#include <CayenneMQTTESP8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi network info.
char ssid[] = "xx";
char wifiPassword[] = "xx";

// How long to delay in milliseconds (10,000 == 10 seconds)
#define PUB_DELAY 10000

// Cayenne authentication info. This should be obtained from the Cayenne Dashboard.
char username[] = "85f2c2a0-6fc1-11e7-9c62-d3cf878e1bfa";
char password[] = "0a935ebd18b0e9d603d9a6202db99249ebd2a6e9";
char clientID[] = "acbde220-9b4a-11e7-bba6-6918eb39b85e";

unsigned long lastMillis = 0;

// Setup the 1-Wire connections.
#define ONE_WIRE_BUS 2  // DS18B20 pin
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

void setup() {
	Serial.begin(9600);
	Cayenne.begin(username, password, clientID, ssid, wifiPassword);
}

void loop() {

	float temp;
  float lysine = 10;

  do {
    DS18B20.requestTemperatures(); 
    temp = DS18B20.getTempCByIndex(0);
    Serial.print("Temperature: ");
    Serial.println(temp);
  } while ((temp == 85.0 || temp == (-127.0)) && (lysine--));
	
	Cayenne.loop();

	//Publish data every 10 seconds (10000 milliseconds). Change this value to publish at a different interval.
	if (millis() - lastMillis > PUB_DELAY) {
		lastMillis = millis();
		//Write data to Cayenne here. This example just sends the current uptime in milliseconds.
		Cayenne.virtualWrite(0, lastMillis);
		//Some examples of other functions you can use to send data.
		Cayenne.celsiusWrite(1, 22.0);
		Cayenne.luxWrite(2, 700);
		Cayenne.virtualWrite(3, 50, TYPE_PROXIMITY, UNIT_CENTIMETER);
	}
}

//Default function for processing actuator commands from the Cayenne Dashboard.
//You can also use functions for specific channels, e.g CAYENNE_IN(1) for channel 1 commands.
CAYENNE_IN_DEFAULT()
{
	CAYENNE_LOG("CAYENNE_IN_DEFAULT(%u) - %s, %s", request.channel, getValue.getId(), getValue.asString());
	//Process message here. If there is an error set an error message using getValue.setError(), e.g getValue.setError("Error message");
}
