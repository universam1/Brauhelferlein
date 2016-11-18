#pragma once

#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <String>
#include "Arduino.h"

enum MessageLevel {
	mlPrimary,
	mlSecondary,
};

enum MessageTarget {
	mtSerial,
	mtLCD,
	mtSerial_LCD,
};

class Visualizer {
public:
	
	inline Visualizer(LiquidCrystal* aLCD)
	{
		this->lcd = aLCD;
		Serial.begin(115200);
	}
	inline void show(String aMessage)
	{
		this->show(aMessage, mlPrimary, mtSerial);
	}
	inline void show(String aPrimaryMessage, String aSecondaryMessage)
	{
		this->show(aPrimaryMessage, mlPrimary, mtSerial_LCD);
		this->show(aSecondaryMessage, mlSecondary, mtSerial_LCD);
	}
	inline void showSecondary(String aSecondaryMessage, MessageTarget aMessageTarget = mtSerial_LCD)
	{
		this->show(aSecondaryMessage, mlSecondary, aMessageTarget);
	}
	inline void show(String aMessage, MessageTarget aTarget)
	{
		this->show(aMessage, mlPrimary, aTarget);
	}
	inline void show(String aMessage, MessageLevel aLevel)
	{
		this->show(aMessage, aLevel, mtSerial);
	}
	inline void show(String aMessage, MessageLevel aLevel, MessageTarget aTarget)
	{
		if (!this->lcd && aTarget == mtLCD)
			return;
		if (!this->lcd && aTarget == mtSerial_LCD)
			aTarget = mtSerial;
		if (aTarget == mtSerial || mtSerial_LCD)
		{
			String prefix = aLevel == mlPrimary ? "" : " -> ";
			Serial.println((prefix + aMessage).c_str());
		}
		if (aTarget == mtLCD || mtSerial_LCD)
		{
			int line = aLevel == mlPrimary ? 0 : 1;
			if (aLevel == mlPrimary)
				lcd->clear();
			lcd->setCursor(0, line);
			lcd->print(aMessage.c_str());
		}
	}
private:
	LiquidCrystal *lcd;

};

#endif // !VISUALIZER_H