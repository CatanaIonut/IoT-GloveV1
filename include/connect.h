#ifndef CONNECT_H
#define CONNECT_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>

void setupWiFi();
void setupTime();
String nowISO8601();


///////////////////////////////////////////////////////////////////

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Arduino.h>

extern PubSubClient mqtt;

void setupMQTT();
void connectMQTT();

//pt trm  de date
void sendData(PubSubClient &mqttClient, const char* topic, const char* sensor, float value, const char* unit);


#endif // CONNECT_H
