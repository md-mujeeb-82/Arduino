#include <stdlib.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <MCP3XXX.h>

#ifndef STASSID
#define STASSID   "Ahmads"
#define STAPSK    "Xg#609590"
#define HOSTNAME  "PowerMonitor"
#define POWER_CHECK_INTERVAL 10000  // 5 Seconds
#endif

const char* ssid              = STASSID;
const char* password          = STAPSK;
const float stepUPS1          = 0.024759322113618; // 1 Step
const float stepUPS2          = 0.0249188892455211; // 1 Step

char strVoltage[10];

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
PubSubClient mqtt(wifi);

MCP3008 adc;

const char BROKER[] = "192.168.68.173";
int        PORT     = 1883;
const char TOPIC_STATE_UPS_1[]           = "ahmads/ups/1";
const char TOPIC_STATE_UPS_2[]           = "ahmads/ups/2";

long previousTimePowerCheck = -1;

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

void setup() {
  adc.begin();
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

  int currentTime = millis();
  
  // Check if power is down
  if(currentTime - previousTimePowerCheck > POWER_CHECK_INTERVAL) {
    
    dtostrf(getUPSBatteryPercentage(1), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_UPS_1, strVoltage);
    dtostrf(getUPSBatteryPercentage(2), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_UPS_2, strVoltage);
    
    previousTimePowerCheck = currentTime;
  }
}

void reconnect() {
  // Reconnect and publish
  if (mqtt.connect(HOSTNAME, "mqtt", "Mummy@123")) {
    
    dtostrf(getUPSBatteryPercentage(1), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_UPS_1, strVoltage);
    dtostrf(getUPSBatteryPercentage(2), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_UPS_2, strVoltage);
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
  html += "     <table border='1' cellpadding='20' cellspacing='0'>";
  html += "       <tr>";
  html += "         <td align='center'>Groud floor UPS: <font color='green'>" + (String) getUPSBatteryPercentage(2) + "%</font></td>";
  html += "         <td align='center'>Second Floor UPS: <font color='green'>" + (String) getUPSBatteryPercentage(1) + "%</font></td>";
  html += "       </tr>";
  html += "     </table>";
  html += "   </center>";
  html += FOOTER;
  html += " </body>";
  html += "</html>";
  server.send(200, "text/html", html);
  server.client().stop();
}

float getUPSVoltage(int upsNumber) {

  if(upsNumber == 1) {
    int val = adc.analogRead(0);
    return stepUPS1 * val;
  } else {
    int val = adc.analogRead(1);
    return stepUPS2 * val;
  }
}

int getUPSBatteryPercentage(int upsNumber) {

  float voltage = getUPSVoltage(upsNumber);
  
  if(voltage < 10.2) {
    return 0;
  } else if(voltage > 12.5) {
    return 100;
  } else {
    return (voltage - 10.2) / (12.5 - 10.2) * 100;
  }
}
