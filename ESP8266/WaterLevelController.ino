#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <PCF8574.h>

#define SERIAL_NUMBER "6858929474"
#define IDENTIFIER    7867

PCF8574 PCF(0x20);

#define PIN_AUTO                  0
#define PIN_WIFI_CONNECTED        2
#define PIN_WIFI_DISCONNECTED    12
#define PIN_PUMP                 14
#define PIN_SWITCH_RESET         16

#define PIN_EX_TANK_SENSOR1      5
#define PIN_EX_TANK_SENSOR2      4
#define PIN_EX_TANK_SENSOR3      3
#define PIN_EX_SUMP_SENSOR1      2
#define PIN_EX_SUMP_SENSOR2      1
#define PIN_EX_SUMP_SENSOR3      0
#define PIN_EX_PUMP_RELAY        6

#define DATA_REFRESH_DELAY 3000 // 3 Seconds

String AP_SSID = String("ALITHIS-") + SERIAL_NUMBER;
char HOSTNAME[21] = "WATERLEVEL-";

const char BROKER[]       = "io.alithistech.com";
//const char BROKER[]      = "192.168.68.173";
int        PORT           = 1883;
String TOPIC_TANK         = "waterLevelController/" + String(SERIAL_NUMBER) + "/tank";
String TOPIC_SUMP         = "waterLevelController/" + String(SERIAL_NUMBER) + "/sump";
String TOPIC_AUTO_STATUS  = "waterLevelController/" + String(SERIAL_NUMBER) + "/auto/status";
String TOPIC_AUTO_CONTROL = "waterLevelController/" + String(SERIAL_NUMBER) + "/auto/control";
String TOPIC_PUMP_STATUS  = "waterLevelController/" + String(SERIAL_NUMBER) + "/pump/status";
String TOPIC_PUMP_CONTROL = "waterLevelController/" + String(SERIAL_NUMBER) + "/pump/control";

static const String FOOTER = String("<br/><br/><br/><br/><br/><br/><br/>\n")
                           + String("<font face=\"Arial\" size=\"2\">\n")
                           + String("<table width=\"100%\" bgcolor=\"lightgray\" style=\"color: black;\">\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("  <tr>\n")
                           + String("    <td colspan=\"3\" align=\"center\">&#169; Copyright 2020 Alithis Technologies - All Rights Reserved</td>\n")
                           + String("  </tr>\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("  <tr>\n")
                           + String("    <td align=\"left\">&nbsp;<a href=\"https://alithistech.com\">Alithis Technologies</a></td>\n")
                           + String("    <td align=\"center\"><a href=\"mailto:alithistechnologies@gmail.com\">alithistechnologies@gmail.com</a></td>\n")
                           + String("    <td align=\"right\"><a href=\"tel:+919880506766\">+91-9880506766</a>&nbsp;</td>\n")
                           + String("  <tr>\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("</table>\n")
                           + String("</font>\n");

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
//PubSubClient mqtt(wifi);
PubSubClient mqtt(BROKER, 1883, wifi);

#define ARRAY_SIZE 33

// The Data Structure
struct Data {
  int identifier;
  bool isAutoOn = true;
  bool isModeAccessPoint = true;
  char ssid[ARRAY_SIZE] = "Ahmads";
  char password[ARRAY_SIZE] = "Xg#609590";
} data;

const int MQTT_CONNECT_TRY_INTERVAL    = 5000;                  // 5 Seconds
const int SWITCH_AUTO_RESET_INTERVAL   = 5000;                  // 5 Seconds
const int WIFI_CONNECTION_TIMEOUT      = 60000;                 // 1 Minute
const int FACTORY_RESET_BEGIN_TIMEOUT  = 3000;                  // 3 Seconds
const int FACTORY_RESET_TIMEOUT        = 10000;                 // 10 Seconds
const int SETUP_MODE_BLINK_DURATION    = 500;                   // 0.5 Seconds
long lastTimeMqttConnect = -1;
long lastTimeWifiConnect = -1;
long lastTimeFactoryReset = -1;
bool isESPNeedsToBeRestarted = false;

long lastTimeSensorRead = -1;
bool setUpModeBlink = false;
long lastTimeSetupMode = -1;

void setup() {
  // Construct Hostname
  for(int i=11; i<sizeof(HOSTNAME); i++) {
    HOSTNAME[i] = SERIAL_NUMBER[i-11];
  }
  
  EEPROM.begin(sizeof(Data));

  // Restore stored data from EEPROM
  restoreEEPROMData();
  
  // Invalid data, store the data for the first time
  if(data.identifier != IDENTIFIER) {
    data.identifier = IDENTIFIER;
    data.isAutoOn = true;
    data.isModeAccessPoint = true;
    for(int i=0; i<ARRAY_SIZE; i++) {
      data.ssid[i] = '\0';
      data.password[i] = '\0';
    }
    storeEEPROMData();
  }

  Serial.begin(9600);

  // GPIO Pin Configuration
  pinMode(PIN_AUTO, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_WIFI_CONNECTED, OUTPUT);
  pinMode(PIN_WIFI_DISCONNECTED, OUTPUT);
  pinMode(PIN_SWITCH_RESET, INPUT);

  // Default States
  digitalWrite(PIN_PUMP, LOW);

  if(data.isAutoOn) {
    digitalWrite(PIN_AUTO, HIGH);
  } else {
    digitalWrite(PIN_AUTO, LOW);
  }

  // Begin IO Extender
  PCF.begin(4, 5, 0);

  digitalWrite(PIN_PUMP, LOW);
  PCF.write(PIN_EX_PUMP_RELAY, LOW);
 
  setWiFiConnected(false);

  if (data.isModeAccessPoint) {
    WiFi.mode(WIFI_AP);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.softAP(AP_SSID);
    delay(500);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.begin(data.ssid, data.password);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
    lastTimeWifiConnect = millis();
    while (WiFi.status() != WL_CONNECTED) {
      long currentTime = millis();
      // Unable to connect to WiFi, reset to Access Point Mode
      if (currentTime - lastTimeWifiConnect > WIFI_CONNECTION_TIMEOUT) {
        data.isModeAccessPoint = true;
        storeEEPROMData();
        ESP.restart();
      }
      delay(500);
    }
    setWiFiConnected(true);
  }

  // Begin MDNS Service
  MDNS.begin(HOSTNAME);

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(HOSTNAME);

  ArduinoOTA.onStart([]() {
//    digitalWrite(0, LOW);
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });

  ArduinoOTA.begin();

  //Associate the handler functions to the path
  server.on("/", handleRoot);
  server.on("/configure", handleConfigure);
  server.on("/setupMode", handleSetupMode);
  server.on("/restartDevice", restartDevice);
  server.on("/factoryResetDevice", factoryResetDevice);
  server.onNotFound(handleNotFound);
  server.begin();

  if(!data.isModeAccessPoint) {
    mqtt.setServer(BROKER, PORT);
    mqtt.setCallback(callback);
    reconnectMQTT();
    lastTimeMqttConnect = millis();
  }
}

void loop() {

  if(isESPNeedsToBeRestarted) {
    delay(300);
    ESP.restart();
  }

  // Handle the OTA Functionality
  ArduinoOTA.handle();

  // Handling of incoming requests
  server.handleClient();

  // For MQTT Polling
  if(mqtt.connected()) {
    mqtt.loop();
  }

  long currentTime = millis();

  // Blink the connection LED red/green in Setup mode
  if(data.isModeAccessPoint) {
    if(currentTime - lastTimeSetupMode > SETUP_MODE_BLINK_DURATION || lastTimeSetupMode == -1) {
      setWiFiConnected(setUpModeBlink);
      setUpModeBlink = !setUpModeBlink;
      lastTimeSetupMode = currentTime;
    }
  } else {
    setWiFiConnected(WiFi.status() == WL_CONNECTED);

    // Try reconnecting to MQTT Broker only periodically
    if(!mqtt.connected() && currentTime - lastTimeMqttConnect > MQTT_CONNECT_TRY_INTERVAL) {
  
      reconnectMQTT();
      lastTimeMqttConnect = currentTime;
    }
  }
  
  if (currentTime - lastTimeSensorRead > DATA_REFRESH_DELAY || lastTimeSensorRead == -1) {

      int tankLevel = getTankLevel();
      int sumpLevel = getSumpLevel();
    
      // Pump On Logic
      if(data.isAutoOn && tankLevel < 20 && sumpLevel > 0) {
        // Turn ON Pump
        if(!isPumpOn()) {
          changePumpState(true);
        }
      }
    
      // Pump Off Logic
      if(tankLevel >= 100 || sumpLevel <= 0) {
        // Turn OFF Pump
        if(isPumpOn()) {
          changePumpState(false);
        }
      }

      if(!data.isModeAccessPoint) {
        mqtt.publish(TOPIC_TANK.c_str(), String(tankLevel).c_str());
        mqtt.publish(TOPIC_SUMP.c_str(), String(sumpLevel).c_str());
        mqtt.publish(TOPIC_AUTO_STATUS.c_str(), data.isAutoOn ? "1" : "0");
        mqtt.publish(TOPIC_PUMP_STATUS.c_str(), isPumpOn() ? "1" : "0");
      }

      lastTimeSensorRead = millis();
  }

  if(data.isModeAccessPoint) {
    return;
  }

  // Factory Reset Logic
  if(readSwitchValue()) {

    lastTimeFactoryReset = currentTime;
    while(readSwitchValue()) {

      currentTime = millis();
      if (currentTime - lastTimeFactoryReset > FACTORY_RESET_BEGIN_TIMEOUT) {
        // Start blinking LED for Factory reset countdown
        setFactoryResetLED(true);
        boolean wasSwitchReleased = false;
        int ledBlinkDelay = 500;
        bool blinkOnCycle = true;
        while(true) {
          delay(ledBlinkDelay);
          setFactoryResetLED(false);
          delay(ledBlinkDelay);
          setFactoryResetLED(true);
          ledBlinkDelay = ledBlinkDelay - 70;
          if(ledBlinkDelay < 20) {
            ledBlinkDelay = 20;
          }
          if(!readSwitchValue()) {
            wasSwitchReleased = true;
            break;
          }
          currentTime = millis();
          if (currentTime - lastTimeFactoryReset > FACTORY_RESET_TIMEOUT) {
            break;
          }
        }

        // If switch was not released till end of 3 Seconds, do Factory reset
        if(!wasSwitchReleased) {
          doFactoryResetAndRestart();
          ESP.restart();
        }
      }

      delay(200);
    }
  }
}

bool readSwitchValue() {
  pinMode(PIN_SWITCH_RESET, OUTPUT);
  digitalWrite(PIN_SWITCH_RESET, HIGH);
  pinMode(PIN_SWITCH_RESET, INPUT);
  return digitalRead(PIN_SWITCH_RESET) == LOW;
}

bool isPumpOn() {
  return digitalRead(PIN_PUMP) == HIGH;
}

void changeAutoState(bool state) {

    data.isAutoOn = state;
    storeEEPROMData();
    digitalWrite(PIN_AUTO, state ? HIGH : LOW);
    if(!data.isModeAccessPoint) {
      mqtt.publish(TOPIC_AUTO_STATUS.c_str(), state ? "1" : "0");
    }
}

void changePumpState(bool state) {
  digitalWrite(PIN_PUMP, state ? HIGH : LOW);
  PCF.write(PIN_EX_PUMP_RELAY, state ? HIGH : LOW);
  if(!data.isModeAccessPoint) {
    mqtt.publish(TOPIC_PUMP_STATUS.c_str(), state ? "1" : "0");
  }
}

void setWiFiConnected(bool status) {
//  PCF.write(PIN_WIFI_CONNECTED, status ? LOW : HIGH);
//  PCF.write(PIN_WIFI_DISCONNECTED, status ? HIGH : LOW);
  digitalWrite(PIN_WIFI_CONNECTED, status ? LOW : HIGH);
  digitalWrite(PIN_WIFI_DISCONNECTED, status ? HIGH : LOW);
}

void setFactoryResetLED(bool status) {
//  PCF.write(PIN_WIFI_CONNECTED, HIGH);
//  PCF.write(PIN_WIFI_DISCONNECTED, status ? LOW : HIGH);
  digitalWrite(PIN_WIFI_CONNECTED, HIGH);
  digitalWrite(PIN_WIFI_DISCONNECTED, status ? LOW : HIGH);
}

void reconnectMQTT() {
  // Reconnect and publish
  if (mqtt.connect(HOSTNAME, "mqtt", "Mummy@123")) {
    mqtt.subscribe(TOPIC_AUTO_CONTROL.c_str());
    mqtt.subscribe(TOPIC_PUMP_CONTROL.c_str());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  String strTopic = String(topic);
  
  if(strTopic == String(TOPIC_AUTO_CONTROL)) {
     changeAutoState((char)payload[0] == '1');
  }

  if(strTopic == String(TOPIC_PUMP_CONTROL)) {
    changePumpState((char)payload[0] == '1');
  }
}

int getTankLevel() {
//  return 50;
  
  if(PCF.readButton(PIN_EX_TANK_SENSOR3) == LOW) {
    return 100;
  } else if(PCF.readButton(PIN_EX_TANK_SENSOR2) == LOW) {
    return 50;
  } else if(PCF.readButton(PIN_EX_TANK_SENSOR1) == LOW) {
    return 20;
  } else {
    return 0;
  }
}

int getSumpLevel() {
//  return 50;
  
  if(PCF.readButton(PIN_EX_SUMP_SENSOR3) == LOW) {
    return 100;
  } else if(PCF.readButton(PIN_EX_SUMP_SENSOR2) == LOW) {
    return 50;
  } else if(PCF.readButton(PIN_EX_SUMP_SENSOR1) == LOW) {
    return 20;
  } else {
    return 0;
  }
}

void handleRoot() {

  String device = server.arg("device");
  String state = server.arg("state");

  if(device != NULL && state != NULL && device.length() > 1 && state.length() > 1) {
    
    if(device == "pump") {
      changePumpState(state == "true");
      
    } else if(device == "auto") {
      changeAutoState(state == "true");
      
    }
  }

  String html = String("<html>\n")
              + String("  <head>\n")
              + String("    <title>Smart Water Level Controller</title>\n")
              + String("    <script language='JavaScript'>\n")
              + String("    function submitForm(device, state) {\n")
              + String("      document.forms[0].device.value = device;\n")
              + String("      document.forms[0].state.value = state;\n")
              + String("      document.forms[0].submit();\n")
              + String("    }\n")
              + String("    function deviceControls(value) {\n")
              + String("      var message;\n")
              + String("      if(value == 0) {\n")
              + String("        message = \"Are you sure that you want to Factory Reset your Device?\";\n")
              + String("      } else if(value == 1) {\n")
              + String("        message = \"Are you sure that you want to Restart your Device?\";\n")
              + String("      } else {\n")
              + String("        message = \"Are you sure that you want to Configure your Device?\";\n")
              + String("      }\n")
              + String("      if (confirm(message) == true) {\n")
              + String("        if(value == 0) {\n")
              + String("         document.forms[1].action = \"/factoryResetDevice\";\n")
              + String("         document.forms[1].submit();\n")
              + String("        } else if(value == 1) {\n")
              + String("         document.forms[1].action = \"/restartDevice\";\n")
              + String("         document.forms[1].submit();\n")
              + String("        } else {\n")
              + String("         document.forms[1].action = \"/configure\";\n")
              + String("         document.forms[1].submit();\n")
              + String("        }\n")
              + String("      } else {\n")
              + String("        return false;\n")
              + String("      }\n")
              + String("     }\n")
              + String("    </script>\n")
              + String("  </head>\n")
              + String("  <body>\n")
              + String("    <center>\n")
              + String("      <br>\n")
              + String("      <h1>Smart Water Level Controller</h1>\n")
              + String("      <br>\n")
              + String("      <table border='1' cellpadding='20' cellspacing='0'>\n")
              + String("        <tr>\n")
              + String("          <td align='center'>Tank: <font color='blue'><b>" + String(getTankLevel()) + "%</b></font></td>\n")
              + String("          <td align='center'>Sump: <font color='blue'><b>" + String(getSumpLevel()) + "%</b></font></td>\n")
              + String("        </tr>\n")
              + String("        <tr>\n")
              + String("          <td align='center'>Pump: <font color=" + String(isPumpOn() ? "'green'>On" : "'red'>Off") + "</font></td>\n")
              + String("          <td align='center'>Auto Control: <font color=" + String(data.isAutoOn ? "'green'>On" : "'red'>Off") + "</font></td>\n")
              + String("        </tr>\n")
              + String("        <tr>\n")
              + String("          <form method='GET'>\n")
              + String("            <input type='hidden' name='device'><input type='hidden' name='state'>\n")
              + String("            <td align='center'><button id='pump1' name='pump' onclick='submitForm(\"pump\"," + String(isPumpOn() ? "false)'>Turn Off" : "true)'>Turn On") + "</button></td>\n")
              + String("            <td align='center'><button id='auto' name='auto' onclick='submitForm(\"auto\"," + String(data.isAutoOn ? "false)'>Turn Off" : "true)'>Turn On") + "</button></td>\n")
              + String("          </form>\n")
              + String("        </tr>\n")
              + String("      </table>\n")
              + String("      <br><br><br>\n")
              + String("      <table border='0' cellpadding='20' cellspacing='0'>\n")
              + String("        <tr>\n")
              + String("          <form method='GET'>\n")
              + String("            <td align='center'><button id='reset' name='reset' onclick='return deviceControls(0)'>Factory Reset</button></td>\n")
              + String("            <td align='center'><button id='restart' name='restart' onclick='return deviceControls(1)'>Restart</button></td>\n")
              + String("            <td align='center'><button id='configure' name='configure' onclick='return deviceControls(2)'>Configure</button></td>\n")
              + String("          </form>\n")
              + String("        </tr>\n")
              + String("      </table>\n")
              + String("    </center>\n")
              + FOOTER
              + String("  </body>\n")
              + String("</html>\n");
        
  server.send(200, "text/html", html);
  server.client().stop();
}

void handleConfigure() {
  String ptr = "<!DOCTYPE html>\n";
  ptr += "<html>\n";
  ptr += "  <head>\n";
  ptr += "    <title>Device Configuration</title>\n";
  ptr += "    <script language='JavaScript'>\n";
  ptr += "    function submitForm(value) {\n";
  ptr += "      if(value == 1) {\n";  
  ptr += "        document.forms[0].action = \"/setupMode\";";
  ptr += "        document.forms[0].submit();\n";
  ptr += "      } else {\n";
  ptr += "        document.forms[0].action = \"/\";";
  ptr += "        document.forms[0].submit();\n";
  ptr += "      }\n";
  ptr += "    }\n";
  ptr += "    </script>\n";
  ptr += "  </head>\n";
  ptr += "  <body>\n";
  ptr += "    <center>\n";
  ptr += "      <font face=\"Arial\">\n";
  ptr += "      <h1>Device Configuration</h1>\n";
  ptr += "      <br>\n";
  ptr += "      <form method=\"GET\">\n";
  ptr += "      <table>\n";
  ptr += "        <tbody>\n";
  ptr += "          <tr>\n";
  ptr += "            <td><b>SSID:</b></td>\n";
  ptr += "            <td>\n";
  ptr += "              <input type=\"text\" name=\"ssid\" size=\"32\" maxlength=\"32\" value=\"";
  ptr += data.ssid;
  ptr += "\"></input>\n";
  ptr += "            </td>\n";
  ptr += "          </tr>\n";
  ptr += "          <tr><td>&nbsp;</td></tr>\n";
  ptr += "          <tr>\n";
  ptr += "            <td><b>Key:</b></td>\n";
  ptr += "            <td>\n";
  ptr += "              <input type=\"text\" name=\"key\" size=\"20\" maxlength=\"20\" value=\"";
  ptr += data.password;
  ptr += "\"></input>\n";
  ptr += "            </td>\n";
  ptr += "          </tr>\n";
  ptr += "          <tr><td>&nbsp;</td></tr>\n";
  ptr += "          <tr>\n";
  ptr += "            <td><b>Mode:</b></td>\n";
  ptr += "            <td>\n";
  ptr += "              <input type=\"radio\" name=\"mode\" value=\"ap\" ";
  if (data.isModeAccessPoint) {
    ptr += "checked=\"checked\"";
  }
  ptr += "> Access Point</input>\n";
  ptr += "              &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\n";
  ptr += "              <input type=\"radio\" name=\"mode\" value=\"station\" ";
  if (!data.isModeAccessPoint) {
    ptr += "checked=\"checked\"";
  }
  ptr += "> Station</input>\n";
  ptr += "            </td>\n";
  ptr += "          </tr>\n";
  ptr += "        </tbody>\n";
  ptr += "      </table>\n";
  ptr += "          <br><br><br>\n";
  ptr += "      <table cellpadding='30' cellspacing='0'>\n";
  ptr += "          <tr>\n";
  ptr += "            <td align=\"center\"><button id='cancel' name='cancel' onclick='return submitForm(0)'>  Cancel  </button></td>\n";
  ptr += "            <td align=\"center\"><button id='save' name='save' onclick='return submitForm(1)'>  Save  </button></td>\n";
  ptr += "          </tr>\n";
  ptr += "      </table>\n";
  ptr += "    </form>\n";
  ptr += "      </font>\n";
  ptr += "    </center>\n";
  ptr +=      FOOTER;
  ptr += "  </body>\n";
  ptr += "</html>\n";
  server.send(200, "text/html", ptr);
  server.client().stop();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
  server.client().stop();
}

void handleSetupMode() {
  String deviceMode = server.arg("mode");
  String ssid = server.arg("ssid");
  String key = server.arg("key");

  if (ssid == NULL || ssid.length() < 1) {
    server.send(200, "text/html", getMessagePageHTML("Error", "red", "SSID Cannot be blank", "/configure"));
    server.client().stop();
    return;
  }

  if (key == NULL || key.length() < 6) {
    server.send(200, "text/html", getMessagePageHTML("Error", "red", "Key Cannot be less than 6 characters", "/configure"));
    server.client().stop();
    return;
  }

  if (deviceMode == "ap") {
    data.isModeAccessPoint = true;
    storeEEPROMData();
    server.send(200, "text/html", getMessagePageHTML("Operation Successful", "green", "Settings saved successfully, rebooting..", "/configure"));
    server.client().stop();
    isESPNeedsToBeRestarted = true;

  } else {
    data.isModeAccessPoint = false;
    ssid.toCharArray(data.ssid, ARRAY_SIZE);
    key.toCharArray(data.password, ARRAY_SIZE);
    data.isAutoOn = true;
    storeEEPROMData();
    server.send(200, "text/html", getMessagePageHTML("Operation Successful", "green", "Settings saved successfully, rebooting..", "/configure"));
    server.client().stop();
    isESPNeedsToBeRestarted = true;
  }
}

void restartDevice() {
  server.send(200, "text/html", getMessagePageHTML("Operation Successful", "green", "Rebooting the Device..", "/"));
  server.client().stop();
  isESPNeedsToBeRestarted = true;
}

void factoryResetDevice() {
  server.send(200, "text/html", getMessagePageHTML("Operation Successful", "green", "Factory Reset complete, rebooting the Device..", "/"));
  server.client().stop();
  doFactoryResetAndRestart();
}

String getMessagePageHTML(String title, String textColor, String message, String backURL) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<html>";
  ptr += "  <head>";
  ptr += "    <title>";
  ptr += title;
  ptr += "    </title>";
  ptr += "  </head>";
  ptr += "  <body>";
  ptr += "    <center>";
  ptr += "      <font face=\"Arial\" color=\"";
  ptr += textColor;
  ptr += "\"><br>";
  ptr += "        <h1>";
  ptr += message;
  ptr += ".</h1>";
  ptr += "        <br>";
  ptr += "        <a href=\"";
  ptr += backURL;
  ptr += "\">Go Back</a>";
  ptr += "      </font>";
  ptr += "    </center>";
  ptr += "  </body>";
  ptr += "</html>";
  return ptr;
}

void fillArrayWithSpaces(char* arr, int length) {
  for(int i=0; i<length; i++) {
    arr[i] = ' ';
  }
}

void doFactoryResetAndRestart() {
    setFactoryResetLED(true);
    data.isAutoOn = true;
    data.isModeAccessPoint = true;
    for(int i=0; i<ARRAY_SIZE; i++) {
      data.ssid[i] = '\0';
      data.password[i] = '\0';
    }
    storeEEPROMData();
    isESPNeedsToBeRestarted = true;
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
