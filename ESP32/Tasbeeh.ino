#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#ifndef STASSID
#define STASSID "SmartTasbeeh"
#define STAPSK "a1b2c3d4e5"
#endif

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WebServer server(80);

int LED_BRIGHTNESS_OFF   = 255;
int LED_BRIGHTNESS_A     = 213;
int LED_BRIGHTNESS_B     = 170;
int LED_BRIGHTNESS_C     = 128;
int LED_BRIGHTNESS_D     = 85;
int LED_BRIGHTNESS_E     = 43;
int LED_BRIGHTNESS_F     =  0;
int LED_BRIGHTNESS_BLINK = -1;

int LED_BRIGHTNESS[8] = {LED_BRIGHTNESS_OFF, LED_BRIGHTNESS_A, LED_BRIGHTNESS_B, LED_BRIGHTNESS_C, LED_BRIGHTNESS_D, LED_BRIGHTNESS_E, LED_BRIGHTNESS_F, LED_BRIGHTNESS_BLINK};

#define DEFAULT_MARKER 7867
#define DEFAULT_DURATION 700
#define DEFAULT_AUTO_PILOT false
#define DEFAULT_BUZZER true
#define DEFAULT_VIBRATOR true
#define DEFAULT_COUNT 0
#define DEFAULT_TARGET 101
#define COUNT_RESET_DURATION 2000
#define AP_TRIGGER_DURATION 4000
#define AP_TIMEOUT 120000 // 2 minutes
#define LED_BLINK_FREQUENCY 500  // Milliseconds
long lastTime = -1;
long wifiOnTime = -1;
bool isWiFiOn = false;
bool isAutoPilotOn = false;
int currentBrightnessIndex = 0;
int currentLEDBrightness = LED_BRIGHTNESS[currentBrightnessIndex];
long lastTimeLEDBlink = -1;
bool isLEDBlink = false;
char buffer[5];

int PIN_BUTTON = 3;
int PIN_BUTTON_LED = 13;
int PIN_SDA = 4;
int PIN_SCL = 5;
int PIN_BUZZER = 1;
int PIN_VIBRATOR = 2;
int PIN_LED = 12;

// EEPROM Operations
struct Data {
  int marker;
  long duration; // Millisecond
  bool isAutoPilot;
  bool isBuzzer;
  bool isVibrator;
  long count;
  long target;
};

Data data;

void handleRoot() {
  wifiOnTime = millis();
  String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Smart Tasbeeh, by Mujeeb Mohammad</title>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <center>
        <font face="Arial">
          <h1>Smart Tasbeeh</h1>
          <h4>by Mujeeb Mohammad (+91 9880506766 / mohammad.mujeeb@gmail.com)</h4>
          <br/><br/>
          <form action="/submit" method="GET">
            Target Count : <input type="text" name="target" value="%target%"><br/><br/>
            Zikr Step Duration (In Milliseconds): <input type="text" name="duration" value="%duration%"><br/><br/><br/>
            Enable Auto Pilot : <input type="checkbox" id="autoPilot" name="autoPilot" value="True" %autoPilotValue%><br>
            Enable Buzzer : <input type="checkbox" id="buzzer" name="buzzer" value="True" %buzzerValue%><br>
            Enable Vibrator : <input type="checkbox" id="vibrator" name="vibrator" value="True" %vibratorValue%><br>
            <br/><br/>
            <input type="submit" value="Change">
          </form>
        </font>
      </center>
    </body>
    </html>
  )=====";
  html.replace("%target%", String(data.target));
  html.replace("%duration%", String(data.duration));
  html.replace("%autoPilotValue%", data.isAutoPilot ? String("checked") : String(""));
  html.replace("%buzzerValue%", data.isBuzzer ? String("checked") : String(""));
  html.replace("%vibratorValue%", data.isVibrator ? String("checked") : String(""));
  server.send(200, "text/html", html);
}

void handleSubmit() {
  wifiOnTime = millis();
  String receivedValue = server.arg("duration");
  long duration = atol(receivedValue.c_str());
  if(duration <= 0) {
    server.send(200, "text/plain", "Duration " + receivedValue + " is Invalid. Please enter a value greater than 0.");
    return;
  }
  data.duration = duration;

  receivedValue = server.arg("target");
  long target = atol(receivedValue.c_str());
  if(target <= 0 || target > 9999) {
    server.send(200, "text/plain", "Target value " + receivedValue + " is Invalid. Please enter a value greater than 0 and less than 9999.");
    return;
  }
  data.target = target;

  receivedValue = server.arg("autoPilot");
  if(receivedValue.equals(String("True"))) {
    data.isAutoPilot = true;
  } else {
    data.isAutoPilot = false;
  }

  receivedValue = server.arg("buzzer");
  if(receivedValue.equals(String("True"))) {
    data.isBuzzer = true;
  } else {
    data.isBuzzer = false;
  }
  receivedValue = server.arg("vibrator");
  if(receivedValue.equals(String("True"))) {
    data.isVibrator = true;
  } else {
    data.isVibrator = false;
  }
  saveData();
  updateDisplay();

  server.send(200, "text/plain", "Data was saved Successfully.");
}

void handleNotFound() {
  wifiOnTime = millis();
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) { message += " " + server.argName(i) + ": " + server.arg(i) + "\n"; }
  server.send(404, "text/plain", message);
}

void saveData() {
  EEPROM.put(0, data);
  EEPROM.commit();
}

void updateDisplay() {
  display.clearDisplay();
  
  display.setTextSize(5);      // Normal 1:1 pixel scale
  display.setCursor(7, 2);
  String str = String(data.count);
  str.toCharArray(buffer, 5);
  display.write(buffer);

  display.setTextSize(1);
  display.setCursor(3,44);
  str = String(data.target);
  str.toCharArray(buffer, 5);
  display.write(buffer);

  display.setCursor(3,56);
  str = String(data.duration);
  str.toCharArray(buffer, 5);
  display.write(buffer);

  display.setCursor(34,44);
  display.write(data.isAutoPilot ? "AUTO:ON" : "AUTO:OFF");

  display.setCursor(34,56);
  display.write(isWiFiOn ? "WiFi:ON" : "WiFi:OFF");

  display.setCursor(91,44);
  display.write(data.isBuzzer ? "Bz:ON" : "Bz:OFF");

  display.setCursor(91,56);
  display.write(data.isVibrator ? "Vb:ON" : "Vb:OFF");

  display.display();
}

void updateLEDBrightness() {

  if(currentLEDBrightness != LED_BRIGHTNESS_BLINK) {
    analogWrite(PIN_LED, currentLEDBrightness);
  }
}

void pulseOutputs(long millis) {
  if(data.isBuzzer) {
    digitalWrite(PIN_BUZZER, LOW);
  }
  if(data.isVibrator) {
    digitalWrite(PIN_VIBRATOR, HIGH);
  }
  
  delay(millis);

  if(data.isBuzzer) {
    digitalWrite(PIN_BUZZER, HIGH);
  }
  if(data.isVibrator) {
    digitalWrite(PIN_VIBRATOR, LOW);
  }
}

void pulseOutputs(long millis, int times) {

  for(int i=0; i<times; i++) {
    if(data.isBuzzer) {
      digitalWrite(PIN_BUZZER, LOW);
    }
    if(data.isVibrator) {
      digitalWrite(PIN_VIBRATOR, HIGH);
    }
    
    delay(millis);

    if(data.isBuzzer) {
      digitalWrite(PIN_BUZZER, HIGH);
    }
    if(data.isVibrator) {
      digitalWrite(PIN_VIBRATOR, LOW);
    }

    delay(millis);
  }
}

void turnOnAP(bool isOn) {
    if(isOn) {
      // AP Mode
      WiFi.mode(WIFI_MODE_AP);
      WiFi.softAP(STASSID, STAPSK);

      // No authentication by default
      // ArduinoOTA.setPassword("admin");

      // Port defaults to 8266
      // ArduinoOTA.setPort(8266);

      // Hostname defaults to esp8266-[ChipID]
      ArduinoOTA.setHostname("SmartTasbeeh");

      ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
          type = "sketch";
        } else {  // U_FS
          type = "filesystem";
        }
      });
      
      ArduinoOTA.begin();

      // Web Server
      server.on("/", handleRoot);
      server.on("/submit", handleSubmit);
      server.on("/inline", []() {
        server.send(200, "text/plain", "this works as well");
      });
      server.onNotFound(handleNotFound);
      server.begin();
      isWiFiOn = true;
      wifiOnTime = millis();
    } else {

      // MDNS.end();
      ArduinoOTA.end();
      server.stop();
      WiFi.mode(WIFI_OFF);
      isWiFiOn = false;
    }
    updateDisplay();
}

// the setup function runs once when you press reset or power the board
void setup() {
  EEPROM.begin(sizeof(data));

  EEPROM.get(0, data);

  if(data.marker != DEFAULT_MARKER) {
    data.marker = DEFAULT_MARKER;
    data.duration = DEFAULT_DURATION;
    data.isAutoPilot = DEFAULT_AUTO_PILOT;
    data.isBuzzer = DEFAULT_BUZZER;
    data.isVibrator = DEFAULT_VIBRATOR;
    data.count = DEFAULT_COUNT;
    data.target = DEFAULT_TARGET;
    saveData();
  }

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LED, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_VIBRATOR, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  updateDisplay();

  // Default States
  digitalWrite(PIN_BUZZER, HIGH);
  digitalWrite(PIN_VIBRATOR, LOW);
  digitalWrite(PIN_LED, HIGH);
  updateLEDBrightness();

  WiFi.mode(WIFI_OFF);
  lastTime = millis();
}

// the loop function runs over and over again forever
void loop() {
  if(isWiFiOn) {
    ArduinoOTA.handle();
    server.handleClient();
  }

  // Sense the Main Tasbeeh Button
  if(digitalRead(PIN_BUTTON) == LOW) {

    long pressTime = millis();

    // While is button is pressed
    while(digitalRead(PIN_BUTTON) == LOW) {
      // Do Nothing
    }

    // To avoid continuous Trigger
    delay(100);

    long pressDuration = millis() - pressTime;

    if(pressDuration > AP_TRIGGER_DURATION) {
      turnOnAP(true);
      pulseOutputs(100, 5);

    } else if(pressDuration > COUNT_RESET_DURATION) {
      data.count = 0;
      updateDisplay();
      pulseOutputs(50,6);
    } else if(data.isAutoPilot) {
        isAutoPilotOn = !isAutoPilotOn;

    } else {
      if(data.count < data.target) {
        data.count++;
        updateDisplay();
        saveData();
        if(data.count >= data.target) {
          pulseOutputs(1000);
          isAutoPilotOn = false;
        } else if(data.count % 100 == 0) {
          pulseOutputs(10,5);
        } else{
          pulseOutputs(100);
        }
      } else {
        pulseOutputs(1000);
        isAutoPilotOn = false;
      }
    }
  }

  long currentTime = millis();
  if(data.isAutoPilot && isAutoPilotOn && data.count < data.target && currentTime - lastTime > data.duration) {
    data.count++;
    updateDisplay();
    saveData();
    if(data.count >= data.target) {
      pulseOutputs(1000);
      isAutoPilotOn = false;
    } else {
      if(data.count % 100 == 0) {
        pulseOutputs(10,5);
      } else{
        pulseOutputs(100);
      }
    }
    lastTime = currentTime;
  }

  // Sense the LED Button
  if(digitalRead(PIN_BUTTON_LED) == LOW) {

    // While is button is pressed
    while(digitalRead(PIN_BUTTON) == LOW) {
      // Do Nothing
    }

    // To avoid continuous Trigger
    delay(200);

    currentBrightnessIndex++;
    if(currentBrightnessIndex >= (sizeof(LED_BRIGHTNESS) / sizeof(LED_BRIGHTNESS[0]))) {
      currentBrightnessIndex = 0;
    }
    currentLEDBrightness = LED_BRIGHTNESS[currentBrightnessIndex];
    updateLEDBrightness();
  }

  // If Blink mode is On, then Blink the LED
  if(currentLEDBrightness == LED_BRIGHTNESS_BLINK && currentTime - lastTimeLEDBlink > LED_BLINK_FREQUENCY) {
    lastTimeLEDBlink = currentTime;
    analogWrite(PIN_LED, isLEDBlink ? 0 : 255);
    isLEDBlink = !isLEDBlink;
  }

  if(isWiFiOn && currentTime - wifiOnTime > AP_TIMEOUT) {
    turnOnAP(false);
  }
}
