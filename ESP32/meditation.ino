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
#define STASSID "Meditation"
#define STAPSK "a1b2c3d4e5"
#endif

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WebServer server(80);

#define DEFAULT_MARKER 7867
#define DEFAULT_FREQUENCY 13.0
#define BLINK_DELAY 100000
#define AP_TIMEOUT 120000000  // Microseconds
long lastTime = -1;
bool isPolirityForward = true;
long halfCycleDelay = ((1/DEFAULT_FREQUENCY) * 1000 * 1000)/2;
bool isPowerAC = true;
bool blink = true;
long lastTimeBlink;
long wifiOnTime = -1;
bool isWiFiOn = false;

int PIN_SDA = 21;
int PIN_SCL = 22;
int PIN_1 = 16;
int PIN_2 = 17;

// EEPROM Operations
struct Data {
  int marker;
  float frequency;
};

Data data;

void handleRoot() {
  wifiOnTime = micros();
  String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Meditation, by Mujeeb Mohammad</title>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <center>
        <h1>Meditation</h1>
        <h4>by Mujeeb Mohammad (+91 9880506766 / mohammad.mujeeb@gmail.com)</h4>
        <form action="/submit" method="GET">
          Frequency: <input type="text" name="frequency" value="%value%">
          <input type="submit" value="Change">
        </form>
      </center>
    </body>
    </html>
  )=====";
  html.replace("%value%", String(data.frequency));
  server.send(200, "text/html", html);
}

void handleSubmit() {
  wifiOnTime = micros();
  String receivedValue = server.arg("frequency");
  float freq = receivedValue.toFloat();
  if(freq < 0 || freq > 5000) {
    server.send(200, "text/plain", "Frequency " + receivedValue + " is Invalid. Please enter a value from 0 to 5000.");
    return;
  }

  data.frequency = freq;
  saveData();
  
  updateDisplay();

  if(data.frequency == 0) {
    digitalWrite(PIN_1, HIGH);
    digitalWrite(PIN_2, LOW);
  }
  server.send(200, "text/plain", "Frequency " + receivedValue + " saved Successfully.");
}

void handleNotFound() {
  wifiOnTime = micros();
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
  if(data.frequency != 0) {
    halfCycleDelay = ((1/data.frequency) * 1000 * 1000)/2;
    isPowerAC = true;
  } else {
    isPowerAC = false;
  }
  EEPROM.put(0, data);
  EEPROM.commit();
}

void updateDisplay() {
  display.clearDisplay();

  // Not all the characters will fit on the display. This is normal.
  // Library will draw what it can and the rest will be clipped.
  display.setTextSize(2);       // Normal 1:1 pixel scale
  display.setCursor(5, 5);     // Start at top-left corner
  display.write("MEDITATION");

  display.setTextSize(1);       // Normal 1:1 pixel scale
  if(isPowerAC) {
    display.setCursor(0, 30);
    display.write("CURRENT TYPE: AC");
    display.setCursor(0, 42);
    display.write("VOLTAGE: 10.88 V");
  } else {
    display.setCursor(0, 30);
    display.write("CURRENT TYPE: DC");
    display.setCursor(0, 42);
    display.write("VOLTAGE: 10.93 V");
  }

  display.setCursor(0, 54);     // Start at top-left corner
  char buffer[128];
  String str = "FREQUENCY: " + String(data.frequency, 0) + " Hz";
  str.toCharArray(buffer, 128);
  display.write(buffer);

  display.display();
}

void animatePower() {
  display.setCursor(100, 42);
  display.setTextColor(SSD1306_BLACK);
 
  if(isPowerAC) {
    if(blink) {
      display.write("\\/\\/");
      display.setCursor(100, 42);
      display.setTextColor(SSD1306_WHITE);
      display.write("/\\/\\");
      blink = false;
    } else {
      display.write("/\\/\\");
      display.setCursor(100, 42);
      display.setTextColor(SSD1306_WHITE);
      display.write("\\/\\/");
      blink = true;
    }
  } else {
    if(blink) {
      display.write("- -");
      display.setCursor(100, 42);
      display.setTextColor(SSD1306_WHITE);
      display.write(" - -");
      blink = false;
    } else {
      display.write(" - -");
      display.setCursor(100, 42);
      display.setTextColor(SSD1306_WHITE);
      display.write("- -");
      blink = false;
      blink = true;
    }
  }

  display.display();
}

void turnOnAP(bool isOn) {
    if(isOn) {
      // AP Mode
      WiFi.softAP(STASSID, STAPSK);

      // Port defaults to 8266
      // ArduinoOTA.setPort(8266);

      // Hostname defaults to esp8266-[ChipID]
      ArduinoOTA.setHostname("Meditation");

      // No authentication by default
      // ArduinoOTA.setPassword("admin");

      // Password can be set with it's md5 value as well
      // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
      // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

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
}

// the setup function runs once when you press reset or power the board
void setup() {

  EEPROM.begin(sizeof(data));

  EEPROM.get(0, data);

  if(data.marker != DEFAULT_MARKER) {
    data.marker = DEFAULT_MARKER;
    data.frequency = DEFAULT_FREQUENCY;
    saveData();
  }

  if(data.frequency != 0) {
    halfCycleDelay = ((1/data.frequency) * 1000 * 1000)/2;
    isPowerAC = true;
  } else {
    isPowerAC = false;
  }

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(PIN_1, OUTPUT);
  pinMode(PIN_2, OUTPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  updateDisplay();

  // Initial setup
  digitalWrite(PIN_1, HIGH);
  digitalWrite(PIN_2, LOW);

  turnOnAP(true);

  lastTime = lastTimeBlink = micros();
}

// the loop function runs over and over again forever
void loop() {

  if(isWiFiOn) {
    ArduinoOTA.handle();
    server.handleClient();
  }

  long currentTime = micros();

  if(isWiFiOn && currentTime - wifiOnTime > AP_TIMEOUT) {
    turnOnAP(false);
  }

  if(currentTime - lastTimeBlink > BLINK_DELAY) {
    animatePower();
    lastTimeBlink = currentTime;
  }

  if(!isPowerAC) {
    return;
  }

  if(currentTime - lastTime >= halfCycleDelay) {

    if(isPolirityForward) {
      
      digitalWrite(PIN_1, HIGH);
      digitalWrite(PIN_2, LOW);
      isPolirityForward = false;
    } else {

      digitalWrite(PIN_1, LOW);
      digitalWrite(PIN_2, HIGH);
      isPolirityForward = true;
    }

    lastTime = currentTime;
  }
}
