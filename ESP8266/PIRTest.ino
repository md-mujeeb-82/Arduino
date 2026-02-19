#include <stdlib.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>

#ifndef STASSID
#define STASSID   "Ahmads"
#define STAPSK    "Xg#609590"
#define HOSTNAME  "PIRTest"
#define SENSOR_PIN 16
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

const char* ssid              = STASSID;
const char* password          = STAPSK;

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
PubSubClient mqtt(wifi);

const char BROKER[] = "192.168.68.173";
int        PORT     = 1883;
const char TOPIC_STATE_RAIN_STATUS[]  = "ahmads/pirTest";

bool previousState = false;

void setup() {
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

  pinMode(SENSOR_PIN, INPUT);

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

  bool currentState = digitalRead(SENSOR_PIN) == LOW;
  if(currentState != previousState) {
    mqtt.publish(TOPIC_STATE_RAIN_STATUS, currentState ? "1" : "0");
    previousState = currentState;
  }
}

void reconnect() {
  // Reconnect and publish
  mqtt.connect(HOSTNAME, "mqtt", "Mummy@123");
}

void handleRoot() {
  String html = "<html>";
  html += "  <head><title>Rain Monitor</title></head>";
  html += "   <body>";
  html += "     <center>";
  html += "     <br>";
  html += "       <h1>Rain Monitor</h1>";
  html += "     <br>";
  html += "      <table border='1' cellpadding='20' cellspacing='0'>";
  html += "       <tr>";
  html += "         <td align='center'>Rain Status: <font color='blue'>" + String(getRainStatus()) + "</font></td>";
  html += "       </tr>";
  html += "     </table>";
  html += "   </center>";
  html += FOOTER;
  html += " </body>";
  html += "</html>";
  server.send(200, "text/html", html);
  server.client().stop();
}

const char* getRainStatus() {

  pinMode(SENSOR_PIN, OUTPUT);
  digitalWrite(SENSOR_PIN, HIGH);
  pinMode(SENSOR_PIN, INPUT);
  return digitalRead(SENSOR_PIN) == HIGH ? "No" : "Yes";
}
