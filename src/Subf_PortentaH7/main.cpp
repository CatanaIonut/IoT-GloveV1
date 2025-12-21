#include <Arduino.h>
#include <SPI.h>
#include <ArduinoBLE.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "connect.h"   // WiFi, NTP, MQTT, sendData()

// ==== TFT pins ====
#define TFT_DC   6
#define TFT_CS   7
#define TFT_RST  8
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// ==== UUID-urile serviciului/characteristicii de pe Nicla ====
static const char* SVC_RPY = "12345678-1234-5678-1234-56789abcdef0";
static const char* CHR_RPY = "12345678-1234-5678-1234-56789abcdef1";

// ==== BLE Central state ====
BLEDevice nicla;
BLECharacteristic rpyChr;

bool rpyConnected = false;
unsigned long lastDraw = 0;

// ====== UI helpers ======
static void drawHeader(const char* status, uint16_t color) {
  tft.fillRect(0, 0, tft.width(), 30, color);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK, color);
  tft.setCursor(8, 7);
  tft.print(status);
}

static void drawSearchingScreen() {
  tft.fillScreen(ILI9341_BLACK);
  drawHeader("Caut NiclaME_RPY...", ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(16, 60);
  tft.print("Porneste Nicla si BLE");
}

static void drawConnectedScreen() {
  tft.fillScreen(ILI9341_BLACK);
  drawHeader("Conectat la Nicla", ILI9341_GREEN);

  // etichete
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(16, 60);  tft.print("Roll :");
  tft.setCursor(16, 100); tft.print("Pitch:");
  tft.setCursor(16, 140); tft.print("Yaw  :");

  // casete valori
  tft.drawRoundRect(110, 48, 190, 32, 6, ILI9341_WHITE);
  tft.drawRoundRect(110, 88, 190, 32, 6, ILI9341_WHITE);
  tft.drawRoundRect(110, 128, 190, 32, 6, ILI9341_WHITE);
}

static void drawRPY(float r, float p, float y) {
  auto drawBox = [&](int y0, float v) {
    tft.fillRect(112, y0+2, 186, 28, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    char buf[32];
    snprintf(buf, sizeof(buf), "%4.0f deg", v);   // tu trimiți întregi ca float
    tft.setCursor(120, y0+10);
    tft.print(buf);
  };
  drawBox(48,  r);
  drawBox(88,  p);
  drawBox(128, y);
}

static void mqttPublishRPY(float r, float p, float y) {
  
  String topic1 = String("tenants/") + "beta" + "/glove/PortentaH7/Roll";
  String topic2 = String("tenants/") + "beta" + "/glove/PortentaH7/Pitch";
  String topic3 = String("tenants/") + "beta" + "/glove/PortentaH7/Yaw";

  sendData(mqtt, topic1.c_str(), "roll",  r, "deg");
  sendData(mqtt, topic2.c_str(), "pitch", p, "deg");
  sendData(mqtt, topic3.c_str(), "yaw",   y, "deg");
}

// ====== BLE: scan & connect ======
static void scanAndConnectNicla() {
  if (rpyConnected) return;

  BLEDevice dev = BLE.available();         // valabil cât timp scanăm
  if (!dev) return;

  // debug: vezi ce găsește
  Serial.print("Gasit: ");
  if (dev.hasLocalName()) Serial.println(dev.localName());
  else Serial.println("(fara nume)");

  bool match = (dev.hasLocalName() && String(dev.localName()) == "NiclaME_RPY");
  if (!match) return;

  Serial.println(">> Nicla gasita, incerc conectare...");
  BLE.stopScan();

  if (!dev.connect()) {
    Serial.println("Conectare esuata!");
    BLE.scan();
    return;
  }

  Serial.println("Conectat, descopar atribute...");
  if (!dev.discoverAttributes()) {
    Serial.println("Nu pot descoperi atribute!");
    dev.disconnect();
    BLE.scan();
    return;
  }

  rpyChr = dev.characteristic(CHR_RPY);
  if (!rpyChr) {
    Serial.println("Caracteristica lipsa!");
    dev.disconnect(); BLE.scan(); return;
  }
  if (!rpyChr.canSubscribe()) {
    Serial.println("Caracteristica nu permite subscribe!");
    dev.disconnect(); BLE.scan(); return;
  }
  if (!rpyChr.subscribe()) {
    Serial.println("Subscribe esuat!");
    dev.disconnect(); BLE.scan(); return;
  }

  nicla = dev;
  rpyConnected = true;
  Serial.println("Conectat la NiclaME_RPY");
  drawConnectedScreen();
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(200);

  // --- WiFi + timp + MQTT (NE-MODIFICATE) ---
  setupWiFi();
  setupTime();
  setupMQTT();
  connectMQTT();

  // --- TFT ---
  tft.begin();
  tft.setRotation(1);
  drawSearchingScreen();

  // --- BLE Central ---
  if (!BLE.begin()) {
    Serial.println("Eroare BLE!");
    while (1) {}
  }
  Serial.println("Scanez pentru NiclaME_RPY...");
  BLE.scan();   // rezultatele se citesc cu BLE.available() în loop
}

// ================= LOOP =================
void loop() {
  // --- menține WiFi/MQTT active (fără a modifica implementarea ta) ---
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) connectMQTT();
    mqtt.loop();
  }

  // --- conexiune BLE către Nicla ---
  if (!rpyConnected) {
    scanAndConnectNicla();
  } else if (!nicla.connected()) {
    // s-a pierdut conexiunea
    rpyConnected = false;
    drawSearchingScreen();
    BLE.scan();
  }

  // --- când vin date noi din caracteristică (12 bytes: 3 float-uri) ---
  if (rpyConnected && rpyChr.valueUpdated()) {
    float buf[3];
    int n = rpyChr.readValue((uint8_t*)buf, sizeof(buf));
    if (n == sizeof(buf)) {
      float roll = buf[0];
      float pitch = buf[1];
      float yaw = buf[2];

      // debug serial
      Serial.print("RX rpy = ");
      Serial.print(roll); Serial.print(',');
      Serial.print(pitch); Serial.print(',');
      Serial.println(yaw);

      // update TFT ~20 Hz
      unsigned long now = millis();
      if (now - lastDraw > 50) {
        drawRPY(roll, pitch, yaw);
        lastDraw = now;
      }

      // publish MQTT ~5 Hz
      static unsigned long lastMQTT = 0;
      if (now - lastMQTT > 2000) {
        mqttPublishRPY(roll, pitch, yaw);
        lastMQTT = now;
      }
    }
  }

  // BLE housekeeping
  BLE.poll();
}