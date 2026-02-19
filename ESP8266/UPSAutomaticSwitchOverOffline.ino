#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

#define IDENTIFIER 7867
#define HOSTNAME "UPSSwitchOver"
#define DEVICE_TITLE "UPS Automatic Switch Over"
#define AC_SAMPLING_DELAY         20    // 20 Milliseconds
#define REBOOT_DELAY              300000

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

const int   RELAY_PIN = 0;
const int   AC_VOLTAGE_SENSE_PIN = 2;

// Set web server port number to 81
ESP8266WebServer server(80);

WiFiClient wifi;
long lastTimeACRead = -1;
long lastTimeReboot = -1;

void setup() {

  // Configure pins as output pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(AC_VOLTAGE_SENSE_PIN, INPUT_PULLUP);
  
  // Initial state - OFF
  // digitalWrite(RELAY_PIN, HIGH);
  
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.softAP(HOSTNAME);
//  delay(500);

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

  long currentTime = millis();

  if (currentTime - lastTimeACRead > AC_SAMPLING_DELAY || lastTimeACRead == -1) {
      // Sense the AC Voltage and Actuate the Relay
      if(digitalRead(AC_VOLTAGE_SENSE_PIN) == LOW) {
         digitalWrite(RELAY_PIN, HIGH);
      } else {
         digitalWrite(RELAY_PIN, LOW);
      }
      lastTimeACRead = currentTime;
  }

  if (currentTime - lastTimeReboot > REBOOT_DELAY) {
      ESP.restart();
      lastTimeReboot = currentTime;
  }
}

void handleRoot() {

  bool switchState = digitalRead(RELAY_PIN) == LOW;
  String command = server.arg("sendCommand");

  if(command.charAt(0) == '0') {
    switchState = !switchState;
    
    if(switchState) {
      digitalWrite(RELAY_PIN, LOW);
    } else {
      digitalWrite(RELAY_PIN, HIGH);
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
  html += "      <table cellpadding=0 cellspacing=5>";
  html += "        <tr>";
  html += "          <td align='center'>Switch</td>";
  html += "        </tr>";
  html += "        <tr>";
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
