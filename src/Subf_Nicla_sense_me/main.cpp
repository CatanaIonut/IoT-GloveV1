#include <Arduino.h>
#include <ArduinoBLE.h>
#include "Arduino_BHY2.h"
// ...existing code...

SensorQuaternion quat(SENSOR_ID_RV);

float offR=0, offP=0, offY=0;
const int DEADZONE = 30;
int dz(int v){ return (abs(v) <= DEADZONE) ? 0 : v; }

// UUID-uri BLE
static const char* SVC_RPY = "12345678-1234-5678-1234-56789abcdef0";
static const char* CHR_RPY = "12345678-1234-5678-1234-56789abcdef1";
BLECharacteristic rpyChr(CHR_RPY, BLENotify, 12);

// ...existing code...
static void calibrateRPY(int ms=1500){
  double sR=0, sP=0, sY=0; int n=0;
  unsigned long t0=millis();
  while(millis()-t0 <  (unsigned long)ms){
    BHY2.update();
    if (quat.dataAvailable()){
      float w=quat.w(), x=quat.x(), y=quat.y(), z=quat.z();
      float roll  = atan2f(2*(w*x + y*z), 1 - 2*(x*x + y*y)) * 180.0f/PI;
      float pitch = asinf (2*(w*y - z*x)) * 180.0f/PI;
      float yaw   = atan2f(2*(w*z + x*y), 1 - 2*(y*y + z*z)) * 180.0f/PI;
      sR+=roll; sP+=pitch; sY+=yaw; n++;
    }
    delay(5);
  }
  if(n>0){ offR=sR/n; offP=sP/n; offY=sY/n; }
}
// ...existing code...

void setup(){
  Serial.begin(115200);

  BHY2.begin();
  quat.begin();
  calibrateRPY(1500);

  BLE.begin();
  BLE.setLocalName("NiclaME_RPY");
  BLEService svc(SVC_RPY);
  svc.addCharacteristic(rpyChr);
  BLE.setAdvertisedService(svc);
  BLE.addService(svc);
  BLE.advertise();

  Serial.println("Nicla BLE RPY advertising...");
}

// Send values at 0.75s interval and allow setting origin via Serial 'o'
void loop(){
  static unsigned long lastSend = 0;
  const unsigned long interval = 750; // ms
  static int lastR = 0, lastP = 0, lastY = 0;

  static float curRollRaw = 0.0f, curPitchRaw = 0.0f, curYawRaw = 0.0f;

  BHY2.update();
  if (quat.dataAvailable()){
    float w=quat.w(), x=quat.x(), y=quat.y(), z=quat.z();
    float roll  = atan2f(2*(w*x + y*z), 1 - 2*(x*x + y*y)) * 180.0f/PI;
    float pitch = asinf (2*(w*y - z*x)) * 180.0f/PI;
    float yaw   = atan2f(2*(w*z + x*y), 1 - 2*(y*y + z*z)) * 180.0f/PI;

    // store raw values (before subtracting origin) so user can set origin later
    curRollRaw  = roll;
    curPitchRaw = pitch;
    curYawRaw   = yaw;

    // apply origin offsets
    roll  -= offR;  pitch -= offP;  yaw -= offY;
    int r = dz((int)lroundf(roll));
    int p = dz((int)lroundf(pitch));
    int yv= dz((int)lroundf(yaw));

    // update last values (to be sent at interval)
    lastR = r; lastP = p; lastY = yv;
  }

  // check for serial command to set origin
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("o")) {
      // set current raw orientation as origin -> future values become ~0
      offR = curRollRaw;
      offP = curPitchRaw;
      offY = curYawRaw;
      Serial.print("Originea setata. offR="); Serial.print(offR);
      Serial.print(" offP="); Serial.print(offP);
      Serial.print(" offY="); Serial.println(offY);
      // reset send timer so first published value appears immediately
      lastSend = 0;
    }
  }

  // send Serial + BLE at 0.75s cadence
  unsigned long now = millis();
  if (now - lastSend >= interval) {
    lastSend = now;
    // print to Serial
    Serial.print(lastR); Serial.print(','); Serial.print(lastP); Serial.print(','); Serial.println(lastY);

    // send via BLE as 3 floats (12 bytes)
    float out[3] = { (float)lastR, (float)lastP, (float)lastY };
    rpyChr.setValue((uint8_t*)out, sizeof(out));
  }

  // short idle
  delay(5);
}