#pragma once

#ifndef FILESTORAGECLASS_H
#define FILESTORAGECLASS_H


#include <FS.h>
#include "Arduino.h"

class FileStorage {
public:
	inline FileStorage(Visualizer* aVisualizer)
	{
		this->visualizer = aVisualizer;
	}

	inline void Initialize()
	{
		visualizer->show("Init SPIFFS", mtSerial_LCD);
		if (!SPIFFS.begin()) visualizer->showSecondary("begin() failed!");
		if (!SPIFFS.format()) visualizer->showSecondary("format() failed!");
		this->visualizer->showSecondary("SPIFFS initialized");
		isInitialized = true;
	}

	inline String getHtmlContentFromFile(String aFileName)
	{
		return this->getTextfileContent(aFileName,"/html/");
	}

private:
	Visualizer* visualizer;
	bool isInitialized;

	inline String getTextfileContent(String aFileName, String aPath)
	{
		if (!isInitialized)
			this->Initialize();
		String result = "";
		String file = aPath + aFileName;

		File fileHandle = fileHandle = SPIFFS.open(file, "r");
		if (!fileHandle)
		{
			visualizer->show("File.open() failed for file: " + aFileName);
			return  "";
		}
		while (fileHandle.available())
		{
			result += fileHandle.readStringUntil('\n');
		}
		fileHandle.close();
		return result;
	}

};

#endif // !FILESTORAGECLASS_H
