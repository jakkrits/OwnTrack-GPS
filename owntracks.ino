/*
 * owntracks-electron.ino (C)2016 by Christoph Krey & JP Mens
 *
 * GPS module must be connected to Serial at 9600bd. The Electron
 * will, periodically, publish location data and battery status
 * via the Particle Cloud and then, if the configured `interval' is
 * set, go into deep sleep for `interval' seconds.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, version 2.1 of the License.
 */


#pragma SPARK_NO_PREPROCESSOR
#include "Particle.h"
#include <TinyGPS++/TinyGPS++.h>
// #include "cellular_hal.h"
// #include "apn.h"

/*
 * When connecting an Adafruit Ultimate GPS, optionally
 * wire the ENable pin from the GPS board to the Electron
 * and define ENABLE_PIN to the digital pin on the Electron.
 */

//#define ENABLE_PIN	D7


long lastSync;
long lastCell;

char status[128];

int set_interval(String secs);		// Particle function

#define INTERVAL_ADDRESS 0x000A
#define INTERVAL_DEFAULT 600
uint32_t interval;			// "publish" interval in seconds

long last_fix;
double lat;
double lon;
double SoC;

TinyGPSPlus gps;
FuelGauge fuel;

int turnLed(String command);
int tinkerDigitalRead(String command);
int tinkerDigitalWrite(String command);
int tinkerAnalogRead(String command);
int tinkerAnalogWrite(String command);

void setup()
{
    Particle.keepAlive(45);

	RGB.control(true);

	Serial1.begin(9600);
// #ifdef ENABLE_PIN
// 	pinMode(ENABLE_PIN, OUTPUT);
// 	digitalWrite(ENABLE_PIN, HIGH);
// #endif

// #ifdef APNxxx
// 	STARTUP(cellular_credentials_set(APN, USERNAME, PASSWORD, NULL));
// #endif

// 	Cellular.on();
// 	Cellular.connect(WIFI_CONNECT_SKIP_LISTEN);
// 	Particle.connect();
// 	Particle.connected();

	Time.zone(0);
	lastSync = Time.now();
	lastCell = 0;

	EEPROM.get(INTERVAL_ADDRESS, interval);
	if (interval == 0xFFFFFFFF) {
		interval = INTERVAL_DEFAULT;
		EEPROM.put(INTERVAL_ADDRESS, interval);
	}

	Particle.variable("status", status);
	Particle.function("interval", set_interval);
	Particle.function("led", turnLed);
	Particle.function("digitalRead", tinkerDigitalRead);
	Particle.function("digitalWrite", tinkerDigitalWrite);
	Particle.function("analogRead", tinkerAnalogRead);
	Particle.function("analogWrite", tinkerAnalogWrite);
}

void loop()
{
	/* sync the clock once a day */
	if (Time.now() > lastSync + 86400) {
		Particle.syncTime();
		lastSync = Time.now();
	}

	/* read battery state every 10 min */
	if (Time.now() > lastCell + 600) {
		SoC = fuel.getSoC();
		lastCell = Time.now();
	}

	/* read gps */
	while (Serial1.available()) {
		char c = Serial1.read();
		gps.encode(c);
	}
	if (gps.location.isValid()) {
		last_fix = Time.now() - gps.location.age() / 1000;
		lat = gps.location.lat();
		lon = gps.location.lng();
	    Particle.publish("BATT","volt:" + String::format("%.2f",fuel.getVCell()) + ",percent:" + String::format("%.2f",fuel.getSoC()), 600, PRIVATE);
        Particle.publish("LAT: " + (String)lat + "LNG: " + (String)lon);
	}

	/* Status show GPS and Connection status alternating
	 *
	 * show red LED if gps location is not valid
	 * show greed LED no gps location is available
	 *
	 * show blue LED if connected
	 * show no LED if not connected
	 */
	if (Time.now() % 2 == 0) {
		if (gps.location.isValid()) {
			RGB.color(0, 255, 0);
		} else {
			RGB.color(255, 0, 0);
		}
	} else {
		if (Particle.connected()) {
			RGB.color(0, 0, 255);
		} else {
			RGB.color(0, 0, 0);
		}
	}

	/* set cloud variable */
	snprintf(status, sizeof(status), "%ld,%.6f,%.6f,%.1f,%ld",
		last_fix, lat, lon, SoC, interval);

	if (gps.location.isValid()) {
		while (!Particle.publish("owntracks", status, 30, PRIVATE)) {
			/* indicator for unsuccessfull publish */
			for (int i = 5; i; i--) {
				RGB.color(255, 0, 0);
				delay(333);
				RGB.color(0, 0, 0);
				delay(667);
			}
		}
// #ifdef ENABLE_PIN
// 		/* Switch off GPS module */
// 		digitalWrite(ENABLE_PIN, LOW);
// #endif
		/* countdown before going to sleep */
		for (int i = 10; i; i--) {
			RGB.color(0, 255, 0);
			delay(333);
			RGB.color(0, 0, 0);
			delay(667);
		}
// 		if (interval) {
// 			Cellular.disconnect();
// 			Cellular.off();
// 			System.sleep(SLEEP_MODE_DEEP, interval);
// 		}
	}
}

int set_interval(String secs)
{
	int n = atoi(secs);

	if (n >= 0) {
		interval = n;
		EEPROM.put(INTERVAL_ADDRESS, interval);
		return (1);
	}
	return (0);	// tell caller this failed
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
int turnLed(String command) {
    if(command == "on") {
        digitalWrite(D7,HIGH);
        return 1;
    } else if (command == "off") {
        digitalWrite(D7,LOW);
        return 0;
    } else {
        return -1;
    }
}
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////




/*******************************************************************************
 * Function Name  : tinkerDigitalRead
 * Description    : Reads the digital value of a given pin
 * Input          : Pin
 * Output         : None.
 * Return         : Value of the pin (0 or 1) in INT type
                    Returns a negative number on failure
 *******************************************************************************/
int tinkerDigitalRead(String pin)
{
	//convert ascii to integer
	int pinNumber = pin.charAt(1) - '0';
	//Sanity check to see if the pin numbers are within limits
	if (pinNumber< 0 || pinNumber >7) return -1;

	if(pin.startsWith("D"))
	{
		pinMode(pinNumber, INPUT_PULLDOWN);
		return digitalRead(pinNumber);
	}
	else if (pin.startsWith("A"))
	{
		pinMode(pinNumber+10, INPUT_PULLDOWN);
		return digitalRead(pinNumber+10);
	}
	return -2;
}

/*******************************************************************************
 * Function Name  : tinkerDigitalWrite
 * Description    : Sets the specified pin HIGH or LOW
 * Input          : Pin and value
 * Output         : None.
 * Return         : 1 on success and a negative number on failure
 *******************************************************************************/
int tinkerDigitalWrite(String command)
{
	bool value = 0;
	//convert ascii to integer
	int pinNumber = command.charAt(1) - '0';
	//Sanity check to see if the pin numbers are within limits
	if (pinNumber< 0 || pinNumber >7) return -1;

	if(command.substring(3,7) == "HIGH") value = 1;
	else if(command.substring(3,6) == "LOW") value = 0;
	else return -2;

	if(command.startsWith("D"))
	{
		pinMode(pinNumber, OUTPUT);
		digitalWrite(pinNumber, value);
		return 1;
	}
	else if(command.startsWith("A"))
	{
		pinMode(pinNumber+10, OUTPUT);
		digitalWrite(pinNumber+10, value);
		return 1;
	}
	else return -3;
}

/*******************************************************************************
 * Function Name  : tinkerAnalogRead
 * Description    : Reads the analog value of a pin
 * Input          : Pin
 * Output         : None.
 * Return         : Returns the analog value in INT type (0 to 4095)
                    Returns a negative number on failure
 *******************************************************************************/
int tinkerAnalogRead(String pin)
{
	//convert ascii to integer
	int pinNumber = pin.charAt(1) - '0';
	//Sanity check to see if the pin numbers are within limits
	if (pinNumber< 0 || pinNumber >7) return -1;

	if(pin.startsWith("D"))
	{
		return -3;
	}
	else if (pin.startsWith("A"))
	{
		return analogRead(pinNumber+10);
	}
	return -2;
}

/*******************************************************************************
 * Function Name  : tinkerAnalogWrite
 * Description    : Writes an analog value (PWM) to the specified pin
 * Input          : Pin and Value (0 to 255)
 * Output         : None.
 * Return         : 1 on success and a negative number on failure
 *******************************************************************************/
int tinkerAnalogWrite(String command)
{
	//convert ascii to integer
	int pinNumber = command.charAt(1) - '0';
	//Sanity check to see if the pin numbers are within limits
	if (pinNumber< 0 || pinNumber >7) return -1;

	String value = command.substring(3);

	if(command.startsWith("D"))
	{
		pinMode(pinNumber, OUTPUT);
		analogWrite(pinNumber, value.toInt());
		return 1;
	}
	else if(command.startsWith("A"))
	{
		pinMode(pinNumber+10, OUTPUT);
		analogWrite(pinNumber+10, value.toInt());
		return 1;
	}
	else return -2;
}



