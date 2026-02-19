#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#ifndef STASSID
#define STASSID "Ahmads"
#define STAPSK  "Xg#609590"
#define HOSTNAME "InternetMasterSwitch"
#define DEVICE_TITLE "Internet Master Switch"
#define DATA_REFRESH_DELAY 3000 // 3 Seconds
#endif

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

const char* ssid = STASSID;
const char* password = STAPSK;
const int   RELAY_PIN = 0; // or 2

// The Data Structure
struct SavedData {
  boolean isSwitchOn = false;  // Off
} data;

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
PubSubClient mqtt(wifi);
long lastTimeStateUpdate = -1;

const char BROKER[] = "192.168.68.173";
int        PORT     = 1883;
const char TOPIC_STATE[]  = "ahmads/internetMasterSwitch/state";
const char TOPIC_COMMAND[]  = "ahmads/internetMasterSwitch/command";

void setup() {

  // Configure pins as output pins
  pinMode(RELAY_PIN, OUTPUT);
  // Initial state - OFF
  digitalWrite(RELAY_PIN, HIGH);
  
  EEPROM.begin(sizeof(SavedData));

  // Restore stored data from EEPROM
  restoreEEPROMData();

  // Invalid data, store the data for the first time
  if(data.isSwitchOn != data.isSwitchOn) {
    data.isSwitchOn = false;
    storeEEPROMData();
  }

  // Set the output to stored previous value
  if(data.isSwitchOn) {
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);                             // wait for 0.2 second
  }

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  ArduinoOTA.setHostname(HOSTNAME);

  ArduinoOTA.onStart([]() {
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
  server.begin();

  mqtt.setServer(BROKER, PORT);
  mqtt.setCallback(callback);
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
    mqtt.loop();
  } else {
    mqtt.loop();
  }

  // Handle the OTA Functionality
  ArduinoOTA.handle();

  //Handling of incoming requests
  server.handleClient();

  long currentTime = millis();
  if (currentTime - lastTimeStateUpdate > DATA_REFRESH_DELAY || lastTimeStateUpdate == -1) {
      mqtt.publish(TOPIC_STATE, data.isSwitchOn ? "1" : "0");
      lastTimeStateUpdate = currentTime;
  }
}

void reconnect() {
  // Reconnect and publish
  if (mqtt.connect(HOSTNAME, "mqtt", "Mummy@123")) {
    mqtt.publish(TOPIC_STATE, data.isSwitchOn ? "1" : "0");
    mqtt.subscribe(TOPIC_COMMAND);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  if((char)payload[0] == '1') {
    digitalWrite(RELAY_PIN, HIGH);
    data.isSwitchOn = true;
  } else {
    digitalWrite(RELAY_PIN, LOW);
    data.isSwitchOn = false;
  }

  storeEEPROMData();
  mqtt.publish(TOPIC_STATE, data.isSwitchOn ? "1" : "0");
}

void handleRoot() {

  bool switchState = digitalRead(RELAY_PIN) == HIGH;
  String device = server.arg("device");

  if(device != NULL && device.equals("toggle")) {
    switchState = !switchState;
    
    if(switchState) {
      digitalWrite(RELAY_PIN, HIGH);
      data.isSwitchOn = true;
    } else {
      digitalWrite(RELAY_PIN, LOW);
      data.isSwitchOn = false;
    }
    
    storeEEPROMData();
    mqtt.publish(TOPIC_STATE, data.isSwitchOn ? "1" : "0");
  }
  
  String html = "<html>";
  html += "   <head>";
  html += "     <title>" + String(DEVICE_TITLE) + "</title>";
  html += "     <script language='JavaScript'>";
  html += "       function submitForm() {";
  html += "          document.getElementById('device').value = 'toggle';";
  html += "          document.forms[0].submit();";
  html += "       }";
  html += "     </script>";
  html += "   </head>";
  html += "   <body>";
  html += "     <center>";
  html += "     <br>";
  html += "       <h1>" + String(DEVICE_TITLE) + "</h1>";
  html += "     <br>";
  html += "      <table border='1' cellpadding='20' cellspacing='0'>";
  html += "       <tr>";
  html += (String)"         <td align='center'>Switch State: <font color='" + (switchState ? (String)"green'>On" : (String)"red'>Off") + (String)"</font></td>";
  html += "       </tr>";
  html += "       <tr>";
  html += "       <form method='GET'>";
  html += "         <td align='center'><button id='device' name='device' onclick='submitForm()'>" + (switchState ? (String)"Turn Off" : (String)"Turn On") + (String)"</button></td>";
  html += "      </form>";
  html += "    </tr>";
  html += "   </table>";
  html += "  </center>";
  html += " </body>";
  html += FOOTER;
  html += "</html>";
  server.send(200, "text/html", html);
  server.client().stop();
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
