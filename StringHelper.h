//#pragma once
//
//#include <memory>
//#include <iostream>
//#include <String>
//#include "Arduino.h"
//
//template<typename ... Args>
//String String_format(const String& format, Args ... args)
//{
//
//	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
//	std::unique_ptr<char[]> buf(new char[size]);
//	snprintf(buf.get(), size, format.c_str(), args ...);
//	auto combined = std::string(buf.get(), buf.get() + size - 1).c_str();
//	return String(combined);
//}