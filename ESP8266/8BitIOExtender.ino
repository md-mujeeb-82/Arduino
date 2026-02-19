#include <PCF8574.h>

PCF8574 PCF(0x20);

void setup() {
  Serial.begin(9600);
  // Begin IO Extender
  PCF.begin(4, 5, 0);
}

void loop() {
  Serial.print(PCF.read(0) == LOW);
  Serial.print(PCF.read(1) == LOW);
  Serial.print(PCF.read(2) == LOW);
  Serial.print(PCF.read(3) == LOW);
  Serial.print(PCF.read(4) == LOW);
  Serial.print(PCF.read(5) == LOW);
  Serial.print(PCF.read(6) == LOW);
  Serial.println(PCF.read(7) == LOW);
}
