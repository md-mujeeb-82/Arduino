#include <MCP3XXX.h>

MCP3008 adc;

int phase1;
int phase2;
int phase3;

long lasttime = -1;

void setup() {
  Serial.begin(9600);
  adc.begin();
  lasttime = millis();
  Serial.println();
}

void loop(){
//  if(millis() - lasttime < 1000) {
    phase1 = adc.analogRead(0);
    phase2 = adc.analogRead(1);
    phase3 = adc.analogRead(2);
    Serial.print("Phase_1:");
    Serial.println(phase1);
    Serial.print(",");
    Serial.print("Phase_2:");
    Serial.print(phase2);
    Serial.print(",");
    Serial.print("Phase_3:");
    Serial.println(phase3);
//  }
}
