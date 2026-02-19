#include <WiFi.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include "BluetoothSerial.h"

#define DEVICE_NAME "Smart Tasbeeh"
#define IDENTIFIER 7867

#define PIN_LED 4
#define PIN_VIBRATOR 3
#define PIN_BUZZER 5
#define PIN_BUTTON 1

#define DATA_CHECK_INTERVAL 20
#define RADIO_TURNOFF_TIMEOUT        60000    // 1 Minute of no activity
#define AP_MODE_ACTIVATION_TIMEOUT   9000
#define TIMEOUT_RESET                1500
#define BUZZER_FREQUENCY             150

#define HEADER_REQUEST_DATA_COUNTS_1 'A'
#define HEADER_REQUEST_DATA_COUNTS_2 'B'
#define HEADER_REQUEST_DATA_COUNTS_3 'C'
#define HEADER_REQUEST_DATA_COUNTS_4 'D'
#define HEADER_REQUEST_DATA_CONFIG_1 'E'
#define HEADER_REQUEST_DATA_CONFIG_2 'F'
#define HEADER_REQUEST_DATA_CONFIG_3 'G'
#define HEADER_REQUEST_SAVE_COUNTS_1 'H'
#define HEADER_REQUEST_SAVE_COUNTS_2 'I'
#define HEADER_REQUEST_SAVE_COUNTS_3 'J'
#define HEADER_REQUEST_SAVE_COUNTS_4 'K'
#define HEADER_REQUEST_SAVE_CONFIG_1 'L'
#define HEADER_REQUEST_SAVE_CONFIG_2 'M'
#define HEADER_REQUEST_SAVE_CONFIG_3 'N'

#define HEADER_REPLY_DATA_COUNTS_1 'A'
#define HEADER_REPLY_DATA_COUNTS_2 'B'
#define HEADER_REPLY_DATA_COUNTS_3 'C'
#define HEADER_REPLY_DATA_COUNTS_4 'D'
#define HEADER_REPLY_DATA_CONFIG_1 'E'
#define HEADER_REPLY_DATA_CONFIG_2 'F'
#define HEADER_REPLY_DATA_CONFIG_3 'G'
#define HEADER_REPLY_SAVE_COUNTS_1 'H'
#define HEADER_REPLY_SAVE_COUNTS_2 'I'
#define HEADER_REPLY_SAVE_COUNTS_3 'J'
#define HEADER_REPLY_SAVE_COUNTS_4 'K'
#define HEADER_REPLY_SAVE_CONFIG_1 'L'
#define HEADER_REPLY_SAVE_CONFIG_2 'M'
#define HEADER_REPLY_SAVE_CONFIG_3 'N'

BluetoothSerial SerialBT;
long lastTimeButtonPress = 0;
long lastTimeAutoTasbeehTick = 0;
long lastTimeRadioActivity = 0;
bool isAutoPilotOn = false;
bool isBluetoothMode = false;
bool isAPMode = false;

// The Data Structure
struct Data {
  int identifier;
  int step = 1;
  int count = 0;
  int dayCount = 0;
  int totalCount = 0;
  int targetCount = 200;
  int autoTasbeehTickTime = 2000;
  int minimumTasbeehTickTime = 2000;
  bool isBuzzerOn = true;
  bool isVibratorOn = true;
  bool isAutoPilotMode = false;
} data;

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_VIBRATOR, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP); // Button
  
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_VIBRATOR, HIGH);
  analogWrite(PIN_BUZZER, 255);

  if(digitalRead(PIN_BUTTON) == LOW) {
    isBluetoothMode = true;
  }

  EEPROM.begin(sizeof(Data));

  // Restore stored data from EEPROM
  restoreEEPROMData();

  // Invalid data, store the data for the first time
  if(data.identifier != IDENTIFIER) {
    data.identifier = IDENTIFIER;
    data.step = 1;
    data.count = 0;
    data.dayCount = 0;
    data.totalCount = 0;
    data.targetCount = 200;
    data.autoTasbeehTickTime = 2000;
    data.minimumTasbeehTickTime = 2000;
    data.isBuzzerOn = true;
    data.isVibratorOn = true;
    data.isAutoPilotMode = false;
    storeEEPROMData();
  }

  if(isBluetoothMode) {
    blinkLED(100, 1);
    
    // Start the Bluetooth if required
    SerialBT.begin(DEVICE_NAME); //Bluetooth device name
    lastTimeRadioActivity = millis();
  }
}

void loop() {

  if(isAPMode) {
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

    if(isBluetoothMode && (currentTime - lastTimeButtonPress > AP_MODE_ACTIVATION_TIMEOUT)) {

        SerialBT.end();
        isBluetoothMode = false;
        blinkLED(2000, 1);
        
        WiFi.softAP(DEVICE_NAME);
        delay(500);
      
        // Port defaults to 8266
        ArduinoOTA.setPort(8266);
        ArduinoOTA.setHostname(DEVICE_NAME);
      
        ArduinoOTA.onStart([]() {
          String type;
          if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
          } else { // U_FS
            type = "filesystem";
          }
        });
      
        ArduinoOTA.begin();
        isAPMode = true;
        
    } else if(currentTime - lastTimeButtonPress > TIMEOUT_RESET) {
      // Reset Count
      data.dayCount = data.dayCount + data.count;
      data.count = 0;
      storeEEPROMData();
      blinkLED(400, 2);
        
    } else if(currentTime - lastTimeButtonPress > 50) {

      // Toggle AutoPilot
      if(data.isAutoPilotMode) {
        if(isAutoPilotOn) {
          isAutoPilotOn = false;
          blinkLED(20, 5);          // Blink fast 5 times
        } else {
          if(data.count + data.step > data.targetCount) {
            isAutoPilotOn = false;
            blinkLED(1000, 1);
          } else {
            isAutoPilotOn = true;
            blinkLED(20, 3);        // Blink fast 3 times
          }
        }
        lastTimeAutoTasbeehTick = currentTime;
        
      } else {
        incrementTasbeeh();
      }
    }

    // Gradually reduce tick time in 50 ticks, till it is equal to minimumTasbeehTickTime
    int autoPilotTickDelay = data.autoTasbeehTickTime;

    if(data.autoTasbeehTickTime != data.minimumTasbeehTickTime) {
      autoPilotTickDelay = autoPilotTickDelay - ((data.minimumTasbeehTickTime / 50) * data.count);
      if(autoPilotTickDelay < data.minimumTasbeehTickTime) {
        autoPilotTickDelay = data.minimumTasbeehTickTime;
      }
    }
  
    // AutoPilot Thread
    if(data.isAutoPilotMode && isAutoPilotOn && currentTime - lastTimeAutoTasbeehTick > autoPilotTickDelay) {
  
      incrementTasbeeh();
      lastTimeAutoTasbeehTick = currentTime;
    }

    if(isBluetoothMode) {
      int bytesRead = 0;
      byte buffer1[1024];
      
      if (SerialBT.available()) {

        lastTimeRadioActivity = millis();

        while(SerialBT.available()) {
          buffer1[bytesRead] = SerialBT.read();
          bytesRead++;
        }

        switch((char)buffer1[0]) {
          case HEADER_REQUEST_DATA_COUNTS_1: 
              SerialBT.write((byte)HEADER_REPLY_DATA_COUNTS_1);
              writeIntegerToBluetooth(data.count);
              break;
          case HEADER_REQUEST_DATA_COUNTS_2: 
              SerialBT.write((byte)HEADER_REPLY_DATA_COUNTS_2);
              writeIntegerToBluetooth(data.targetCount);
              break;
          case HEADER_REQUEST_DATA_COUNTS_3: 
              SerialBT.write((byte)HEADER_REPLY_DATA_COUNTS_3);
              writeIntegerToBluetooth(data.dayCount);
              break;
          case HEADER_REQUEST_DATA_COUNTS_4: 
              SerialBT.write((byte)HEADER_REPLY_DATA_COUNTS_4);
              writeIntegerToBluetooth(data.totalCount);
              break;
          case HEADER_REQUEST_DATA_CONFIG_1:
              SerialBT.write((byte)HEADER_REPLY_DATA_CONFIG_1);
              writeIntegerToBluetooth(data.autoTasbeehTickTime);
              break;
          case HEADER_REQUEST_DATA_CONFIG_2: 
              SerialBT.write((byte)HEADER_REPLY_DATA_CONFIG_2);
              writeIntegerToBluetooth(data.minimumTasbeehTickTime);
              break;
          case HEADER_REQUEST_DATA_CONFIG_3: 
              SerialBT.write((byte)HEADER_REPLY_DATA_CONFIG_3);
              writeIntegerToBluetooth(data.step);
              SerialBT.write((byte)':');
              SerialBT.write((byte)(data.isBuzzerOn ? '1' : '0'));
              SerialBT.write((byte)':');
              SerialBT.write((byte)(data.isVibratorOn ? '1' : '0'));
              SerialBT.write((byte)':');
              SerialBT.write((byte)(data.isAutoPilotMode ? '1' : '0'));
              break;
          case HEADER_REQUEST_SAVE_COUNTS_1:
              saveCounts(1, buffer1, bytesRead);
              SerialBT.write((byte)HEADER_REPLY_SAVE_COUNTS_1);
              blinkLED(100, 1);
              break;
          case HEADER_REQUEST_SAVE_COUNTS_2:
              saveCounts(2, buffer1, bytesRead);
              SerialBT.write((byte)HEADER_REPLY_SAVE_COUNTS_2);
              blinkLED(100, 1);
              break;
          case HEADER_REQUEST_SAVE_COUNTS_3:
              saveCounts(3, buffer1, bytesRead);
              SerialBT.write((byte)HEADER_REPLY_SAVE_COUNTS_3);
              blinkLED(100, 1);
              break;
          case HEADER_REQUEST_SAVE_COUNTS_4:
              saveCounts(4, buffer1, bytesRead);
              SerialBT.write((byte)HEADER_REPLY_SAVE_COUNTS_4);
              blinkLED(100, 1);
              break;
          case HEADER_REQUEST_SAVE_CONFIG_1:
              saveConfig(1, buffer1, bytesRead);
              SerialBT.write((byte)HEADER_REPLY_SAVE_CONFIG_1);
//              blinkLED(100, 1);
              break;
          case HEADER_REQUEST_SAVE_CONFIG_2:
              saveConfig(2, buffer1, bytesRead);
              SerialBT.write((byte)HEADER_REPLY_SAVE_CONFIG_2);
//              blinkLED(100, 1);
              break;
          case HEADER_REQUEST_SAVE_CONFIG_3:
              saveConfig(3, buffer1, bytesRead);
              SerialBT.write((byte)HEADER_REPLY_SAVE_CONFIG_3);
              blinkLED(100, 1);
              break;
        }
      }

      // Turn off Bluetooth if there was no activity for 1 Minute
      if(currentTime - lastTimeRadioActivity > RADIO_TURNOFF_TIMEOUT) {
        SerialBT.end();
        isBluetoothMode = false;
        blinkLED(100, 3);
      }
    }
  }
}

void writeIntegerToBluetooth(int number) {
  char buffer2[6];
  ((String)number).toCharArray(buffer2, sizeof(buffer2));
  int i=0;
  while(buffer2[i] != '\0') {
    SerialBT.write((byte)buffer2[i]);
    i++;
  }
}

void saveCounts(short whichCount, byte* buffer1, int length1) {
  
 String str = "";
 for(int i=1; i<length1; i++) {
  str = str + (char)buffer1[i];
 }

 switch(whichCount) {
  case 1:
        data.count = str.toInt();
        break;
  case 2:
        data.targetCount = str.toInt();
        break;
  case 3:
        data.dayCount = str.toInt();
        break;
  case 4:
        data.totalCount = str.toInt();
        break;
 }
 storeEEPROMData();
}

void saveConfig(short whichConfig, byte* buffer1, int length1) {

 int previousIndex;
 int index;
 String str = "";
 for(int i=1; i<length1; i++) {
  str = str + (char)buffer1[i];
 }

 switch(whichConfig) {
  case 1:
        data.autoTasbeehTickTime = str.toInt();
        break;
  case 2:
        data.minimumTasbeehTickTime = str.toInt();
        break;
  case 3:
        index = str.indexOf(':', 0);
        data.step = str.substring(0, index).toInt();

        previousIndex = index;
        index = str.indexOf(':', index+1);  
        data.isBuzzerOn = str.substring(previousIndex+1, index).toInt() == 1;

        previousIndex = index;
        index = str.indexOf(':', index+1);  
        data.isVibratorOn = str.substring(previousIndex+1, index).toInt() == 1;

        data.isAutoPilotMode = str.substring(index+1, str.length()).toInt() == 1;
        break;
 }
 storeEEPROMData();
}

void incrementTasbeeh() {
  if(data.count + data.step > data.targetCount) {
    isAutoPilotOn = false;
    blinkLED(1000, 1);
    return;
  }
  
  data.count =  data.count + data.step;
  storeEEPROMData();
  if(data.count == data.targetCount) {
    isAutoPilotOn = false;
    tasbeehCompleteSound();
  } else if(data.count % 100 == 0){
    blinkLED(10, 2);
  } else {
    blinkLED(10, 1);
  }
}

void blinkLED(int millisecs, int count) {

  int vibratorDelay = millisecs < 100 ? 100 : millisecs;

  long startTime = millis();
  long buzzerStartTime = millis();
  long vibratorStartTime = millis();
  bool isBuzzerOn1 = false;
  bool isVibratorOn1 = false;
  if(data.isBuzzerOn) {
    analogWrite(PIN_BUZZER, BUZZER_FREQUENCY);
    isBuzzerOn1 = true;
  }
  digitalWrite(PIN_LED, HIGH);
  isVibratorOn1 = true;
  if(data.isVibratorOn) {
    digitalWrite(PIN_VIBRATOR, LOW);
  }
  
  while(millis() - startTime <= (vibratorDelay * count * 2 - vibratorDelay)) {

      long currentTime = millis();
      if(data.isBuzzerOn) {
        if((currentTime - startTime <= (millisecs * count * 2 - millisecs)) && (currentTime - buzzerStartTime > millisecs)) {
          buzzerStartTime = currentTime;
          isBuzzerOn1 = !isBuzzerOn1;
          if(isBuzzerOn1) {
            analogWrite(PIN_BUZZER, BUZZER_FREQUENCY);
          } else {
            analogWrite(PIN_BUZZER, 255);
          }
        }
      }

      if(currentTime - startTime > (millisecs * count * 2 - millisecs)) {
        analogWrite(PIN_BUZZER, 255);
      }

      if(currentTime - vibratorStartTime > vibratorDelay) {
        vibratorStartTime = currentTime;
        isVibratorOn1 = !isVibratorOn1;
        if(isVibratorOn1) {
          digitalWrite(PIN_LED, HIGH);
          if(data.isVibratorOn) {
            digitalWrite(PIN_VIBRATOR, LOW);
          }
        } else {
          digitalWrite(PIN_LED, LOW);
          if(data.isVibratorOn) {
            digitalWrite(PIN_VIBRATOR, HIGH);
          }
        }
      }
  }

  digitalWrite(PIN_LED, LOW);
  analogWrite(PIN_BUZZER, 255);
  digitalWrite(PIN_VIBRATOR, HIGH);
}

void tasbeehCompleteSound() {
  blinkLED(10, 3);
  blinkLED(500, 1);
}

// Method to Store Structure data into EEPROM
void restoreEEPROMData() {
  EEPROM.get(0, data);
}

// Method to Restore Structure data from EEPROM
void storeEEPROMData() {
  EEPROM.put(0, data);
  EEPROM.commit();
}
