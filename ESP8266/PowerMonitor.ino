#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>

#define DEFAULT_MARKER 78670
#define DEFAULT_MOBILES String("919880506766")
#define SMS_API_KEY "V4cDXFYmKEdE"
// #define "marghoob.hasan@gmail.com"
#define DATA_PIN 2

ESP8266WebServer server(80);

// EEPROM Operations
struct Data {
  int marker;
  char recipientMobiles[100];
};

Data data;

#define WIFI_SSID "Ahmads"
#define WIFI_PASSWORD "03312018"

bool isPowerGone = false;
bool wasValue0 = false;
long lastTime = -1;

void handleRoot() {
  String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Power Monitor, by Mujeeb Mohammad</title>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <center>
        <h1>Power Monitor</h1>
        <h4>by Mujeeb Mohammad (+91 9880506766 / mohammad.mujeeb@gmail.com)</h4>
        <form action="/submit" method="GET">
          Recipient Mobile Numbers: <input type="text" name="recipientMobiles" value="%value%">
          <input type="submit" value="Change">
        </form>
      </center>
    </body>
    </html>
  )=====";
  html.replace("%value%", String(data.recipientMobiles));
  server.send(200, "text/html", html);
}

void handleSubmit() {
  String receivedValue = server.arg("recipientMobiles");
  // if(freq <= 0 || freq > 5000) {
  //   server.send(200, "text/plain", "Frequency " + receivedValue + " is Invalid. Please enter a value greater than 0 and less than or equal to 100.");
  //   return;
  // }

  receivedValue.toCharArray(data.recipientMobiles, receivedValue.length()+1);
  saveData();
  server.send(200, "text/plain", "Data was saved Successfully.");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) { message += " " + server.argName(i) + ": " + server.arg(i) + "\n"; }
  server.send(404, "text/plain", message);
}

void saveData() {
  EEPROM.put(0, data);
  EEPROM.commit();
}

void setup() {
  EEPROM.begin(sizeof(data));

  EEPROM.get(0, data);

  if(data.marker != DEFAULT_MARKER) {
    data.marker = DEFAULT_MARKER;
    DEFAULT_MOBILES.toCharArray(data.recipientMobiles, DEFAULT_MOBILES.length()+1);
    saveData();
  }

  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoConnect(true);
  WiFi.setSleep(false);
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Power Monitor");
  MDNS.begin("Power Monitor");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
  });
  
  ArduinoOTA.begin();

  // Web Server
  server.on("/", handleRoot);

  server.on("/submit", handleSubmit);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/gif", []() {
    static const uint8_t gif[] PROGMEM = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = millis() % 256;
    gif_colored[17] = millis() % 256;
    gif_colored[18] = millis() % 256;
    server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
  });

  server.onNotFound(handleNotFound);

  server.begin();

  pinMode(DATA_PIN, INPUT);
  lastTime = millis();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  MDNS.update();

  if(digitalRead(DATA_PIN) == 0) {
    wasValue0 = true;
  }

  long currentTime = millis();
  if(currentTime - lastTime > 30) {
    if(isPowerGone && wasValue0) {
      isPowerGone = false;
      Serial.print("Power Returned ");
      sendSMS(false);
    }
    
    if(!isPowerGone && !wasValue0){
      isPowerGone = true;
      Serial.print("Power Gone ");
      sendSMS(true);
    }

    wasValue0 = false;
    lastTime = currentTime;
  }
}

void sendSMS(bool isPowerGone) {
  WiFiClientSecure client; // Use WiFiClientSecure for HTTPS connections
  client.setInsecure();    // Skip certificate validation (not secure but works for development)
  HTTPClient http;
  http.begin(client, "https://www.circuitdigest.cloud/send_sms?ID=101"); // Specify the destination URL
  http.addHeader("Content-Type", "application/json"); // Set content type for form data
  http.addHeader("Authorization", SMS_API_KEY);

  String postData = String("{\n")
                    + String("\"mobiles\": \"") + data.recipientMobiles + String("\",\n")
                    + String("\"var1\": \"Power\",\n")
                    + String("\"var2\": \"" +  (isPowerGone ? String("Gone\"\n") : String("Came Back\"\n")))
                    + String("}");
  Serial.print(postData);

  int httpCode = http.POST(postData); // Send the POST request with the data

  if (httpCode > 0) { // Check for successful request
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      String payload = http.getString(); // Get the response from the server
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end(); // Close the connection
}
