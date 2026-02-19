#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include "fauxmoESP.h"

#define IDENTIFIER 7867
#ifndef STASSID
#define STASSID "Allahu Akbar"
#define STAPSK  "bg$786786"
#define HOSTNAME "Amplifier"
#define DEVICE_TITLE "Amplifier Switch"
#define DATA_REFRESH_DELAY 3000 // 3 Seconds
#define DEFAULT_ON_DURATION   2000
#define DEFAULT_OFF_DURATION  2000
#endif

fauxmoESP fauxmo;

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
                           + String("    <td align=\"left\"><a href=\"mailto:getitdone@alithistech.com\">mohammad.mujeeb@gmail.com</a></td>\n")
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

const char* ssid = STASSID;
const char* password = STAPSK;
const int   RELAY_PIN = 0; // or 2

// The Data Structure
struct SavedData {
  int identifier;
  int onTime;
  int offTime;
  bool isSwitchOn;  // Off
  bool isBlinkOn;   // Off
} data;

// Set web server port number to 81
ESP8266WebServer server(81);

WiFiClient wifi;
PubSubClient mqtt(wifi);
long lastTimeStateUpdate = -1;

long lastTimeBlink = 0;

const char BROKER[] = "iot.quicksynclab.in";
int        PORT     = 1883;
const char TOPIC_STATE_SWITCH[]   = "ahmads/amplifier/switch/state";
const char TOPIC_COMMAND_SWITCH[] = "ahmads/amplifier/switch/command";
const char TOPIC_STATE_BLINK[]    = "ahmads/amplifier/blink/state";
const char TOPIC_COMMAND_BLINK[]  = "ahmads/amplifier/blink/command";

void setup() {

  // Configure pins as output pins
  pinMode(RELAY_PIN, OUTPUT);
  // Initial state - OFF
  digitalWrite(RELAY_PIN, HIGH);
  
  EEPROM.begin(sizeof(SavedData));

  // Restore stored data from EEPROM
  restoreEEPROMData();

  // Invalid data, store the data for the first time
  if(data.identifier != IDENTIFIER) {
    data.identifier = IDENTIFIER;
    data.onTime = DEFAULT_ON_DURATION;
    data.offTime = DEFAULT_OFF_DURATION;
    data.isSwitchOn = false;
    data.isBlinkOn = false;
    storeEEPROMData();
  }

  // Set the output to stored previous value
  if(data.isSwitchOn) {
    digitalWrite(RELAY_PIN, LOW);
  } else {
    digitalWrite(RELAY_PIN, HIGH);
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

  // By default, fauxmoESP creates it's own webserver on the defined port
  // The TCP port must be 80 for gen3 devices (default is 1901)
  // This has to be done before the call to enable()
  fauxmo.createServer(true); // not needed, this is the default value
  fauxmo.setPort(80); // This is required for gen3 devices

  // You have to call enable(true) once you have a WiFi connection
  // You can enable or disable the library at any moment
  // Disabling it will prevent the devices from being discovered and switched
  fauxmo.enable(true);

  // Add virtual device
  fauxmo.addDevice(HOSTNAME);

  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        
  if(state) {
    digitalWrite(0, LOW);
    data.isSwitchOn = true;
    mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
  }else {
    digitalWrite(0, HIGH);
    data.isSwitchOn = false;
    mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
  }
 });
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

  // fauxmoESP uses an async TCP server but a sync UDP server
  // Therefore, we have to manually poll for UDP packets
  fauxmo.handle();

  long currentTime = millis();
  if (currentTime - lastTimeStateUpdate > DATA_REFRESH_DELAY || lastTimeStateUpdate == -1) {
      mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
      mqtt.publish(TOPIC_STATE_BLINK, data.isBlinkOn ? "1" : "0");
      lastTimeStateUpdate = currentTime;
  }

  if(data.isBlinkOn) {
    if(digitalRead(0) == LOW) {
      if(millis() - lastTimeBlink > data.onTime) {
        digitalWrite(0, HIGH);
        data.isSwitchOn = false;
        mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
        lastTimeBlink = millis();
      }
    } else {
      if(millis() - lastTimeBlink > data.offTime) {
        digitalWrite(0, LOW);
        data.isSwitchOn = true;
        mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
        lastTimeBlink = millis();
      }
    }
  }
}

void reconnect() {
  // Reconnect and publish
  if (mqtt.connect(HOSTNAME, "mujeeb", "einstein")) {
    mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
    mqtt.publish(TOPIC_STATE_BLINK, data.isBlinkOn ? "1" : "0");
    mqtt.subscribe(TOPIC_COMMAND_SWITCH);
    mqtt.subscribe(TOPIC_COMMAND_BLINK);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  String strTopic = String(topic);
  if(strTopic == String(TOPIC_COMMAND_SWITCH)) {
  
    if((char)payload[0] == '1') {
      digitalWrite(RELAY_PIN, LOW);
      data.isSwitchOn = true;
    } else {
      digitalWrite(RELAY_PIN, HIGH);
      data.isSwitchOn = false;
    }

    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
  }

  if(strTopic == String(TOPIC_COMMAND_BLINK)) {
  
    if((char)payload[0] == '1') {
      data.isBlinkOn = true;
    } else {
      data.isBlinkOn = false;
      digitalWrite(0, HIGH);
      data.isSwitchOn = false;
      mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
    }

    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_BLINK, data.isBlinkOn ? "1" : "0");
  }
}

void handleRoot() {

  bool switchState = digitalRead(RELAY_PIN) == LOW;
  String command = server.arg("sendCommand");

  if(server.arg("ontime") != NULL && server.arg("offtime") != NULL) {
    data.onTime = server.arg("ontime").toInt() * 1000;
    data.offTime = server.arg("offtime").toInt() * 1000;
    storeEEPROMData();
  }

  if(command.charAt(0) == '0') {
    switchState = !switchState;
    
    if(switchState) {
      digitalWrite(RELAY_PIN, LOW);
      data.isSwitchOn = true;
    } else {
      digitalWrite(RELAY_PIN, HIGH);
      data.isSwitchOn = false;
    }
    
    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
    
  }
  
  if(command.charAt(0) == '1'){
    data.isBlinkOn = !data.isBlinkOn;
    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_BLINK, data.isBlinkOn ? "1" : "0");

    if(!data.isBlinkOn) {
      digitalWrite(0, HIGH);
      data.isSwitchOn = false;
      mqtt.publish(TOPIC_STATE_SWITCH, data.isSwitchOn ? "1" : "0");
    }
  }
  
  String html = "<html>";
  html += "   <head>";
  html += "     <title>" + String(DEVICE_TITLE) + "</title>";
  html += STYLE;
  html += "     <script language='JavaScript'>";
  html += "       function submitForm(value) {";
  html += "          if(value == 0) {";
  html += "            document.getElementById('sendCommand').value = '0';";
  html += "          } else if(value == 1){";
  html += "            document.getElementById('sendCommand').value = '1';";
  html += "          } else {";
  html += "            document.getElementById('sendCommand').value = '2';";
  html += "          }";
  html += "          document.forms[0].submit();";
  html += "       }";
  html += "       function valueChanged(value) {";
  html += "         var xhttp = new XMLHttpRequest();";
  html += "         xhttp.onreadystatechange = function() {};";
  html += "         xhttp.open(\"GET\", \"http://" + WiFi.localIP().toString() + "?sendCommand=\" + (value == 0 ? \"0\" : \"1\"), true);";
  html += "         xhttp.send();";
  html += "       }";
  html += "     </script>";
  html += "   </head>";
  html += "   <body>";
  html += "     <center>";
  html += "     <br>";
  html += "       <h1>" + String(DEVICE_TITLE) + "</h1>";
  html += "     <br>";
  html += "      <table border='1' cellpadding='20' cellspacing='0'>";
  html += "        <form method='GET'>";
  html += "           <tr>";
  html += (String)"         <td align='center'>On Time: <input name='ontime' id='ontime' type='text' size='3' maxlength='3' value='" + (data.onTime/1000) + "'></td>";
  html += (String)"         <td align='center'>Off Time: <input name='offtime' id='offtime' type='text' size='3' maxlength='3' value='" + (data.offTime/1000) + "'></td>";
  html += "           </tr>";
  html += "           <tr>";
  html += "             <input type='hidden' id='sendCommand' name='sendCommand'>";
  html += "             <td align='center' colspan='2'><button onclick='submitForm(3)'>Save</button></td>";
  html += "           </tr>";
  html += "        </form>";
  html += "      </table>";
  html += "      <br><br><br>";
  html += "      <table cellpadding=0 cellspacing=5>";
  html += "        <tr>";
  html += "          <td align='cemter'>Blink</td>";
  html += "          <td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>";
  html += "          <td align='center'>Switch</td>";
  html += "        </tr>";
  html += "        <tr>";
  html += "          <td align='cemter'><label class=\"switch\"><input type=\"checkbox\" " + (String)(data.isBlinkOn ? "checked" : "") + " onChange=\"valueChanged(1)\"><span class=\"slider round\"></span></label></td>";
  html += "          <td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>";
  html += "          <td align='cemter'><label class=\"switch\"><input type=\"checkbox\" " + (String)(switchState ? "checked" : "") + " onChange=\"valueChanged(0)\"><span class=\"slider round\"></span></label></td>";
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
