#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

#define ALLAH_NAMES_COUNT 99 

String Allah99Names[ALLAH_NAMES_COUNT] = {
  "Ar-Rahmaan",
  "Ar-Raheem",
  "Al-Malik",
  "Al-Quddoos",
  "As-Salaam",
  "Al-Mu'min",
  "Al-Muhaymin",
  "Al-'Azeez",
  "Al-Jabbaar",
  "Al-Mutakabbir",
  "Al-Khaaliq",
  "Al-Baari'",
  "Al-Musawwir",
  "Al-Ghaffaar",
  "Al-Qahhaar",
  "Al-Wahhaab",
  "Ar-Razzaaq",
  "Al-Fattaah",
  "Al-Aleem",
  "Al-Qabid",
  "Al-Basit",
  "Al-Khafid",
  "Ar-Rafi'",
  "Al-Mu'izz",
  "Al-Mu'zill",
  "As-Sami'",
  "Al-Basir",
  "Al-Hakam",
  "Al-Adl",
  "Al-Lateef",
  "Al-Khabeer",
  "Al-Haleem",
  "Al-Azeem",
  "Al-Ghafoor",
  "Ash-Shakoor",
  "Al-'Ali",
  "Al-Kabeer",
  "Al-Hafeez",
  "Al-Muqeet",
  "Al-Haseeb",
  "Al-Jaleel",
  "Al-Kareem",
  "Ar-Raqeeb",
  "Al-Mujeeb",
  "Al-Wasi'",
  "Al-Hakeem",
  "Al-Wadood",
  "Al-Majeed",
  "Al-Ba'ith",
  "As-Shaheed",
  "Al-Haqq",
  "Al-Wakeel",
  "Al-Qawiyy",
  "Al-Mateen",
  "Al-Waliyy",
  "Al-Hameed",
  "Al-Muhsi",
  "Al-Mubdi",
  "Al-Mu'id",
  "Al-Muh'yi",
  "Al-Mumeet",
  "Al-Hayyu",
  "Al-Qayyum",
  "Al-Wajid",
  "Al-Majid",
  "Al-Wahid",
  "Al-Ahad",
  "As-Samad",
  "Al-Qadir",
  "Al-Muqtadir",
  "Al-Muqaddim",
  "Al-Mu'akhkhar",
  "Al-Awwal",
  "Al-Akhir",
  "Az-Zahir",
  "Al-Batin",
  "Al-Wali",
  "Al-Muta'ali",
  "Al-Barr",
  "At-Tawwaab",
  "Al-Muntaqim",
  "Al-'Afuw",
  "Ar-Ra'uf",
  "Al Malik al-Mulk",
  "Al Dhul-Jalal wal-Ikram",
  "Al-Muqsit",
  "Al-Jami'",
  "Al-Ghani",
  "Al-Mughni",
  "Al-Mani'",
  "Ad-Dharr",
  "An-Nafi'",
  "An-Noor",
  "Al-Hadi",
  "Al-Badi'",
  "Al-Baqi",
  "Al-Waris",
  "Ar-Rasheed",
  "As-Saboor"
};

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

#define DEFAULT_MARKER 7867
#define DEFAULT_DURATION 700
#define DEFAULT_AUTO_PILOT true
#define DEFAULT_BUZZER false
#define DEFAULT_VIBRATOR true
#define DEFAULT_COUNT 0
#define DEFAULT_NAME_INDEX 0
#define DEFAULT_TARGET 25
#define COUNT_RESET_DURATION 2000
#define AP_TRIGGER_DURATION 4000
#define AP_TIMEOUT 120000 // 2 minutes
int durationMalikulMulk = DEFAULT_DURATION;
int durationJalalIkram = DEFAULT_DURATION;
long lastTime = -1;
long wifiOnTime = -1;
bool isWiFiOn = false;
bool isAutoPilotOn = false;
char buffer[25];
long pressTime = 0;
long pressDuration = 0;

int PIN_BUTTON = 32;
int PIN_SDA = 19;
int PIN_SCL = 21;
int PIN_BUZZER = 23;
int PIN_VIBRATOR = 22;

// EEPROM Operations
struct Data {
  int marker;
  int duration; // Millisecond
  bool isAutoPilot;
  bool isBuzzer;
  bool isVibrator;
  int count;
  int target;
  int currentNameIndex;
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
  // Calculate other durations
  durationMalikulMulk = data.duration * 2.142857142857143;
  durationJalalIkram = data.duration * 2.857142857142857;

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
  
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setCursor(0, 0);
  String str = String(100);
  Allah99Names[data.currentNameIndex].toCharArray(buffer, sizeof(buffer));
  display.write(buffer);

  display.setTextSize(1);
  display.setCursor(3,56);
  str = String(data.currentNameIndex+1);
  str.toCharArray(buffer, sizeof(buffer));
  display.write(buffer);

  display.setCursor(58,56);
  str = String(data.target);
  str.toCharArray(buffer, sizeof(buffer));
  display.write(buffer);

  display.setCursor(110,56);
  str = String(data.count);
  str.toCharArray(buffer, sizeof(buffer));
  display.write(buffer);

  display.display();
}

void pulseOutputs(long millis) {
  if(data.isBuzzer) {
    digitalWrite(PIN_BUZZER, LOW);
  }
  if(data.isVibrator) {
    digitalWrite(PIN_VIBRATOR, LOW);
  }
  
  delay(millis);

  if(data.isBuzzer) {
    digitalWrite(PIN_BUZZER, HIGH);
  }
  if(data.isVibrator) {
    digitalWrite(PIN_VIBRATOR, HIGH);
  }
}

void pulseOutputs(long millis, int times) {

  for(int i=0; i<times; i++) {
    if(data.isBuzzer) {
      digitalWrite(PIN_BUZZER, LOW);
    }
    if(data.isVibrator) {
      digitalWrite(PIN_VIBRATOR, LOW);
    }
    
    delay(millis);

    if(data.isBuzzer) {
      digitalWrite(PIN_BUZZER, HIGH);
    }
    if(data.isVibrator) {
      digitalWrite(PIN_VIBRATOR, HIGH);
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
    data.currentNameIndex = DEFAULT_NAME_INDEX;
    saveData();
  }

  // Calculate other durations
  durationMalikulMulk = data.duration * 2.142857142857143;
  durationJalalIkram = data.duration * 2.142857142857143;

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_VIBRATOR, OUTPUT);
  
  Wire.begin(PIN_SDA, PIN_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  updateDisplay();

  // Default States
  digitalWrite(PIN_BUZZER, HIGH);
  digitalWrite(PIN_VIBRATOR, HIGH);

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

    pressTime = millis();

    while(digitalRead(PIN_BUTTON) == LOW) {
      // Do Nothing
    }

    pressDuration = millis() - pressTime;

    if(pressDuration > AP_TRIGGER_DURATION) {
      turnOnAP(true);
      pulseOutputs(100, 5);

    } else if(pressDuration > COUNT_RESET_DURATION) {
      data.currentNameIndex = 0;
      data.count = 0;
      saveData();
      updateDisplay();
      pulseOutputs(50,6);
      isAutoPilotOn = false;

    } else if(data.isAutoPilot) {
        isAutoPilotOn = !isAutoPilotOn;
        lastTime = millis();

    } else {
      if(data.count < data.target) {
        data.count++;
        saveData();
        updateDisplay();
        if(data.count >= data.target) {
          data.currentNameIndex++;
          saveData();
          if(data.currentNameIndex >= ALLAH_NAMES_COUNT) {
            pulseOutputs(1000);
            isAutoPilotOn = false;
          } else {
            updateDisplay();
            data.count = 0;
            saveData();
            pulseOutputs(20,5);
            delay(1000);
          }
          
        } else{
          pulseOutputs(100);
        }
      } else {
        data.currentNameIndex++;
        saveData();
        if(data.currentNameIndex >= ALLAH_NAMES_COUNT) {
          pulseOutputs(1000);
          isAutoPilotOn = false;
        } else {
          updateDisplay();
          data.count = 0;
          saveData();
          pulseOutputs(20,5);
          delay(1000);
        }
      }
    }

    delay(200);
  }

  int duration = data.duration;
  if(data.currentNameIndex == 83) {
    duration = durationMalikulMulk;
  } else if(data.currentNameIndex == 84) {
    duration = durationJalalIkram;
  }

  long currentTime = millis();
  if(data.isAutoPilot && isAutoPilotOn && data.count < data.target && currentTime - lastTime > duration) {
    data.count++;
    saveData();
    updateDisplay();
    if(data.count >= data.target) {
      data.currentNameIndex++;
      saveData();
      if(data.currentNameIndex >= ALLAH_NAMES_COUNT) {
        pulseOutputs(1000);
        isAutoPilotOn = false;
      } else {
        updateDisplay();
        data.count = 0;
        saveData();
        pulseOutputs(20,5);
        delay(1000);
      }
    } else {
      pulseOutputs(100);
    }
    lastTime = currentTime;
  }

  if(isWiFiOn && currentTime - wifiOnTime > AP_TIMEOUT) {
    turnOnAP(false);
  }
}
