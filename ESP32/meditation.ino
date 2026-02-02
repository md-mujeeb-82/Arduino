#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

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
long lastTime = -1;
bool isPolirityForward = true;
long halfCycleDelay = ((1/DEFAULT_FREQUENCY) * 1000 * 1000)/2;
bool isPowerAC = true;
bool blink = true;
long lastTimeBlink;

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
  display.setCursor(36, 0);     // Start at top-left corner
  display.write("MEDITATION");

  display.setCursor(0, 10);
  if(isPowerAC) {
    display.write("TYPE: AC");
  } else {
    display.write("TYPE: DC");
  }

  display.setCursor(0, 22);     // Start at top-left corner
  char buffer[14];

  if(isPowerAC) {
    String str = "FREQ: " + String(data.frequency, 0) + " Hz";
    str.toCharArray(buffer, 14);
  } else {
    String str = "VOLTAGE: 9 V";
    str.toCharArray(buffer, 14);
  }
  display.write(buffer);

  display.display();
}

void animatePower() {
  display.setCursor(55, 10);
  display.setTextColor(SSD1306_BLACK);
 
  if(isPowerAC) {
    if(blink) {
      display.write("\\/\\/");
      display.setCursor(55, 10);
      display.setTextColor(SSD1306_WHITE);
      display.write("/\\/\\");
      blink = false;
    } else {
      display.write("/\\/\\");
      display.setCursor(55, 10);
      display.setTextColor(SSD1306_WHITE);
      display.write("\\/\\/");
      blink = true;
    }
  } else {
    if(blink) {
      display.write("- -");
      display.setCursor(55, 10);
      display.setTextColor(SSD1306_WHITE);
      display.write(" - -");
      blink = false;
    } else {
      display.write(" - -");
      display.setCursor(55, 10);
      display.setTextColor(SSD1306_WHITE);
      display.write("- -");
      blink = false;
      blink = true;
    }
  }

  display.display();
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
  
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  updateDisplay();

  // AP Mode
  WiFi.softAP(STASSID, STAPSK);

  // Station Mode
  // WiFi.mode(WIFI_STA);
  // WiFi.begin("Ahmads", "03312018");
  // while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  //   Serial.println("Connection Failed! Rebooting...");
  //   delay(5000);
  //   ESP.restart();
  // }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Meditation");
  MDNS.begin("Meditation");

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

  // server.on("/gif", []() {
  //   static const uint8_t gif[] PROGMEM = {
  //     0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
  //     0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
  //     0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
  //     0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
  //     0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
  //   };
  //   char gif_colored[sizeof(gif)];
  //   memcpy_P(gif_colored, gif, sizeof(gif));
  //   // Set the background to a random set of colors
  //   gif_colored[16] = millis() % 256;
  //   gif_colored[17] = millis() % 256;
  //   gif_colored[18] = millis() % 256;
  //   server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
  // });

  server.onNotFound(handleNotFound);

  MDNS.addService("http", "udp", 5353);
  server.begin();

  // Initial setup
  digitalWrite(PIN_1, HIGH);
  digitalWrite(PIN_2, LOW);

  lastTime = lastTimeBlink = micros();
}

// the loop function runs over and over again forever
void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  // MDNS.update();

  long currentTime = micros();

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
