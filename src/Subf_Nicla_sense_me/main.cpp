#include <Arduino.h>
#include <ArduinoBLE.h>
#include "Arduino_BHY2.h"
#include <Wire.h>
#include "SparkFun_Displacement_Sensor_Arduino_Library.h"

SensorQuaternion quat(SENSOR_ID_RV);

float offR=0, offP=0, offY=0;
const int DEADZONE = 30;
int dz(int v){ return (abs(v) <= DEADZONE) ? 0 : v; }

static const char* SVC_RPY = "12345678-1234-5678-1234-56789abcdef0";
static const char* CHR_RPY = "12345678-1234-5678-1234-56789abcdef1";
BLECharacteristic rpyChr(CHR_RPY, BLENotify, 12);

static const char* SVC_FLEX = "87654321-4321-8765-4321-abcdef012345";
static const char* CHR_FLEX = "87654321-4321-8765-4321-abcdef012346";
BLECharacteristic flexChr(CHR_FLEX, BLENotify, 8);

ADS myFlexSensor;

byte findI2CDevice(byte startingAddress)
{
  if (startingAddress == 0)
    startingAddress = 1;
  for (byte address = startingAddress; address < 127; address++)
  {
    Wire.beginTransmission(address);
    byte response = Wire.endTransmission();
    if (response == 0)
      return (address);
  }
  return (0);
}

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
  
  Wire.begin();
  byte currentAddress = 0;
  for (byte addr = 1; addr < 127; addr++)
  {
    addr = findI2CDevice(addr);
    if (addr == 0)
      break;
    if (myFlexSensor.begin(addr) == true)
    {
      currentAddress = addr;
      break;
    }
  }

  if (currentAddress == 0)
  {
    Serial.println(F("No Flex Sensors found on I2C bus. Check wiring and power."));
    while (1)
      ;
  }

  Serial.print(F("Flex Sensor detected at address 0x"));
  Serial.print(currentAddress, HEX);
  Serial.println();
  myFlexSensor.enableStretching(true);
  delay(200);
  Serial.println(F("Flex Sensor initialized successfully!"));
}

void loop(){
  static unsigned long lastSend = 0;
  const unsigned long interval = 750;
  static int lastR = 0, lastP = 0, lastY = 0;

  static float curRollRaw = 0.0f, curPitchRaw = 0.0f, curYawRaw = 0.0f;

  BHY2.update();
  if (quat.dataAvailable()){
    float w=quat.w(), x=quat.x(), y=quat.y(), z=quat.z();
    float roll  = atan2f(2*(w*x + y*z), 1 - 2*(x*x + y*y)) * 180.0f/PI;
    float pitch = asinf (2*(w*y - z*x)) * 180.0f/PI;
    float yaw   = atan2f(2*(w*z + x*y), 1 - 2*(y*y + z*z)) * 180.0f/PI;

    curRollRaw  = roll;
    curPitchRaw = pitch;
    curYawRaw   = yaw;

    roll  -= offR;  pitch -= offP;  yaw -= offY;
    int r = dz((int)lroundf(roll));
    int p = dz((int)lroundf(pitch));
    int yv= dz((int)lroundf(yaw));

    lastR = r; lastP = p; lastY = yv;
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("o")) {
      offR = curRollRaw;
      offP = curPitchRaw;
      offY = curYawRaw;
      Serial.print("Originea setata. offR="); Serial.print(offR);
      Serial.print(" offP="); Serial.print(offP);
      Serial.print(" offY="); Serial.println(offY);
      lastSend = 0;
    }
  }

  unsigned long now = millis();
  if (now - lastSend >= interval) {
    lastSend = now;
    
    float angle = 0.0f, stretch = 0.0f;
    if (myFlexSensor.available())
    {
      angle = myFlexSensor.getX();
      stretch = myFlexSensor.getStretchingData();
    }
    
    Serial.print("RPY: ");
    Serial.print(lastR); Serial.print(','); Serial.print(lastP); Serial.print(',');
    Serial.print(lastY); Serial.print(" | Flex: ");
    Serial.print(angle, 2); Serial.print(','); Serial.println(stretch, 4);

    byte rpy_data[12];
    memcpy(&rpy_data[0], &lastR, sizeof(int));
    memcpy(&rpy_data[4], &lastP, sizeof(int));
    memcpy(&rpy_data[8], &lastY, sizeof(int));
    rpyChr.setValue(rpy_data, 12);
    
    byte flex_data[8];
    memcpy(&flex_data[0], &angle, sizeof(float));
    memcpy(&flex_data[4], &stretch, sizeof(float));
    flexChr.setValue(flex_data, 8);
  }

  delay(5);
}
