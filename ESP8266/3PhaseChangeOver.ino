#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <MCP3XXX.h>

#define IS_DUMMY_MODE false
#define SERIAL_NUMBER "6858929475"

#define ANALOG_1 1
#define ANALOG_2 0
#define ANALOG_3 2

#define PIN_WIFI_CONNECTED    1
#define PIN_WIFI_DISCONNECTED 0
#define PIN_PHASE_SELECT_RYB  2
#define PIN_PHASE_SELECT_RBY  3
#define PIN_SWITCH            4

#define PIN_RELAY_RYB      5
#define PIN_RELAY_RBY      16

#define DATA_REFRESH_DELAY 10000 // 10 Seconds

String AP_SSID = String("ALITHIS-") + SERIAL_NUMBER;
char HOSTNAME[21] = "CHNGOVR-";

const char BROKER[] = "io.alithistech.com";
//const char BROKER[] = "192.168.68.173";
int        PORT     = 1883;
String TOPIC_CHANGE_OVER_STATUS = "phaseChangeOver/" + String(SERIAL_NUMBER) + "/status";

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
                           + String("    <td align=\"center\"><a href=\"mailto:getitdone@alithistech.com\">getitdone@alithistech.com</a></td>\n")
                           + String("    <td align=\"right\"><a href=\"tel:+919880506766\">+91-9880506766</a>&nbsp;</td>\n")
                           + String("  <tr>\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("</table>\n")
                           + String("</font>\n");

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
PubSubClient mqtt(wifi);

MCP3008 adc;

#define ARRAY_SIZE 33

// The Data Structure
struct Data {
  bool isChangeOver = false;
  bool isModeAccessPoint = true;
  char ssid[ARRAY_SIZE] = "Ahmads";
  char password[ARRAY_SIZE] = "Xg#609590";
} data;

const int MQTT_CONNECT_TRY_INTERVAL         = 5000;                  // 5 Seconds
const int SWITCH_AUTO_RESET_INTERVAL        = 5000;                  // 5 Seconds
const int AC_SAMPLING_INTERVAL              = 1000;                  // 1 Second
const int WIFI_CONNECTION_TIMEOUT           = 60000;                 // 1 Minute
const int FACTORY_RESET_BEGIN_TIMEOUT       = 3000;                  // 3 Seconds
const int FACTORY_RESET_TIMEOUT             = 10000;                 // 10 Seconds
const int SETUP_MODE_BLINK_DURATION         = 500;                   // 0.5 Seconds
long lastTimeWifiConnect = -1;
long lastTimeFactoryReset = -1;
bool isESPNeedsToBeRestarted = false;

bool setUpModeBlink = false;
long lastTimeSetupMode = -1;
long lastTimeStatePublish = -1;

int prev1Ph1, prev1Ph2, prev1Ph3;
int prev2Ph1, prev2Ph2, prev2Ph3;

bool phase1Trigger = false, phase2Trigger = false, phase3Trigger = false;
bool gapTrigger = true;

int RYB = 0;
int RBY = 0;

long lastTimeACRead = -1;
long lasttimeGap = -1;
long lastTimeMqttConnect = -1;

void setup() {
  // Construct Hostname
  for(int i=8; i<sizeof(HOSTNAME); i++) {
    HOSTNAME[i] = SERIAL_NUMBER[i-8];
  }

  if(!IS_DUMMY_MODE) {
    // GPIO Pin Configuration
    pinMode(PIN_WIFI_CONNECTED, OUTPUT);
    pinMode(PIN_WIFI_DISCONNECTED, OUTPUT);
    pinMode(PIN_PHASE_SELECT_RYB, OUTPUT);
    pinMode(PIN_PHASE_SELECT_RBY, OUTPUT);
    pinMode(PIN_RELAY_RYB, OUTPUT);
    pinMode(PIN_RELAY_RBY, OUTPUT);
    pinMode(PIN_SWITCH, INPUT);
  
    adc.begin();
  }
  
  EEPROM.begin(sizeof(Data));

  // Restore stored data from EEPROM
  restoreEEPROMData();
  
  // Invalid data, store the data for the first time
  if(data.ssid[0] != data.ssid[0]) {
    data.isChangeOver = false;
    data.isModeAccessPoint = true;
    for(int i=0; i<ARRAY_SIZE; i++) {
      data.ssid[i] = '\0';
      data.password[i] = '\0';
    }
    storeEEPROMData();
  }

  if(!IS_DUMMY_MODE) {
    // Set the stored Phase Selection
    setChangeOver(data.isChangeOver);
    setWiFiConnected(false);
  }

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
    
    long currentTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
      long currentTime = millis();
      // Unable to connect to WiFi, come out of the loop
      if (currentTime - lastTimeWifiConnect > WIFI_CONNECTION_TIMEOUT) {
          break;
      }
      delay(200);
    }
    setWiFiConnected(true);
  }

  // Begin MDNS Service
  MDNS.begin(HOSTNAME);

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(HOSTNAME);

  ArduinoOTA.onStart([]() {
    digitalWrite(0, LOW);
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

  if(!data.isModeAccessPoint && !IS_DUMMY_MODE) {
    mqtt.setServer(BROKER, PORT);
    lastTimeMqttConnect = millis();
  }

  prev1Ph1 = prev2Ph1 = adc.analogRead(ANALOG_1);
  prev1Ph2 = prev2Ph2 = adc.analogRead(ANALOG_2);
  prev1Ph3 = prev2Ph3 = adc.analogRead(ANALOG_3);
  lasttimeGap = millis();
  lastTimeACRead = lasttimeGap;
}

void loop(){

  // Handle the OTA Functionality
  ArduinoOTA.handle();

  //Handling of incoming requests
  server.handleClient();

//  if(millis() - lasttimeGap > 5000) {
//    setChangeOver(data.isChangeOver);
//    lasttimeGap = millis();
//  }
//
//  return;

  if (isESPNeedsToBeRestarted) {
    delay(300);
    ESP.restart();
  }

  if(IS_DUMMY_MODE) {
    return;
  }

  long currentTime = millis();

  // Try reconnecting to MQTT Broker only periodically
  if(!mqtt.connected() && currentTime - lastTimeMqttConnect > MQTT_CONNECT_TRY_INTERVAL) {

    reconnectMQTT();
    lastTimeMqttConnect = currentTime;
  }

  gapTrigger = millis() - lasttimeGap > 20;

  int phase1 = adc.analogRead(ANALOG_1);
  int phase2 = adc.analogRead(ANALOG_2);
  int phase3 = adc.analogRead(ANALOG_3);

  if(prev1Ph1 == 1023 && prev1Ph1 == prev2Ph1 && prev2Ph1 > phase1 && !phase1Trigger && !phase2Trigger && !phase3Trigger && gapTrigger) {
  
    phase1Trigger = false;
    phase2Trigger = true;
    phase3Trigger = true;
  }

  if(prev1Ph2 == 1023 && prev1Ph2 == prev2Ph2 && prev2Ph2 > phase2 && phase2Trigger && gapTrigger) {

    RYB++;
    phase2Trigger = false;
    phase3Trigger = false;

    lasttimeGap = millis();
  }

  if(prev1Ph3 == 1023 && prev1Ph3 == prev2Ph3 && prev2Ph3 > phase3 && phase3Trigger && gapTrigger) {

    RBY++;
    phase3Trigger = false;
    phase2Trigger = false;

    lasttimeGap = millis();
  }

  prev1Ph1 = prev2Ph1;
  prev2Ph1 = phase1;
  prev1Ph2 = prev2Ph2;
  prev2Ph2 = phase2;
  prev1Ph3 = prev2Ph3;
  prev2Ph3 = phase3;

  if(currentTime - lastTimeACRead > AC_SAMPLING_INTERVAL) {

    if(RYB > RBY && data.isChangeOver) {
      setChangeOver(false);
      publishStatesToMQTT();
    }

    if(RYB < RBY && !data.isChangeOver) {
      setChangeOver(true);
      publishStatesToMQTT();
    }

    lastTimeACRead = millis();
    RYB = 0;
    RBY = 0;
  }

  // Blink the connection LED red/green in Setup mode
  if(data.isModeAccessPoint) {
    if(currentTime - lastTimeSetupMode > SETUP_MODE_BLINK_DURATION || lastTimeSetupMode == -1) {
      setWiFiConnected(setUpModeBlink);
      setUpModeBlink = !setUpModeBlink;
      lastTimeSetupMode = millis();
    }
  } else {
    setWiFiConnected(WiFi.status() == WL_CONNECTED);
    
    mqtt.loop();
    
    if (currentTime - lastTimeStatePublish > DATA_REFRESH_DELAY || lastTimeStatePublish == -1) {
      publishStatesToMQTT();
      lastTimeStatePublish = millis();
    }
  }

  // Factory Reset Logic
  if(readSwitchValue() == LOW) {

    lastTimeFactoryReset = currentTime;
    while(readSwitchValue() == LOW) {

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
          if(readSwitchValue() == HIGH) {
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
        }
      }                                                          

      delay(200);
    }
  }
}

int readSwitchValue() {
  digitalWrite(PIN_SWITCH, HIGH);
  return digitalRead(PIN_SWITCH);
}

String getPhaseSelectionString() {
  return data.isChangeOver ? "Engaged" : "Not Engaged";
}

void setChangeOver(bool selection) {

  data.isChangeOver = selection;
  storeEEPROMData();

  digitalWrite(PIN_PHASE_SELECT_RYB, selection ? LOW : HIGH);
  digitalWrite(PIN_PHASE_SELECT_RBY, selection ? HIGH : LOW);

  digitalWrite(PIN_RELAY_RYB, selection ? HIGH : LOW);
  digitalWrite(PIN_RELAY_RBY, selection ? LOW : HIGH);
}

void publishStatesToMQTT() {
  if(mqtt.connected()) {
    mqtt.publish(TOPIC_CHANGE_OVER_STATUS.c_str(), data.isChangeOver ? "Engaged" : "Not Engaged");
  }
}

void setWiFiConnected(bool status) {
  digitalWrite(PIN_WIFI_CONNECTED, status ? LOW : HIGH);
  digitalWrite(PIN_WIFI_DISCONNECTED, status ? HIGH : LOW);
}

void setFactoryResetLED(bool status) {
  digitalWrite(PIN_WIFI_CONNECTED, HIGH);
  digitalWrite(PIN_WIFI_DISCONNECTED, status ? LOW : HIGH);
}

void reconnectMQTT() {
  // Reconnect
  mqtt.connect(HOSTNAME, "mqtt", "Mummy@123");
}

void handleRoot() {

  String html = String("<html>\n")
              + String("  <head>\n")
              + String("    <title>Smart 3 Phase Automatic Change Over System</title>\n")
              + String("    <script language='JavaScript'>\n")
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
              + String("         document.forms[0].action = \"/factoryResetDevice\";\n")
              + String("         document.forms[0].submit();\n")
              + String("        } else if(value == 1) {\n")
              + String("         document.forms[0].action = \"/restartDevice\";\n")
              + String("         document.forms[0].submit();\n")
              + String("        } else {\n")
              + String("         document.forms[0].action = \"/configure\";\n")
              + String("         document.forms[0].submit();\n")
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
              + String("      <h1><font face='Arial'>Smart 3 Phase Automatic Change Over System</font></h1>\n")
              + String("      <br>\n")
              + String("      <table border='1' cellpadding='20' cellspacing='0'>\n")
              + String("        <tr>\n")
              + String("          <td colspan='3' align='center'><font face='Arial'>Status: <font color='blue'><b>" + getPhaseSelectionString() + "</b></font></font></td>\n")
              + String("        </tr>\n")
//              + String("        <tr>\n")
//              + String("            <td colspan='2' align='center'>" + (String) adc.analogRead(ANALOG_1) + " : " + (String) adc.analogRead(ANALOG_2) + " : " + (String) adc.analogRead(ANALOG_3) + "</td>\n")
//              + String("            <td align='center'>" + (String) RYB + "</td>\n")
//              + String("            <td align='center'>" + (String) RBY + "</td>\n")
//              + String("        </tr>\n")
              + String("      </table>\n")
              + String("      <br><br><br>\n")
              + String("      <table border='0' cellpadding='20' cellspacing='0'>\n")
              + String("        <tr>\n")
              + String("          <form method='GET'>\n")
              + String("            <td align='center'><button onclick='return deviceControls(0)'>Factory Reset</button></td>\n")
              + String("            <td align='center'><button onclick='return deviceControls(1)'>Restart</button></td>\n")
              + String("            <td align='center'><button onclick='return deviceControls(2)'>Configure</button></td>\n")
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
    data.isChangeOver = false;
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

void doFactoryResetAndRestart() {
    setFactoryResetLED(true);
    data.isChangeOver = false;
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
