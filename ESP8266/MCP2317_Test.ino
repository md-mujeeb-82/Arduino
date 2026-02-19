#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp;

void setup() {
  Serial.begin(9600);
  Serial.println("MCP23xxx Blink Test!");

  if (!mcp.begin_I2C()) {
    Serial.println("Error.");
    while (1);
  }

  // configure pin for output
  for(int i=0; i<16; i++) {
    mcp.pinMode(i, OUTPUT);
  }

  Serial.println("Looping...");
}

void loop() {
  
  for(int i=0; i<15; i++) {
    mcp.digitalWrite(i, LOW);
    mcp.digitalWrite(15-i, LOW);
    delay(30);
    mcp.digitalWrite(i, HIGH);
    mcp.digitalWrite(15-i, HIGH);
    delay(30);
  }

//  for(int i=14; i>1; i--) {
//    mcp.digitalWrite(i, LOW);
//    mcp.digitalWrite(i-1, LOW);
//    delay(30);
//    mcp.digitalWrite(i, HIGH);
//    mcp.digitalWrite(i-1, HIGH);
//    delay(30);
//  }
}
