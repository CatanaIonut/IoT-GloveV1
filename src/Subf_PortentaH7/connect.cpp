#include "connect.h"
#include <WiFi.h>


const char *ssid="Vodafone_WIFI_Students";
const char  *password="Future8T@len!s";



void setupWiFi()
{ delay(1000);

WiFi.begin(ssid, password);

Serial.print("Connecting to WiFi..");

while (WiFi.status() !=WL_CONNECTED){
    delay(500);
    Serial.print(".");
}

Serial.println("/nConnected to WiFi");
Serial.print("IP Address: ");
Serial.println(WiFi.localIP());


}