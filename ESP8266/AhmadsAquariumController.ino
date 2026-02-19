#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#ifndef STASSID
#define STASSID   "Allahu Akbar"
#define STAPSK    "bg$786786"
#define HOSTNAME  "AquariumController"
#define DATA_REFRESH_DELAY 3000 // 3 Seconds
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

const int PIN_FILTER = 5;
const int PIN_BULB = 4;
const int PIN_SWITCH_FILTER = 0;
const int PIN_SWITCH_BULB = 2;


const long utcOffsetInSeconds = 19800;            // Seconds
const int TIME_UPDATE_INTERVAL = 10 * 60 * 1000;  // 10 Minutes
const int AQUARIUM_REST_TIME[] = {22,30};
const int AQUARIUM_ACTIVE_TIME[] = {5,30};

long lastTimeTimeUpdate = -1;           // For periodic fetch of time

// The Data Structure
struct SavedData {
  boolean isFilterOn = false;  // Off
  boolean isBulbOn = false;  // Off
} data;

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
PubSubClient mqtt(wifi);
long lastTimeStateUpdate = -1;

const char BROKER[] = "iot.quicksynclab.in";
int        PORT     = 1883;
const char FILTER_TOPIC_STATE[]  = "ahmads/aquariumControllerFilter/state";
const char FILTER_TOPIC_COMMAND[]  = "ahmads/aquariumControllerFilter/command";
const char BULB_TOPIC_STATE[]  = "ahmads/aquariumControllerBulb/state";
const char BULB_TOPIC_COMMAND[]  = "ahmads/aquariumControllerBulb/command";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

static const String FOOTER = String("<br/><br/><br/><br/><br/><br/><br/>\n")
                           + String("<font face=\"Arial\" size=\"2\">\n")
                           + String("<table width=\"100%\" bgcolor=\"lightgray\" style=\"color: black;\">\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("  <tr>\n")
                           + String("    <td colspan=\"2\" align=\"center\">&#169; Copyright 2020 Alithis Technologies - All Rights Reserved</td>\n")
                           + String("  </tr>\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("  <tr>\n")
                           + String("    <td colspan=\"2\" align=\"center\">&nbsp;<a href=\"https://alithistech.com\">Alithis Technologies</a></td>\n")
                           + String("  <tr>\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("  <tr>\n")
                           + String("    <td align=\"left\"><a href=\"mailto:getitdone@alithistech.com\">getitdone@alithistech.com</a></td>\n")
                           + String("    <td align=\"right\"><a href=\"tel:+919880506766\">+91-9880506766</a>&nbsp;</td>\n")
                           + String("  <tr>\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("</table>\n")
                           + String("</font>\n");

static const String STYLE   = String("   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">")
                            + String("   <style>")
                            + String("     .switch {")
                            + String("       position: relative;")
                            + String("       display: inline-block;")
                            + String("       width: 60px;")
                            + String("       height: 34px;")
                            + String("     }")
                            + String("     ")
                            + String("     .switch input {")
                            + String("       opacity: 0;")
                            + String("       width: 0;")
                            + String("       height: 0;")
                            + String("     }")
                            + String("     ")
                            + String("     .slider {")
                            + String("       position: absolute;")
                            + String("       cursor: pointer;")
                            + String("       top: 0;")
                            + String("       left: 0;")
                            + String("       right: 0;")
                            + String("       bottom: 0;")
                            + String("       background-color: #ccc;")
                            + String("       -webkit-transition: .4s;")
                            + String("       transition: .4s;")
                            + String("     }")
                            + String("     ")
                            + String("     .slider:before {")
                            + String("       position: absolute;")
                            + String("       content: \"\";")
                            + String("       height: 26px;")
                            + String("       width: 26px;")
                            + String("       left: 4px;")
                            + String("       bottom: 4px;")
                            + String("       background-color: white;")
                            + String("       -webkit-transition: .4s;")
                            + String("       transition: .4s;")
                            + String("     }")
                            + String("     ")
                            + String("     input:checked + .slider {")
                            + String("       background-color: #2196F3;")
                            + String("     }")
                            + String("     ")
                            + String("     input:focus + .slider {")
                            + String("       box-shadow: 0 0 1px #2196F3;")
                            + String("     }")
                            + String("     ")
                            + String("     input:checked + .slider:before {")
                            + String("       -webkit-transform: translateX(26px);")
                            + String("       -ms-transform: translateX(26px);")
                            + String("       transform: translateX(26px);")
                            + String("     }")
                            + String("     ")
                            + String("     /* Rounded sliders */")
                            + String("     .slider.round {")
                            + String("       border-radius: 34px;")
                            + String("     }")
                            + String("     ")
                            + String("     .slider.round:before {")
                            + String("       border-radius: 50%;")
                            + String("     }")
                            + String("   </style>");

void setup() {
  // Configure pins as output pins
  pinMode(PIN_FILTER, OUTPUT);
  pinMode(PIN_BULB, OUTPUT);

  // Initial State - OFF
  digitalWrite(PIN_FILTER, HIGH);
  digitalWrite(PIN_BULB, HIGH);
  
  pinMode(PIN_SWITCH_FILTER, INPUT);
  pinMode(PIN_SWITCH_BULB, INPUT);
  
  EEPROM.begin(sizeof(SavedData));

  // Restore stored data from EEPROM
  restoreEEPROMData();

  // Invalid data, store the data for the first time
  if(data.isFilterOn != data.isFilterOn) {
    data.isFilterOn = false;
    data.isBulbOn = false;
    storeEEPROMData();
  }

  // Set both outputs to stored previous state
  if(data.isFilterOn) {
    digitalWrite(PIN_FILTER, LOW);
  } else {
    digitalWrite(PIN_FILTER, HIGH);
  }
  if(data.isBulbOn) {
    digitalWrite(PIN_BULB, LOW);
  } else {
    digitalWrite(PIN_BULB, HIGH);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
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
  
  // Start the NTP client if connected to Internet
//  timeClient.setTimeOffset(utcOffsetInSeconds);
  timeClient.begin();

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

  // Periodic update of time
  if (currentTime - lastTimeTimeUpdate > TIME_UPDATE_INTERVAL || lastTimeTimeUpdate == -1) {
      timeClient.update();
      lastTimeTimeUpdate = currentTime;
  }

  if (currentTime - lastTimeStateUpdate > DATA_REFRESH_DELAY || lastTimeStateUpdate == -1) {
    
    lastTimeStateUpdate = currentTime;

    // Take action at correct time
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    int seconds = timeClient.getSeconds();

    if(hours == AQUARIUM_REST_TIME[0] && minutes == AQUARIUM_REST_TIME[1] && seconds >= 0 && seconds <= 3) {
      digitalWrite(PIN_FILTER, LOW);
      data.isFilterOn = true;
      digitalWrite(PIN_BULB, HIGH);
      data.isBulbOn = false;
      storeEEPROMData();
      mqtt.publish(FILTER_TOPIC_STATE, data.isFilterOn ? "1" : "0");
      mqtt.publish(BULB_TOPIC_STATE, data.isBulbOn ? "1" : "0");
      
    } else  if(hours == AQUARIUM_ACTIVE_TIME[0] && minutes == AQUARIUM_ACTIVE_TIME[1] && seconds >= 0 && seconds <= 3) {
      
      digitalWrite(PIN_FILTER, HIGH);
      data.isFilterOn = false;
      digitalWrite(PIN_BULB, LOW);
      data.isBulbOn = true;
      storeEEPROMData();
      mqtt.publish(FILTER_TOPIC_STATE, data.isFilterOn ? "1" : "0");
      mqtt.publish(BULB_TOPIC_STATE, data.isBulbOn ? "1" : "0");
    } else {
      
      mqtt.publish(FILTER_TOPIC_STATE, data.isFilterOn ? "1" : "0");
      mqtt.publish(BULB_TOPIC_STATE, data.isBulbOn ? "1" : "0");
    }
  }

  // Check if Filter Switch is pressed
  if(digitalRead(PIN_SWITCH_FILTER) == 0) {
    
    if(data.isFilterOn) {
      digitalWrite(PIN_FILTER, HIGH);
      data.isFilterOn = false;
    } else {
      digitalWrite(PIN_FILTER, LOW);
      data.isFilterOn = true;
    }
    
    mqtt.publish(FILTER_TOPIC_STATE, data.isFilterOn ? "1" : "0");
    storeEEPROMData();
    delay(500);
  }

  // Check if Bulb Switch is pressed
  if(digitalRead(PIN_SWITCH_BULB) == 0) {
    
    if(data.isBulbOn) {
      digitalWrite(PIN_BULB, HIGH);
      data.isBulbOn = false;
    } else {
      digitalWrite(PIN_BULB, LOW);
      data.isBulbOn = true;
    }
    
    mqtt.publish(BULB_TOPIC_STATE, data.isBulbOn ? "1" : "0");
    storeEEPROMData();
    delay(500);
  }

   mqtt.publish(FILTER_TOPIC_STATE, data.isFilterOn ? "1" : "0");
   mqtt.publish(BULB_TOPIC_STATE, data.isBulbOn? "1" : "0");
}

void reconnect() {
  // Reconnect and publish
  if (mqtt.connect(HOSTNAME, "mujeeb", "einstein")) {
    mqtt.publish(FILTER_TOPIC_STATE, data.isFilterOn ? "1" : "0");
    mqtt.publish(BULB_TOPIC_STATE, data.isBulbOn? "1" : "0");
    mqtt.subscribe(FILTER_TOPIC_COMMAND);
    mqtt.subscribe(BULB_TOPIC_COMMAND);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  String strTopic = String(topic);
  if(strTopic == String(FILTER_TOPIC_COMMAND)) {
    if((char)payload[0] == '1') {
      digitalWrite(PIN_FILTER, LOW);
      data.isFilterOn = true;
    } else {
      digitalWrite(PIN_FILTER, HIGH);
      data.isFilterOn = false;
    }
    storeEEPROMData();
    mqtt.publish(FILTER_TOPIC_STATE, data.isFilterOn ? "1" : "0");
  }

  if(strTopic == String(BULB_TOPIC_COMMAND)) {
    if((char)payload[0] == '1') {
      digitalWrite(PIN_BULB, LOW);
      data.isBulbOn = true;
    } else {
      digitalWrite(PIN_BULB, HIGH);
      data.isBulbOn = false;
    }
    storeEEPROMData();
    mqtt.publish(BULB_TOPIC_STATE, data.isBulbOn ? "1" : "0");
  }
}

void handleRoot() {

  String dev1 = server.arg("dev1");
  String dev2 = server.arg("dev2");

  if(dev1 != NULL && dev1.equals("toggle")) {
    if(data.isFilterOn) {
      digitalWrite(PIN_FILTER, HIGH);
      data.isFilterOn = false;
    } else {
      digitalWrite(PIN_FILTER, LOW);
      data.isFilterOn = true;
    }
    storeEEPROMData();
    mqtt.publish(FILTER_TOPIC_STATE, data.isFilterOn ? "1" : "0");
  }

  if(dev2 != NULL && dev2.equals("toggle")) {
    if(data.isBulbOn) {
      digitalWrite(PIN_BULB, HIGH);
      data.isBulbOn = false;
    } else {
      digitalWrite(PIN_BULB, LOW);
      data.isBulbOn = true;
    }
    storeEEPROMData();
    mqtt.publish(BULB_TOPIC_STATE, data.isBulbOn ? "1" : "0");
  }
  
  String html = "<html>";
  html += "   <head>";
  html += "     <title>Aquarium Controller</title>";
  html += STYLE;
  html += "     <script language='JavaScript'>";
  html += "       function submitForm(value) {";
  html += "         var xhttp = new XMLHttpRequest();";
  html += "         xhttp.onreadystatechange = function() {};";
  html += "         xhttp.open(\"GET\", \"http://" + WiFi.localIP().toString() + "\" + (value == 0 ? \"?dev1=toggle\" : \"?dev2=toggle\"), true);";
  html += "         xhttp.send();";
  html += "       }";
  html += "     </script>";
  html += "   </head>";
  html += "   <body>";
  html += "     <center>";
  html += "     <br>";
  html += "      <h1>Aquarium Controller</h1>";
  html += "      <br><br><br>";
  html += "      <table cellpadding=0 cellspacing=5>";
  html += "        <tr>";
  html += "          <td align='cemter'>Filter</td>";
  html += "          <td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>";
  html += "          <td align='center'>Bulb</td>";
  html += "        </tr>";
  html += "        <tr>";
  html += "          <td align='cemter'><label class=\"switch\"><input type=\"checkbox\" " + (String)(data.isFilterOn ? "checked" : "") + " onChange=\"submitForm(0)\"><span class=\"slider round\"></span></label></td>";
  html += "          <td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>";
  html += "          <td align='cemter'><label class=\"switch\"><input type=\"checkbox\" " + (String)(data.isBulbOn ? "checked" : "") + " onChange=\"submitForm(1)\"><span class=\"slider round\"></span></label></td>";
  html += "        </tr>";
  html += "      </table>";
  html += "  </center>";
  html += FOOTER;
  html += " </body>";
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
