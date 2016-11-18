#pragma once

#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H


#include "Arduino.h"

enum WifiConnectionType {
	ctAP,
	ctSTA,
};

class WifiManager {
public:
	inline WifiManager(Visualizer* aVisualizer)
	{
		this->visualizer = aVisualizer;
	}
	inline void wifiStart(String aAPssid, String aAPpassword, String aSTAssid, String aSTApassword) {
		uint8_t i = 0;
		bool bResult;

		WiFi.disconnect();
		WiFi.softAPdisconnect();
		WiFi.mode(WIFI_AP_STA);

		visualizer->show("Initializing Soft-AP",mtSerial);
		WiFi.softAP(aAPssid.c_str(), aAPpassword.c_str());
		bResult = waitForWiFiConnection(ctAP);
		if (bResult) {
			visualizer->show("AP ready!", WiFi.softAPIP().toString());
		}

		visualizer->show("Connecting to SSID: " + aSTAssid,mtSerial);
		WiFi.begin(aSTAssid.c_str(), aSTApassword.c_str());
		bResult = waitForWiFiConnection(ctSTA);
		if (bResult) {
			visualizer->show("WiFi IP" + aSTAssid, WiFi.localIP().toString());
		}
	}
private:
	Visualizer* visualizer;
	inline bool waitForWiFiConnection(WifiConnectionType connectionType)
	{
		switch (connectionType)
		{
		case ctAP:
			for (int i = 0; i < 20; i++)
			{
				if (WiFi.softAPIP().toString() == "0.0.0.0") {
					Serial.print(".");
					delay(500);
					yield();
				}
				else {
					visualizer->show("");
					visualizer->showSecondary("AP ready @ IP: " + WiFi.softAPIP().toString());
					return true;
				}
			}
			break;
		case ctSTA:
			for (int i = 0; i < 20; i++)
			{
				if (WiFi.status() != WL_CONNECTED) {
					Serial.print(".");
					delay(500);
					yield();
				}
				else {
					visualizer->show("");
					visualizer->showSecondary("STA WiFi ready @ IP: " + WiFi.localIP().toString());
					return true;
				}
			}
			break;
		default:
			break;
		}
		Serial.print(" -> timed out!\n");
		return false;
	}
};
#endif // !WIFIMANAGER_H