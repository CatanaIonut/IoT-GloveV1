#include "Arduino_BHY2.h"

SensorQuaternion quat(SENSOR_ID_RV);

float offR=0, offP=0, offY=0;     // offseturi calibrate
const int DEADZONE = 30;          // prag “aprox 0”
int dz(int v){ return (abs(v) <= DEADZONE) ? 0 : v; }

// calibrare: media pe ~1.5s (mana statică)
void calibrare(int ms=1500){
  double sR=0,sP=0,sY=0; int n=0;
  unsigned long t0=millis();
  while(millis()-t0 < (unsigned long)ms){
    BHY2.update();
    if (quat.dataAvailable()){            // FĂRĂ quat.read()
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
  calibrare(1500); // 1.5s cu mana nemiscata
}

void loop(){
  BHY2.update();
  if (quat.dataAvailable()){              // FĂRĂ quat.read()
    float w=quat.w(), x=quat.x(), y=quat.y(), z=quat.z();
    float roll  = atan2f(2*(w*x + y*z), 1 - 2*(x*x + y*y)) * 180.0f/PI;
    float pitch = asinf (2*(w*y - z*x)) * 180.0f/PI;
    float yaw   = atan2f(2*(w*z + x*y), 1 - 2*(y*y + z*z)) * 180.0f/PI;

    roll  -= offR;  pitch -= offP;  yaw -= offY;     // scade offsetul
    int r = dz((int)lroundf(roll));                  // rotunjire + deadzone
    int p = dz((int)lroundf(pitch));
    int yv= dz((int)lroundf(yaw));

    Serial.print(r); Serial.print(',');
    Serial.print(p); Serial.print(',');
    Serial.println(yv);
  }
}
