#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <TinyGPS++.h>

// Logging
void logToBoth(const String &message);

// GPS Utilities
String formatGpsTimestamp(TinyGPSDate &d, TinyGPSTime &t);
String createPayload(double lat, double lon, const String &timestamp, const String &msgId);

// SMS Utilities
bool sendSMSToNumber(const char *toNumber, const String &message);
bool sendSMSToAll(const String &message);

// Command Processing
void processKeyboardCommand(String command);

#endif
