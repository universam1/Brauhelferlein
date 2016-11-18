#pragma once

#ifndef WEBSERVERMANAGERCLASS_H
#define WEBSERVERMANAGERCLASS_H


#include <String.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FileStorageClass.h"
#include "ConfigClass.h"
#include "StringHelper.h"
#include "PID_Handler.h"
#include "Stirrer_Handler.h"
#include "Arduino.h"

class WebserverManager {
public:
	
	AsyncEventSource events = AsyncEventSource("/events");
	Visualizer* visualizer;
	inline WebserverManager(Visualizer* aVisualizer, Config* aConfig)
	{
		this->visualizer = aVisualizer;
		this->config = aConfig;
	}
	inline void startServer()
	{
		this->visualizer->show("Starting Webserver @ Port 80");
		fileStorage = FileStorage(visualizer);
		server = AsyncWebServer(80);
		fileStorage.Initialize();
		this->InitWebserverAndEvents();
		server.begin();
		this->visualizer->showSecondary("Webserver started");
	}
private:
	FileStorage fileStorage = NULL;
	Config* config;
	AsyncWebServer server = NULL;
	inline void InitWebserverAndEvents()
	{
		// Start Web server configuration
		events.onConnect([this](AsyncEventSourceClient *client) {
			if (client->lastId()) {
				this->visualizer->show("Client reconnected! Last message ID that it got is: " + client->lastId());
			}});
		server.addHandler(&events);

		// respond to GET requests on URL /heap
		server.on("/heap", HTTP_GET, [this](AsyncWebServerRequest *request) {
			request->send(200, "text/plain", String(ESP.getFreeHeap()));
		});
		// respond to GET requests on URL homepage
		server.on("/", HTTP_ANY, [this](AsyncWebServerRequest *request) {

			//Check if POST (but not File) parameter exists
			if (request->hasParam("Tsoll", true)) {
				AsyncWebParameter* p = request->getParam("Tsoll", true);
				if (configStore.solltemp != p->value().toFloat()) {
					configStore.solltemp = p->value().toFloat();
					lastSetpoint = 0;
				}
			}
			if (request->hasParam("Mon", true)) {
				AsyncWebParameter* p;
				p = request->getParam("Mon", true);
				mischerOn = p->value().toInt();

				p = request->getParam("Moff", true);
				mischerOff = p->value().toInt();

				p = request->getParam("MPw", true);
				mischerPower = p->value().toInt();
			}

			AsyncResponseStream *response = request->beginResponseStream("text/html");

			response->addHeader("Server", "ESP Async Web Server");
			response->print(fileStorage.getHtmlContentFromFile("index.html"));
			request->send(response);
		});


		// respond to  requests on URL /settings for PID settings
		server.on("/settings", HTTP_ANY, [this](AsyncWebServerRequest *request) {

			//Check if POST (but not File) parameter exists
			if (request->hasParam("Kp", true)) {
				configStore.consKp = request->getParam("Kp", true)->value().toFloat();
				configStore.consKi = request->getParam("Ki", true)->value().toFloat();
				configStore.consKd = request->getParam("Kd", true)->value().toFloat();
				pid.SetTunings(configStore.consKp, configStore.consKi, configStore.consKd);
				resetPID();
				config->save();
			}

			AsyncResponseStream *response = request->beginResponseStream("text/html");

			response->addHeader("Server", "ESP Async Web Server");
			response->printf(fileStorage.getHtmlContentFromFile("settings.html").c_str(), Output, pid.GetKp(), pid.GetKi(), pid.GetKd());
			request->send(response);
		});
		// Simple Firmware Update Form
		server.on("/update", HTTP_GET, [this](AsyncWebServerRequest *request) {
			request->send(200, "text/html", fileStorage.getHtmlContentFromFile("update.html"));
		});

		server.on("/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
			shouldReboot = !Update.hasError();
			AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
			response->addHeader("Connection", "close");
			request->send(response);
		}, [this](AsyncWebServerRequest *request, String filename, std::size_t index, uint8_t *data, std::size_t len, bool final) {
			if (!index) {
				Serial.printf("Update Start: %s\n", filename.c_str());
				//WiFiUDP::stopAll();
				Update.runAsync(true);
				if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
					Update.printError(Serial);
				}
			}
			if (!Update.hasError()) {
				if (Update.write(data, len) != len) {
					Update.printError(Serial);
				}
			}
			if (final) {
				if (Update.end(true)) {
					Serial.printf("Update Success: %uB\n", index + len);
				}
				else {
					Update.printError(Serial);
				}
			}
		});
		this->visualizer->showSecondary("Server URIs and Events assigned");
	}
};

#endif // !WEBSERVERMANAGERCLASS_H