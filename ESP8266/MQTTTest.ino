#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

#define SERIAL_NUMBER "6858929474"

#define STATION_SSID "Ahmads"
#define STATION_PSK  "Xg#609590"
#define AP_SSID "MQTTTest"
#define AP_PSK  "a1b2c3d4e5"

#define DATA_REFRESH_DELAY 10000 // 10 Seconds

long lastTimeSensorRead = -1;

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
PubSubClient mqtt(wifi);

const char BROKER[] = "103.127.146.137";
int        PORT     = 1883;
String TOPIC_TANK1  = "waterLevelController/" + String(SERIAL_NUMBER) + "/tank1";
String TOPIC_TANK2  = "waterLevelController/" + String(SERIAL_NUMBER) + "/tank2";
String TOPIC_SUMP1  = "waterLevelController/" + String(SERIAL_NUMBER) + "/sump1";
String TOPIC_SUMP2  = "waterLevelController/" + String(SERIAL_NUMBER) + "/sump2";
String TOPIC_AUTO_STATUS    = "waterLevelController/" + String(SERIAL_NUMBER) + "/auto/status";
String TOPIC_AUTO_CONTROL   = "waterLevelController/" + String(SERIAL_NUMBER) + "/auto/control";
String TOPIC_PUMP1_STATUS   = "waterLevelController/" + String(SERIAL_NUMBER) + "/pump1/status";
String TOPIC_PUMP1_CONTROL  = "waterLevelController/" + String(SERIAL_NUMBER) + "/pump1/control";
String TOPIC_PUMP2_STATUS   = "waterLevelController/" + String(SERIAL_NUMBER) + "/pump2/status";
String TOPIC_PUMP2_CONTROL  = "waterLevelController/" + String(SERIAL_NUMBER) + "/pump2/control";
String TOPIC_PUMP3_STATUS   = "waterLevelController/" + String(SERIAL_NUMBER) + "/pump3/status";
String TOPIC_PUMP3_CONTROL  = "waterLevelController/" + String(SERIAL_NUMBER) + "/pump3/control";

bool isAutoOn = false;
bool isPump1On = false;
bool isPump2On = false;
bool isPump3On = false;

void setup() {
  Serial.begin(9600);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(STATION_SSID, STATION_PSK);
  
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);                         // wait for sometime
  }

  Serial.printf("Connected to WiFi...");

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]0
  ArduinoOTA.setHostname(AP_SSID);

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
  server.begin();
  
  mqtt.setServer(BROKER, PORT);
  mqtt.setCallback(callback);
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

  int tank1Level = random(101);
  int tank2Level = random(101);
  int sump1Level = random(101);
  int sump2Level = random(101);

  

  long currentTime = millis();
  if (currentTime - lastTimeSensorRead > DATA_REFRESH_DELAY || lastTimeSensorRead == -1) {
      
      mqtt.publish(TOPIC_TANK1.c_str(), String(tank1Level).c_str());
      mqtt.publish(TOPIC_TANK2.c_str(), String(tank2Level).c_str());
      mqtt.publish(TOPIC_SUMP1.c_str(), String(sump1Level).c_str());
      mqtt.publish(TOPIC_SUMP2.c_str(), String(sump2Level).c_str());
      lastTimeSensorRead = currentTime;
  }
}

void reconnect() {
  // Reconnect and publish
  if (mqtt.connect(AP_SSID, "mqtt", "Mummy@123")) {
    mqtt.subscribe(TOPIC_AUTO_STATUS.c_str());
    mqtt.subscribe(TOPIC_PUMP1_STATUS.c_str());
    mqtt.subscribe(TOPIC_PUMP2_STATUS.c_str());
    mqtt.subscribe(TOPIC_PUMP3_STATUS.c_str());

    mqtt.subscribe(TOPIC_AUTO_CONTROL.c_str());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  String strTopic = String(topic);

  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.printf("\n");
  
  if(strTopic == String(TOPIC_AUTO_STATUS)) {
    if((char)payload[0] == '1') {
      isAutoOn = true;
    } else {
      isAutoOn = false;
    }
    Serial.println("Auto is now " + isAutoOn);
  }

  if(strTopic == String(TOPIC_PUMP1_STATUS)) {
    if((char)payload[0] == '1') {
      isPump1On = true;
    } else {
      isPump1On = false;
    }
    Serial.println("Pump1 is now " + isPump1On);
  }

  if(strTopic == String(TOPIC_PUMP2_STATUS)) {
    if((char)payload[0] == '1') {
      isPump2On = true;
    } else {
      isPump2On = false;
    }
    Serial.println("Pump2 is now " + isPump2On);
  }

  if(strTopic == String(TOPIC_PUMP3_STATUS)) {
    if((char)payload[0] == '1') {
      isPump3On = true;
    } else {
      isPump3On = false;
    }
    Serial.println("Pump3 is now " + isPump3On);
  }

  if(strTopic == String(TOPIC_AUTO_CONTROL)) {
    if((char)payload[0] == '1') {
      isAutoOn = true;
    } else {
      isAutoOn = false;
    }
    mqtt.publish(TOPIC_AUTO_STATUS.c_str(), isAutoOn ? "1" : "0");
  }
}

void printHex(uint8_t num) {
  char hexCar[2];

//  sprintf(hexCar, "%02X", num);
//  Serial.print(hexCar);
   Serial.print((char)num);
}

void handleRoot() {

  String html = String("<html>")
        + String(" <head>")
        + String("   <title>Water Level Controller</title>")
        + String("     <script language='JavaScript'>")
        + String("       function submitForm(value) {")
        + String("         document.getElementById('lift').value = 'toggle';")
        + String("         document.forms[0].submit();")
        + String("       }")
        + String("     </script>")
        + String(" </head>")
        + String(" <body>")
        + String("   <center>")
        + String("         <br>")
        + String("       <h1>Water Level & Lift Controller</h1>")
        + String("     <br>")
        + String("     <table border='1' cellpadding='5' cellspacing='0'>")
        + String("       <tr>")
        + String("         <td align='center'>Sump is Empty: <font color='blue'>") + true + String("</font></td>")
        + String("       </tr>")
        + String("       <tr>")
        + String("         <td align='center'>Lift: <font color='") + (true ? "green'>On" : "red'>Off") + String("</font></td>")
        + String("       </tr>")
        + String("       <tr>")
        + String("             <form method='GET'>")
        + String("                <td colspan='2' align='center'><button id='lift' name='lift' onclick='submitForm()'>") + (true ? "Turn Off" : "Turn On") + String("</button></td>")
        + String("             </form>")
        + String("       </tr>")
        + String("     </table>")
        + String("   </center>")
        + String(" </body>")
        + String("</html>");
        
  server.send(200, "text/html", html);
  server.client().stop();
}
