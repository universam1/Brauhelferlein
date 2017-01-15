#pragma once

#ifndef CONFIG_H
#define CONFIG_H



#include <EEPROM.h>
#include "VisualizerClass.h"
#include "Arduino.h"

#define CONFIG_VERSION 1
#define CONFIG_START 10

struct StoreStruct {
	uint8_t version;
	double solltemp, consKp, consKi, consKd;
} configStore;

bool shouldReboot = false;


class Config {
public:
	Config(Visualizer* aVisualizer) {
		this->visualizer = aVisualizer;
		EEPROM.begin(512);
	}
    
	void save() {
		if(visualizer) visualizer->show("EE save: ",mtSerial_LCD);
		EEPROM.put(CONFIG_START, configStore);
		EEPROM.commit();
		if (visualizer) visualizer->showSecondary("OK");
	}

	void load() {
		if (visualizer) visualizer->show("EE load: ", mtSerial_LCD);
		EEPROM.get(CONFIG_START, configStore);

		if (configStore.version != CONFIG_VERSION)
		{
			configStore.version = CONFIG_VERSION;
			configStore.solltemp = 24;
			configStore.consKp = 5000;
			configStore.consKi = 0;
			configStore.consKd = 0;
			save();
		}
	}
private:
	Visualizer* visualizer;
};

#endif // !CONFIG_H