#include <stdlib.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>

#ifndef STASSID
#define STASSID   "Ahmads"
#define STAPSK    "Xg#609590"
#define HOSTNAME  "PowerMonitor2"
#define POWER_CHECK_INTERVAL 10000
#endif

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
PubSubClient mqtt(wifi);

const char BROKER[] = "192.168.68.173";
int        PORT     = 1883;
const char TOPIC_STATE_VOLTAGE_PHASE1[]  = "ahmads/acVoltage/phase1";
const char TOPIC_STATE_VOLTAGE_PHASE2[]  = "ahmads/acVoltage/phase2";
const char TOPIC_STATE_VOLTAGE_PHASE3[]  = "ahmads/acVoltage/phase3";
const char TOPIC_STATE_UPS_1[]           = "ahmads/ups/1";
const char TOPIC_STATE_UPS_2[]           = "ahmads/ups/2";
const char TOPIC_POWER_STATUS[]          = "ahmads/powerStatus";

long previousTimePowerCheck = -1;

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(STASSID, STAPSK);
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
  
  mqtt.setServer(BROKER, PORT);
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
  // Check if power is down
  if(currentTime - previousTimePowerCheck > POWER_CHECK_INTERVAL) {
    
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE1, "220");
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE2, "220");
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE3, "220");
    mqtt.publish(TOPIC_STATE_UPS_1, "100");
    mqtt.publish(TOPIC_STATE_UPS_2, "100");
    mqtt.publish(TOPIC_POWER_STATUS, "Yes");
    previousTimePowerCheck = currentTime;
  }
}

void reconnect() {
  // Reconnect and publish
  if (mqtt.connect(HOSTNAME, "mqtt", "Mummy@123")) {
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE1, "220");
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE2, "220");
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE3, "220");
    mqtt.publish(TOPIC_STATE_UPS_1, "100");
    mqtt.publish(TOPIC_STATE_UPS_2, "100");
    mqtt.publish(TOPIC_POWER_STATUS, "Yes");
  }
}

void handleRoot() {
  String html = "<html>";
  html += "  <head><title>Power Monitor</title></head>";
  html += "   <body>";
  html += "     <center>";
  html += "     <br>";
  html += "       <h1>Power Monitor</h1>";
  html += "     <br>";
  html += "      <table border='1' cellpadding='20' cellspacing='0'>";
  html += "       <tr>";
  html += "         <td colspan='3' align='center'>Power Status: <font color='blue'>" + (String) "Yes" + "</font></td>";
  html += "       </tr>";
  html += "       <tr>";
  html += "         <td align='center'>Phase 1: <font color='blue'>" + (String) "220" + "V</font></td>";
  html += "         <td align='center'>Phase 2: <font color='blue'>" + (String) "220" + "V</font></td>";
  html += "         <td align='center'>Phase 3: <font color='blue'>" + (String) "220" + "V</font></td>";
  html += "       </tr>";
  html += "     </table>";
  html += "     <br>";
  html += "     <table border='1' cellpadding='20' cellspacing='0'>";
  html += "       <tr>";
  html += "         <td align='center'>Groud floor UPS: <font color='green'>" + (String) "100" + "%</font></td>";
  html += "         <td align='center'>Second Floor UPS: <font color='green'>" + (String) "100" + "%</font></td>";
  html += "       </tr>";
  html += "     </table>";
  html += "   </center>";
  html += " </body>";
  html += "</html>";
  server.send(200, "text/html", html);
  server.client().stop();
}
