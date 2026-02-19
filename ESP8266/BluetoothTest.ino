//This example code is in the Public Domain (or CC0 licensed, at your option.)
//By Evandro Copercini - 2018
//
//This example creates a bridge between Serial and Classical Bluetooth (SPP)
//and also demonstrate that SerialBT have the same functionalities of a normal Serial

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

#define PIN_LED 4
#define PIN_VIBRATOR 3
#define PIN_BUZZER 5

BluetoothSerial SerialBT;
unsigned char buffer1[32];
char data1[] = "understood\n";

void setup() {
  SerialBT.begin("Smart Tasbeeh"); //Bluetooth device name
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_VIBRATOR, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_VIBRATOR, HIGH);
  digitalWrite(PIN_BUZZER, HIGH);
}

void loop() {

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
  delay(20);
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
