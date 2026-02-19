#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include "fauxmoESP.h"

#define IDENTIFIER 7867
#define STATION_SSID "Network"
#define STATION_PSK  "bg$786786"
#define AP_SSID "WaterLevelController"
#define AP_PSK  "a1b2c3d4e5"

#define SUMP_INDICATOR_LED 14
#define SUMP_EMPTY_SENSOR  12
#define PIN_LIFT           13
#define DATA_REFRESH_DELAY 3000 // 3 Seconds

long lastTimeSensorRead = -1;

// The Data Structure
struct StoredData {
  int identifier;
  boolean isLiftOn;
  boolean isLiftOnOverride;
  boolean isLiftOffOverride;
} data;

// Set web server port number to 81
ESP8266WebServer server(81);

WiFiClient wifi;
PubSubClient mqtt(wifi);

fauxmoESP fauxmo;

const char BROKER[] = "192.168.68.173";
int        PORT     = 1883;
const char TOPIC_STATE_SUMP_EMPTY[]  = "ahmads/waterLevel/sumpEmpty";
const char TOPIC_STATE_LIFT[]  = "ahmads/liftSwitch/state";
const char TOPIC_COMMAND_LIFT[]  = "ahmads/liftSwitch/command";
const char TOPIC_STATE_LIFT_ON_OVERRIDE[]  = "ahmads/liftOnOverride/state";
const char TOPIC_COMMAND_LIFT_ON_OVERRIDE[]  = "ahmads/liftOnOverride/command";
const char TOPIC_STATE_LIFT_OFF_OVERRIDE[]  = "ahmads/liftOffOverride/state";
const char TOPIC_COMMAND_LIFT_OFF_OVERRIDE[]  = "ahmads/liftOffOverride/command";

static const String FOOTER = String("<br/><br/><br/><br/><br/><br/><br/>\n")
                           + String("<font face=\"Arial\" size=\"2\">\n")
                           + String("<table width=\"100%\" bgcolor=\"lightgray\" style=\"color: black;\">\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("  <tr>\n")
                           + String("    <td colspan=\"2\" align=\"center\">&#169; Copyright 2023 Quicksync Lab - All Rights Reserved</td>\n")
                           + String("  </tr>\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("  <tr>\n")
                           + String("    <td colspan=\"2\" align=\"center\">&nbsp;<a href=\"https://quicksynclab.com\">Quicksync Lab</a></td>\n")
                           + String("  <tr>\n")
                           + String("  <tr><td>&nbsp;</td></tr>\n")
                           + String("  <tr>\n")
                           + String("    <td align=\"left\"><a href=\"mailto:quicksynclab@gmail.com\">quicksynclab@gmail.com</a></td>\n")
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

//  Serial.begin(9600);
//  pinMode(LED, OUTPUT);
  pinMode(PIN_LIFT, OUTPUT);
  pinMode(SUMP_INDICATOR_LED, OUTPUT);
  pinMode(SUMP_EMPTY_SENSOR, INPUT);

  // Boot time state of Lift - ON
  digitalWrite(PIN_LIFT, HIGH);
  digitalWrite(SUMP_INDICATOR_LED, HIGH);

  EEPROM.begin(sizeof(StoredData));

  // Restore stored data from EEPROM
  restoreEEPROMData();

  // Invalid data, store the data for the first time
  if(data.identifier != IDENTIFIER) {
    data.identifier = IDENTIFIER;
    data.isLiftOn = false;
    data.isLiftOnOverride = false;
    data.isLiftOffOverride = false;
    storeEEPROMData();
  }

  // Set the Lift state to stored previous value
  if(data.isLiftOn) {
    digitalWrite(PIN_LIFT, HIGH);
  } else {
    digitalWrite(PIN_LIFT, LOW);
  }

//  Serial.printf("Trying to connect to WiFi");
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(STATION_SSID, STATION_PSK);
  
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);                         // wait for sometime
  }

//  Serial.printf("Connected to WiFi...");

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]0
  ArduinoOTA.setHostname(AP_SSID);

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
  server.on("/dashboard", handleDashboard);
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
  fauxmo.addDevice("Lift");

  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        
    if(state) {
      digitalWrite(PIN_LIFT, HIGH);
      data.isLiftOn = true;
    }else {
      digitalWrite(PIN_LIFT, LOW);
      data.isLiftOn = false;
    }
    
    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_LIFT, data.isLiftOn ? "1" : "0");
 });
}

void loop(){
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

  if(digitalRead(SUMP_EMPTY_SENSOR) == 0) {
    digitalWrite(SUMP_INDICATOR_LED, LOW);
  } else {
    digitalWrite(SUMP_INDICATOR_LED, HIGH);
  }

  long currentTime = millis();
  if (currentTime - lastTimeSensorRead > DATA_REFRESH_DELAY || lastTimeSensorRead == -1) {
      if(digitalRead(SUMP_EMPTY_SENSOR) == HIGH) {
        mqtt.publish(TOPIC_STATE_SUMP_EMPTY, "No");
      } else {
        mqtt.publish(TOPIC_STATE_SUMP_EMPTY, "Yes");
      }
    
      mqtt.publish(TOPIC_STATE_LIFT, data.isLiftOn ? "1" : "0");
      mqtt.publish(TOPIC_STATE_LIFT_ON_OVERRIDE, data.isLiftOnOverride ? "1" : "0");
      mqtt.publish(TOPIC_STATE_LIFT_OFF_OVERRIDE, data.isLiftOffOverride ? "1" : "0");
      lastTimeSensorRead = currentTime;
  }
}

void reconnect() {
  // Reconnect and publish
  if (mqtt.connect(AP_SSID, "mqtt", "Mummy@123")) {
    mqtt.publish(TOPIC_STATE_SUMP_EMPTY, isSumpEmpty());
    mqtt.publish(TOPIC_STATE_LIFT, data.isLiftOn ? "1" : "0");
    mqtt.publish(TOPIC_STATE_LIFT_ON_OVERRIDE, data.isLiftOnOverride ? "1" : "0");
    mqtt.publish(TOPIC_STATE_LIFT_OFF_OVERRIDE, data.isLiftOffOverride ? "1" : "0");
    mqtt.subscribe(TOPIC_COMMAND_LIFT);
    mqtt.subscribe(TOPIC_COMMAND_LIFT_ON_OVERRIDE);
    mqtt.subscribe(TOPIC_COMMAND_LIFT_OFF_OVERRIDE);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  String strTopic = String(topic);
  if(strTopic == String(TOPIC_COMMAND_LIFT)) {
    if((char)payload[0] == '1') {
      if(!data.isLiftOffOverride) {
         digitalWrite(PIN_LIFT, HIGH);
         data.isLiftOn = true;
         storeEEPROMData();
      }
    } else {
        if(!data.isLiftOnOverride) {
          digitalWrite(PIN_LIFT, LOW);
          data.isLiftOn = false;
          storeEEPROMData();
        }
    }
    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_LIFT, data.isLiftOn ? "1" : "0");
  }

  if(strTopic == String(TOPIC_COMMAND_LIFT_ON_OVERRIDE)) {
    if((char)payload[0] == '1') {
      data.isLiftOnOverride = true;
    } else {
      data.isLiftOnOverride = false;
    }
    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_LIFT_ON_OVERRIDE, data.isLiftOnOverride ? "1" : "0");
  }

  if(strTopic == String(TOPIC_COMMAND_LIFT_OFF_OVERRIDE)) {
    if((char)payload[0] == '1') {
      data.isLiftOffOverride = true;
    } else {
      data.isLiftOffOverride = false;
    }
    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_LIFT_OFF_OVERRIDE, data.isLiftOffOverride ? "1" : "0");
  }
}

void handleRoot() {

  String lift = server.arg("lift");

  if(lift != NULL && lift.equals("toggle")) {

    // Toggle the Motor Switch
    if(data.isLiftOn) {
      digitalWrite(PIN_LIFT, LOW);
      data.isLiftOn = false;
    } else {
      digitalWrite(PIN_LIFT, HIGH);
      data.isLiftOn = true;
    }
    
    storeEEPROMData();
    mqtt.publish(TOPIC_STATE_LIFT, data.isLiftOn ? "1" : "0");
  }
  
  String html = String("<html>")
        + String(" <head>")
        + String("   <title>Water Level Controller</title>")
        + STYLE
        + String("   <script language='JavaScript'>")
        + String("     function submitForm() {")
        + String("       var xhttp = new XMLHttpRequest();")
        + String("       xhttp.onreadystatechange = function() {};")
        + String("       xhttp.open(\"GET\", \"http://" + WiFi.localIP().toString() + "?lift=toggle\", true);")
        + String("       xhttp.send();")
        + String("     }")
        + String("   </script>")
        + String(" </head>")
        + String(" <body>")
        + String("   <center>")
        + String("     <br>")
        + String("     <h1>Water Level & Lift Controller</h1>")
        + String("     <br>")
        + String("     <table border='0' cellpadding='5' cellspacing='0'>")
        + String("       <tr>")
        + String("         <td align='center'>Sump is Empty: <font color='blue'>") + isSumpEmpty() + String("</font></td>")
        + String("       </tr>")
        + String("     </table>")
        + String("    <br><br><br>")
        + String("    <table cellpadding=0 cellspacing=5>")
        + String("      <tr>")
        + String("        <td align='cemter'>&nbsp;&nbsp;&nbsp;Lift</td>")
        + String("      </tr>")
        + String("      <tr>")
        + String("        <td align='cemter'><label class=\"switch\"><input type=\"checkbox\" " + (String)(data.isLiftOn ? "checked" : "") + " onChange=\"submitForm()\"><span class=\"slider round\"></span></label></td>")
        + String("      </tr>")
        + String("    </table>")
        + String("   </center>")
        + String(" </body>")
        + FOOTER
        + String("</html>");
        
  server.send(200, "text/html", html);
  server.client().stop();
}

void handleDashboard() {
  String html = "<html>";
  html += "  <html>\n";
  html += "    <head><title>Ahmads IoT Dashboard</title>\n";
  html += "      <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  html += "    </head>\n";
  html += "    <body>\n";
  html += "      <br>\n";
  html += "      <center>\n";
  html += "        <font face='Arial'>\n";
  html += "          <h1>Ahmads IoT Dashboard</h1>\n";
  html += "          <br><br>\n";
  html += "          <table width='80%' cellspacing='20' cellpadding='20'>\n";
  html += "            <tr>\n";
  html += "              <td align='center'><a href='http://192.168.68.108'>Aquarium Controller</a></td>\n";
  html += "              <td align='center'><a href='http://192.168.68.114:81'>Water Level & Lift Controller</a></td>\n";
  html += "            </tr>\n";
  html += "            <tr>\n";
/*  html += "              <td align='center'><a href='http://192.168.68.112'>Power Monitor</a></td>\n"; */
  html += "              <td colspan='2' align='center'><a href='http://192.168.68.129'>Terrace LED Controller</a></td>\n";
  html += "            </tr>\n";
/*  html += "            <tr>\n";
  html += "              <td colspan='2' align='center'><a href='http://192.168.68.100'>Rain Monitor</a></td>\n";
  html += "            </tr>\n";*/
  html += "          </table>\n";
  
  html += "        </font>\n";
  html += "      </center>\n";
  html += FOOTER;
  html += "    </body>\n";
  html += "  </html>\n";
  server.send(200, "text/html", html);
  server.client().stop();
}

const char* isSumpEmpty() {
  
  return digitalRead(SUMP_EMPTY_SENSOR) == HIGH ? "No" : "Yes";
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
