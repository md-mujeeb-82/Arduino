#include <stdlib.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>

#ifndef STASSID
#define STASSID   "Ahmads"
#define STAPSK    "Xg#609590"
#define HOSTNAME  "MasterDashboard"
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

void setup() {
  pinMode(2, OUTPUT);
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
}

void loop() {

  // Handle the OTA Functionality
  ArduinoOTA.handle();

  //Handling of incoming requests
  server.handleClient();

  digitalWrite(2, HIGH);
}

void handleRoot() {
  String html = "<html>";
  html += "  <html>\n";
  html += "    <head><title>Ahmads IoT Dashboard</title></head>\n";
  html += "    <body>\n";
  html += "      <br>\n";
  html += "      <center>\n";
  html += "        <font face='Arial'>\n";
  html += "          <h1>Ahmads IoT Dashboard</h1>\n";
  html += "          <br><br>\n";
  html += "          <table width='80%' cellspacing='20' cellpadding='20'>\n";
  html += "            <tr>\n";
  html += "              <td align='center'><a href='http://192.168.68.114'>Aquarium Controller</a></td>\n";
  html += "              <td align='center'><a href='http://192.168.68.118'>Water Level & Lift Controller</a></td>\n";
  html += "            </tr>\n";
  html += "            <tr>\n";
  html += "              <td align='center'><a href='http://192.168.68.112'>Power Monitor</a></td>\n";
  html += "              <td align='center'><a href='http://192.168.68.129'>Terrace LED Controller</a></td>\n";
  html += "            </tr>\n";
  html += "            <tr>\n";
  html += "              <td colspan='2' align='center'><a href='http://192.168.68.100'>Rain Monitor</a></td>\n";
  html += "            </tr>\n";
  html += "          </table>\n";
  html += "        </font>\n";
  html += "      </center>\n";
  html += FOOTER;
  html += "    </body>\n";
  html += "  </html>\n";
  server.send(200, "text/html", html);
  server.client().stop();
}
