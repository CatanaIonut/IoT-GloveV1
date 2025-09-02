#include <ArduinoBLE.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "connect.h"
#define TFT_DC   6
#define TFT_CS   7
#define TFT_RST  8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// BLE Service + Characteristic
BLEService textService("180C");
BLECharacteristic textCharacteristic("2A56", BLEWrite, 50);

String textLog = "";   // aici stocăm toate caracterele trimise

// ================= HELPER =================
void drawRoundBoxedText(String text, int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t textColor, uint16_t bgColor,
                        const GFXfont *font = NULL, uint8_t textSize = 1) {
  // fundal rotunjit
  tft.fillRoundRect(x, y, w, h, 10, bgColor);
  tft.drawRoundRect(x, y, w, h, 10, ILI9341_WHITE);
 
  // text centrat
  if (font != NULL) {
    tft.setFont(font);
  } else {
    tft.setFont();
  }
  tft.setTextSize(textSize);
  tft.setTextColor(textColor, bgColor);

  int16_t x1, y1;
  uint16_t tw, th;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);

  int tx = x + (w - tw) / 2;
  int ty = y + (h + th) / 2;

  tft.setCursor(tx, ty);
  tft.print(text);

  tft.setFont(); // reset
}

// ================= UI SCREENS =================
void showStartupScreen() {
  tft.fillScreen(ILI9341_BLACK);
  drawRoundBoxedText("Astept BLE...", 20, 60, tft.width() - 40, 60,
                     ILI9341_WHITE, ILI9341_BLUE, NULL, 2);
}

void showConnectedScreen() {
  tft.fillScreen(ILI9341_BLACK);
  drawRoundBoxedText("Conectat!", 20, 60, tft.width() - 40, 60,
                     ILI9341_WHITE, ILI9341_GREEN, NULL, 2);
}

void showPromptScreen() {
  tft.fillScreen(ILI9341_BLACK);
  drawRoundBoxedText("Trimiteti un caracter:", 20, 40, tft.width() - 40, 50,
                     ILI9341_WHITE, ILI9341_BLUE, NULL, 2);

  // caseta log jos (gri închis)
  tft.fillRoundRect(10, tft.height() - 50, tft.width() - 20, 40, 8, ILI9341_DARKGREY);
  tft.drawRoundRect(10, tft.height() - 50, tft.width() - 20, 40, 8, ILI9341_WHITE);
}

void showMainChar(String ch) {
  int boxH = tft.height() / 2;
  drawRoundBoxedText(ch, 20, (tft.height() - boxH) / 2, tft.width() - 40, boxH,
                     ILI9341_WHITE, ILI9341_NAVY, &FreeSansBold24pt7b, 1);
}

void updateLogBox() {
  int boxY = tft.height() - 50;
  int boxH = 40;
  int boxW = tft.width() - 20;

  // reumple caseta log cu gri închis
  tft.fillRoundRect(10, boxY, boxW, boxH, 8, ILI9341_DARKGREY);
  tft.drawRoundRect(10, boxY, boxW, boxH, 8, ILI9341_WHITE);

  // text alb mic
  tft.setFont();
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_DARKGREY);
  tft.setCursor(15, boxY + 25);

  tft.print(textLog);
}

// ================= SETUP =================
void setup() {
  delay(1000);
  Serial.begin(115200);
  setupWiFi();
  setupTime();
  while (!Serial);

  delay(1000);
  Serial.println(nowISO8601());
  delay(5000);
  
  tft.begin();
  tft.setRotation(1);

  showStartupScreen();

  if (!BLE.begin()) {
    Serial.println("Eroare BLE!");
    while (1);
  }

  BLE.setLocalName("PortentaH7_BT");
  BLE.setAdvertisedService(textService);
  textService.addCharacteristic(textCharacteristic);
  BLE.addService(textService);

  BLE.advertise();
  Serial.println("BLE Pornit!");
}

// ================= LOOP =================
void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Conectat la: ");
    Serial.println(central.address());

    showConnectedScreen();
    delay(2000);

    showPromptScreen();

    while (central.connected()) {
      if (textCharacteristic.written()) {
        int len = textCharacteristic.valueLength();
        const uint8_t* val = textCharacteristic.value();

        String msg = "";
        for (int i = 0; i < len; i++) {
          msg += (char)val[i];
        }

        Serial.print("Primit: ");
        Serial.println(msg);

        // actualizează caracter mare + log
        showMainChar(msg);
        textLog += msg;
        updateLogBox();
      }
    }

    Serial.println("Deconectat!");
    textLog = ""; // reset log
    showStartupScreen();
  }
}