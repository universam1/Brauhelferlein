#include <String.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <ArduinoJson.h>
#include "PID_Handler.h"
#include "Stirrer_Handler.h"
#include "VisualizerClass.h"
#include "WebserverManagerClass.h"
#include "WiFiManagerClass.h"
#include "FileStorageClass.h"
#include "ConfigClass.h"

const String APssid = "JBLM-IoT-Wemos1";
const String APpassword = "12345678";
const String STAssid = "JBLM-IoT-Playground";
const String STApassword = "WemosPi&Co";

#define ONE_WIRE_BUS 12  // DS18B20 on ESP pin12
#define MISCHERPORT 2
#define RELAISPORT 16
float triggerC = 0.5;

#define resolution 12 // 12bit resolution == 750ms update rate
#define OWinterval (800 / (1 << (12 - resolution))) 
#define WindowSize (OWinterval * 6)
#define minWindow 150

#define OFF HIGH
#define ON LOW

#define JSONSize 150

void relais(bool state) {
	digitalWrite(RELAISPORT, state);
}

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
//      addr, en,rw,rs,d4,d5,d6,d7
LiquidCrystal lcd(27, 1, 1, 1, 1, 1); //(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

Visualizer visualizer(&lcd);
Config config(&visualizer);
WifiManager wifiManager(&visualizer);
DeviceAddress ds18B20Address;
WebserverManager webserverManager(&visualizer, &config);


void setup() {

	visualizer.show("Init Wire");
	Wire.begin(4, 5);
	visualizer.showSecondary("Wire initialized");

	// waiting for China Display 
	//visualizer.show("Start LCD");
	//lcd.begin(16, 2);
	////lcd.backlight();
	//visualizer.showSecondary("LCD started");

	wifiManager.wifiStart(APssid, APpassword, STAssid, STApassword);
	webserverManager.startServer();

	visualizer.show("DS18B20 Setup");

	// Start up the DS18B20
	if (DS18B20.getDeviceCount() > 0 && DS18B20.isConnected(0))
	{
		DS18B20.begin();
		DS18B20.getAddress(ds18B20Address, 0);
		DS18B20.setResolution(ds18B20Address, resolution);
		DS18B20.setWaitForConversion(false);
		DS18B20.requestTemperatures();
		visualizer.show("Temp. Sensor", "OK");
	}
	else
	{
		visualizer.show("Temp. Sensor", "Fehler");
		delay(1000);
	}

	// init IoT_GPIO_Gedöns :o)
	analogWriteFreq(10000);
	digitalWrite(RELAISPORT, HIGH);
	pinMode(RELAISPORT, OUTPUT);

	digitalWrite(MISCHERPORT, HIGH);
	pinMode(MISCHERPORT, OUTPUT);

	config.load();
	windowStartTime = millis();
	//tell the PID to range between 0 and the full window size
	pid.SetOutputLimits(minWindow, WindowSize);
	pid.SetTunings(configStore.consKp, configStore.consKi, configStore.consKd);
	pid.SetSampleTime(OWinterval);
	//turn the PID on
	resetPID();

	delay(2000);
	lcd.clear();
}


void loop() {


	if (shouldReboot) {
		Serial.println("Rebooting...");
		delay(100);
		ESP.restart();
	}


	// We run yet when we have new Temperatur data from DS18B20
	if (millis() - lastrun > OWinterval) {
		lastrun = millis();

		float tnew = DS18B20.getTempCByIndex(0);
		DS18B20.requestTemperatures();

		// test if we have valid temp reading
		if (tnew == DEVICE_DISCONNECTED_C || // DISCONNECTED
			(tnew == 85.0 && abs(tnew - temperature) > 2)) // we read 85 uninitialized but we are not close to 85 hot
		{
			Serial.println("DISCONNECTED");
			lcd.setCursor(0, 0); lcd.print("-");
		}
		else
		{
			temperature = tnew;

			uint32_t passedT = 0;

			// calculate the time we are set in
			if (lastSetpoint != 0) passedT = (millis() - lastSetpoint) / 1000;
			else if (abs(temperature - configStore.solltemp) < triggerC) // or we just reached it
			{
				lastSetpoint = millis();
				// reset the PID error here
				resetPID();
			}

			// pid.Compute();

			if (millis() - windowStartTime > WindowSize)
			{ //time to shift the Relay Window and recalculate 
				windowStartTime = millis();
				pid.Compute();
			}

			updateWebclientAndLcd(passedT);

		}
	}

	// Translation of Power to Relay interval
	if (Output > minWindow && Output > millis() - windowStartTime) relais(ON);
	else relais(OFF);

	// Mischer interval calculation
	static uint32_t lastmischer;
	if (millis() - lastmischer > 1000) {
		lastmischer = millis();
		if (mischerOn && (millis() / 1000) % (mischerOn + mischerOff) < mischerOn) {
			analogWrite(MISCHERPORT, 10.23 * (100 - mischerPower));
		}
		else {
			digitalWrite(MISCHERPORT, true);
		}
	}
}

void updateWebclientAndLcd(uint32_t aPassedT)
{
	char buf[10];
	StaticJsonBuffer<JSONSize> jsonBuffer;

	JsonObject& root = jsonBuffer.createObject();

	snprintf(buf, sizeof(buf), "%03d:%02d", aPassedT / 60, aPassedT % 60);
	root["timer"] = jsonBuffer.strdup(buf);
	lcd.setCursor(10, 1); lcd.print(buf);

	Serial.print("T: ");  Serial.println(temperature);
	snprintf(buf, sizeof(buf), "%6.2f", temperature);
	lcd.setCursor(0, 0); lcd.print("T:"); lcd.print(buf); lcd.print((char)223);
	root["temp"] = temperature;

	snprintf(buf, sizeof(buf), "%6.2f", configStore.solltemp);
	lcd.setCursor(0, 1); lcd.print("S:"); lcd.print(buf); lcd.print((char)223);
	root["sollT"] = configStore.solltemp;

	int8_t load = (Output - minWindow)*100.0 / (WindowSize - minWindow);
	snprintf(buf, sizeof(buf), "%3u%%", load);
	lcd.setCursor(12, 0);
	lcd.print(buf);
	root["load"] = load;

	// We send JSON data to the client
	char buffer[JSONSize];
	root.printTo(buffer, sizeof(buffer));
	webserverManager.events.send(buffer, "message");
}
