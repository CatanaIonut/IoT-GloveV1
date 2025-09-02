#include <connect.h>


  const String ssid     = "Vodafone_WiFi_Students";
  const String password = "Future8T@len!s";


  void setupWiFi(){
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.print("Connecting to WiFi ...");

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

