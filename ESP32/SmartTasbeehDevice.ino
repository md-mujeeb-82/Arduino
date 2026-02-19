#include <WiFi.h>
#include <ArduinoOTA.h>
#include "BluetoothSerial.h"

const char* ssid = "Smart Tasbeeh";

#define PIN_LED 4
#define PIN_VIBRATOR 3
#define PIN_BUZZER 5
#define PIN_BUTTON 1

#define DATA_CHECK_INTERVAL 20

BluetoothSerial SerialBT;
unsigned char buffer1[5];
long lastTimeDataCheck = -200;
long lastTimeButtonPress = 0;
bool isConfigMode = false;

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_VIBRATOR, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP); // Button
  
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_VIBRATOR, HIGH);
  digitalWrite(PIN_BUZZER, HIGH);

  if(digitalRead(PIN_BUTTON) == LOW) {
    isConfigMode = true;
  }

  if(isConfigMode) {
    WiFi.softAP(ssid);
    delay(5000);
  
    // Port defaults to 8266
    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname(ssid);
  
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_FS
        type = "filesystem";
      }
    });
  
    ArduinoOTA.begin(); 
  } else {
    
    SerialBT.begin(ssid); //Bluetooth device name
  }
}

void loop() {

  if(isConfigMode) {
    // Handle the OTA Functionality
    ArduinoOTA.handle();
    
  } else {

    long currentTime = millis();
    lastTimeButtonPress = currentTime;
  
    if(digitalRead(PIN_BUTTON) == LOW) {
      lastTimeButtonPress = currentTime;

      while(digitalRead(PIN_BUTTON) == LOW) {
        // Do Nothing
      }
      
      currentTime = millis();
    }

    // Button Pressed, send Toggle Command to Phone
    if(currentTime - lastTimeButtonPress > 50) {
      SerialBT.write(buffer1[0]);
    }
    
    if(currentTime - lastTimeDataCheck > DATA_CHECK_INTERVAL) {
      int available1 = 0;
      available1 = SerialBT.available();
      if (available1) {
    
        for(int i=0; i<available1; i++) {
          buffer1[i] = SerialBT.read();
        }
        
        if(buffer1[0] == '0') {
          blinkLED(30, buffer1[1] == '1', buffer1[2] == '1', 1);
          
        } else if(buffer1[0] == '1') {
          blinkLED(30, buffer1[1] == '1', buffer1[2] == '1', 2);
          
        }else if(buffer1[0] == '2') {
          tasbeehCompleteSound(buffer1[1] == '1', buffer1[2] == '1');
          
        }
      }
      lastTimeDataCheck = currentTime;
    }
  }
}

void blinkLED(int millisecs, bool withSound, bool withVibration, int count) {
  for(int i=0; i<count; i++) {
    digitalWrite(PIN_LED, HIGH);
    if(withVibration) {
      digitalWrite(PIN_VIBRATOR, LOW);
    }
    if(withSound) {
      digitalWrite(PIN_BUZZER, LOW);
    }
    delay(millisecs);
    digitalWrite(PIN_LED, LOW);
    if(withVibration) {
      digitalWrite(PIN_VIBRATOR, HIGH);
    }
    if(withSound) {
      digitalWrite(PIN_BUZZER, HIGH);
    }
    if(i < count-1) {
      delay(millisecs);
    }
  }
}

void tasbeehCompleteSound(bool withSound, bool withVibration) {
  for(int i=0; i<3; i++){
    digitalWrite(PIN_LED, HIGH);
    if(withVibration) {
      digitalWrite(PIN_VIBRATOR, LOW);
    }
    if(withSound) {
      digitalWrite(PIN_BUZZER, LOW);
    }
    delay(30);
    digitalWrite(PIN_LED, LOW);
    if(withVibration) {
      digitalWrite(PIN_VIBRATOR, HIGH);
    }
    if(withSound) {
      digitalWrite(PIN_BUZZER, HIGH);
    }
    delay(30);
  }
  digitalWrite(PIN_LED, HIGH);
  if(withVibration) {
    digitalWrite(PIN_VIBRATOR, LOW);
  }
  if(withSound) {
    digitalWrite(PIN_BUZZER, LOW);
  }
  delay(1000);
  digitalWrite(PIN_LED, LOW);
  if(withVibration) {
    digitalWrite(PIN_VIBRATOR, HIGH);
  }
  if(withSound) {
    digitalWrite(PIN_BUZZER, HIGH);
  }
}
