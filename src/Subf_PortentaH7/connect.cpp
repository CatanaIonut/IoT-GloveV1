#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "secret.h"   // ssid, pass
#include "connect.h"

// offset pentru RO: UTC+2 (iarna) = 7200; vara + DST = 10800
static const long kTimeOffset = 10800;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", kTimeOffset, 60*60*1000); // update la 1h

void setupWiFi() {
  Serial.print("Connecting to WiFi..");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());
}

void setupTime() {
  timeClient.begin() ;
  // forțează prima sincronizare
  while (!timeClient.update()) timeClient.forceUpdate();
  Serial.println("Time synced from NTP");
}

// Timp în ISO8601 (UTC+offset setat mai sus)
String nowISO8601() {
  time_t epoch = timeClient.getEpochTime();
  struct tm *t = gmtime(&epoch);  // gmtime pe epoch+offset intern
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", t);
  return String(buf);
}


const char* MQTT_HOST = "mqtt.aiot-xplorer.eu"; // Ip broker
const uint16_t MQTT_PORT = 1883;

WiFiClient portentaClient; // protentaClient
PubSubClient mqtt(portentaClient);

void setupMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqtt.connect("portentaClient")) {
      Serial.println(" connected");
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
}
void sendData(PubSubClient &mqttClient, const char* topic, const char* sensor, float value, const char* unit)
{
  StaticJsonDocument<200> doc;
  doc["ts"] = nowISO8601();
  doc["value"] = value;
  doc["unit"] = unit;
  doc["sensor"] = sensor;
char buffer[200];
  size_t n = serializeJson(doc, buffer);

mqttClient.publish(topic, buffer, n);

  Serial.println("Published to : ");
  Serial.print(topic);
  Serial.print(", ");
  Serial.print(buffer);



}