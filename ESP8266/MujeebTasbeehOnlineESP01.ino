#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define IDENTIFIER 7867
#define STASSID "Ahmads"
#define STAPSK  "Xg#609590"
#define HOSTNAME "tasbeeh"

#define WIFI_CONNECTION_TIMEOUT   60000    // 1 Minute
#define TIMEOUT_RESET             1500     // 1.5 Seconds
#define PIN_LED                   0
#define PIN_VIBRATOR              1
#define PIN_BUZZER                2
#define PIN_BUTTON                3

ESP8266WebServer server(80);

long lastTimeWifiConnect = 0;
long lastTimeButtonPress = 0;
long lastTimeAutoTasbeehTick = 0;
long lastTimeBlink = 0;
bool isConfigMode = false;
bool isAutoPilotOn = false;
bool isESPNeedsToBeRestarted = false;

// The Data Structure
struct Data {
  int identifier;
  int count;
  int dayCount;
  int totalCount;
  int targetCount;
  int step;
  int tickDuration;
  int minimumTickDuration;
  bool isBuzzerOn;
  bool isVibratorOn;
  bool isAutoPilotMode;
} data;

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

void setup()
{
  pinMode(PIN_LED, OUTPUT);          // Status LED
  pinMode(PIN_VIBRATOR, OUTPUT);     // Vibrator
  pinMode(PIN_BUZZER, OUTPUT);       // Buzzer
  pinMode(PIN_BUTTON, INPUT_PULLUP); // Button

  digitalWrite(PIN_LED, LOW);        // Turn on the LED
  digitalWrite(PIN_VIBRATOR, HIGH);  // Turn off Vibrator
  digitalWrite(PIN_BUZZER, HIGH);    // Turn off the Buzzer

  if(digitalRead(PIN_BUTTON) == LOW) {
    isConfigMode = true;
  }

  EEPROM.begin(sizeof(Data));

  // Restore stored data from EEPROM
  restoreEEPROMData();
  
  // Invalid data, store the data for the first time
  if(data.identifier != IDENTIFIER) {
    setDataToDefaultValues();
    storeEEPROMData();
  }

  long currentTime = millis();

  if(isConfigMode) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(STASSID, STAPSK);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
    lastTimeWifiConnect = currentTime;
    while (WiFi.status() != WL_CONNECTED) {
  
      if(digitalRead(PIN_LED) == HIGH) {
        digitalWrite(PIN_LED, LOW);
      } else {
        digitalWrite(PIN_LED, HIGH);
      }
  
      // Unable to connect to WiFi, come out of the loop
      if (currentTime - lastTimeWifiConnect > WIFI_CONNECTION_TIMEOUT) {
          break;
      }
      delay(80);
    }
  
    // Port defaults to 8266
    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname(HOSTNAME);
  
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
    server.on("/getData", getData);
    server.on("/saveData", saveData);
    server.on("/restartDevice", restartDevice);
    server.on("/factoryResetDevice", factoryResetDevice);
    server.onNotFound(handleNotFound);
    server.begin();

    MDNS.begin(HOSTNAME);
    MDNS.addService("http", "tcp", 80);
  }

  digitalWrite(PIN_LED, LOW);
}

void loop() {
  
  long currentTime = millis();
  lastTimeButtonPress = currentTime;
  
  if(digitalRead(PIN_BUTTON) == LOW) {
    lastTimeButtonPress = currentTime;

    while(digitalRead(PIN_BUTTON) == LOW) {
      // Do Nothing
    }
  }

  currentTime = millis();

  if(currentTime - lastTimeButtonPress > TIMEOUT_RESET) {

    // Reset Count
    data.dayCount = data.dayCount + data.count;
    data.count = 0;
    storeEEPROMData();
    blinkLED(1000, true, 1);
      
  } else if(currentTime - lastTimeButtonPress > 50) {

    // Toggle AutoPilot
    if(data.isAutoPilotMode) {
      if(isAutoPilotOn) {
        isAutoPilotOn = false;
        // Blink fast 6 times
        blinkLED(20, true, 6);
      } else {
        isAutoPilotOn = true;
        // Blink fast 3 times
        blinkLED(20, true, 3);
        
      }
      lastTimeAutoTasbeehTick = currentTime;
      
    } else {
      incrementTasbeeh();
    }
  }

  // Gradually reduce tick time in 50 ticks, till it is 1 Second
  int autoPilotTickDelay = data.tickDuration;
  if(data.minimumTickDuration != data.tickDuration) {
    autoPilotTickDelay = autoPilotTickDelay - ((data.minimumTickDuration / 50) * data.count);
    if(autoPilotTickDelay < data.minimumTickDuration) {
      autoPilotTickDelay = data.minimumTickDuration;
    }
  }

  // AutoPilot Thread
  if(data.isAutoPilotMode && isAutoPilotOn && currentTime - lastTimeAutoTasbeehTick > autoPilotTickDelay) {

    incrementTasbeeh();
    lastTimeAutoTasbeehTick = currentTime;
  }

  if(isConfigMode) {
    // Restart ESP, if required
    if (isESPNeedsToBeRestarted) {
      delay(300);
      ESP.restart();
    }
  
    // Handle the OTA Functionality
    ArduinoOTA.handle();
  
    //Handling of incoming requests
    server.handleClient();
  }
}

void incrementTasbeeh() {
  if(data.count >= data.targetCount) {
    isAutoPilotOn = false;
    blinkLED(1000, true, 1);
    return;
  }
  data.count =  data.count + data.step;
  storeEEPROMData();
  if(data.count == data.targetCount) {
    isAutoPilotOn = false;
    tasbeehCompleteSound();
  } else if(data.count % 100 == 0){
    blinkLED(20, true, 2);
  } else {
    blinkLED(20, true, 1);
  }
}

void blinkLED(int millisecs, bool withSound, int count) {
  for(int i=0; i<count; i++) {
    digitalWrite(PIN_LED, HIGH);
    if(data.isVibratorOn) {
      digitalWrite(PIN_VIBRATOR, LOW);
    }
    if(withSound) {
      changeBuzzerState(true);
    }
    delay(millisecs);
    digitalWrite(PIN_LED, LOW);
    if(data.isVibratorOn) {
      digitalWrite(PIN_VIBRATOR, HIGH);
    }
    if(withSound) {
      changeBuzzerState(false);
    }
    if(i < count-1) {
      delay(millisecs);
    }
  }
}

void tasbeehCompleteSound() {
  for(int i=0; i<3; i++){
    changeBuzzerState(true);
    digitalWrite(PIN_LED, HIGH);
    if(data.isVibratorOn) {
      digitalWrite(PIN_VIBRATOR, LOW);
    }
    delay(20);
    changeBuzzerState(false);
    digitalWrite(PIN_LED, LOW);
    if(data.isVibratorOn) {
      digitalWrite(PIN_VIBRATOR, HIGH);
    }
    delay(20);
  }
  changeBuzzerState(true);
  digitalWrite(PIN_LED, HIGH);
  if(data.isVibratorOn) {
    digitalWrite(PIN_VIBRATOR, LOW);
  }
  delay(1000);
  changeBuzzerState(false);
  digitalWrite(PIN_LED, LOW);
  if(data.isVibratorOn) {
    digitalWrite(PIN_VIBRATOR, HIGH);
  }
}

void changeBuzzerState(bool state) {
  if(data.isBuzzerOn && state) {
    digitalWrite(PIN_BUZZER, LOW);
  } else {
    digitalWrite(PIN_BUZZER, HIGH);
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
  server.client().stop();
}

void handleRoot() {
  String step = server.arg("step");
  String tickDuration = server.arg("tickDuration");
  String minTickDuration = server.arg("minTickDuration");
  String sleep = server.arg("sleep");
  String total = server.arg("total");
  String daily = server.arg("day");
  String count = server.arg("count");
  String target = server.arg("target");
  String endSession = server.arg("endSession");
  String endDay = server.arg("endDay");
  String buzzer = server.arg("buzzer");
  String vibrator = server.arg("vibrator");
  String autoPilot = server.arg("autoPilot");
  if(step != NULL) {
    data.totalCount = total.toInt();
    data.dayCount = daily.toInt();
    data.targetCount = target.toInt();
    data.count = count.toInt();
    data.step = step.toInt();
    data.tickDuration = tickDuration.toInt();
    data.minimumTickDuration = minTickDuration.toInt();

    if(endSession.toInt() == 1) {
      data.dayCount = data.dayCount + data.count;
      data.count = 0;
    }

    if(endDay.toInt() == 1) {
      data.dayCount = data.dayCount + data.count;
      data.totalCount = data.totalCount + data.dayCount;
      data.count = 0;
      data.dayCount = 0;
    }

    if(buzzer.toInt() == 1) {
      data.isBuzzerOn = !data.isBuzzerOn;
    }

    if(vibrator.toInt() == 1) {
      data.isVibratorOn = !data.isVibratorOn;
    }

    if(autoPilot.toInt() == 1) {
      data.isAutoPilotMode = !data.isAutoPilotMode;
    }
    
    storeEEPROMData();
    blinkLED(100, true, 1);
  }

  String html = String("<html>\n")
              + String("  <head>\n")
              + String("    <title>Smart Tasbeeh</title>\n")
              + String("    <script language='JavaScript'>\n")
              + String("    function submitCommand(value) {\n")
              + String("      var message;\n")
              + String("      if(value == 1) {\n")
              + String("        message = \"Are you sure that you want to End the Day?\";\n")
              + String("      } else if(value == 3) {\n")
              + String("        message = \"Are you sure that you want to Factory Reset your Device?\";\n")
              + String("      } else if(value == 4) {\n")
              + String("        message = \"Are you sure that you want to Restart your Device?\";\n")
              + String("      }\n")
              + String("      if (value == 0 || value == 2 || value == 5 || value == 6 || value == 7 || value == 8 || confirm(message) == true) {\n")
              + String("        if(value == 1) {\n")
              + String("          document.forms[0].endDay.value = '1';\n")
              + String("          document.forms[0].submit();\n")
              + String("        } else if(value == 3) {\n")
              + String("          document.forms[0].action = \"/factoryResetDevice\";\n")
              + String("          document.forms[0].submit();\n")
              + String("        } else if(value == 4) {\n")
              + String("          document.forms[0].action = \"/restartDevice\";\n")
              + String("          document.forms[0].submit();\n")
              + String("        } else if(value == 5) {\n")
              + String("          document.forms[0].action = \"/uploadDataToServer\";\n")
              + String("          document.forms[0].submit();\n")
              + String("        } else if(value == 2) {\n")
              + String("          document.forms[0].buzzer.value = '1';\n")
              + String("          document.forms[0].submit();\n")
              + String("        } else if(value == 6) {\n")
              + String("          document.forms[0].endSession.value = '1';\n")
              + String("          document.forms[0].submit();\n")
              + String("        } else if(value == 7) {\n")
              + String("          document.forms[0].autoPilot.value = '1';\n")
              + String("          document.forms[0].submit();\n")
              + String("        } else if(value == 8) {\n")
              + String("          document.forms[0].vibrator.value = '1';\n")
              + String("          document.forms[0].submit();\n")
              + String("        } else {\n")
              + String("          document.forms[0].submit();\n")
              + String("        }\n")
              + String("      } else {\n")
              + String("        return false;\n")
              + String("      }\n")
              + String("     }\n")
              + String("    </script>\n")
              + String("  </head>\n")
              + String("  <body>\n")
              + String("    <center>\n")
              + String("      <br>\n")
              + String("      <h1><font face='Arial'>Smart Tasbeeh</font></h1>\n")
              + String("      <br>\n")
              + String("      <form method='GET'>\n")
              + String("        <table border='1' cellpadding='20' cellspacing='0'>\n")
              + String("          <tr>\n")
              + String("            <td align='center'><font face='Arial'>Total Count: <input name='total' id='total' size='5' maxlength='5' value='") + data.totalCount + String("'></font></td>\n")
              + String("            <td align='center'><font face='Arial'>Day Count: <input name='day' id='day' size='5' maxlength='5' value='") + data.dayCount + String("'></font></td>\n")
              + String("            <td align='center'><font face='Arial'>Target Count: <input name='target' id='target' size='5' maxlength='5' value='") + data.targetCount + String("'></font></td>\n")
              + String("            <td align='center'><font face='Arial'>Current Count: <input name='count' id='count' size='5' maxlength='5' value='") + data.count + String("'></font></td>\n")
              + String("          </tr>\n")
              + String("          <tr>\n")
              + String("            <td align='center'><font face='Arial'>Single Step: <input name='step' id='step' size='3' maxlength='3' value='") + data.step + String("'></font></td>\n")
              + String("            <td align='center'><font face='Arial'>Tick Duration: <input name='tickDuration' id='tickDuration' size='5' maxlength='5' value='") + data.tickDuration + String("'></font></td>\n")
              + String("            <td align='center' colspan='2'><font face='Arial'>Minimum Tick Duration: <input name='minTickDuration' id='minTickDuration' size='5' maxlength='5' value='") + data.minimumTickDuration + String("'></font></td>\n")
              + String("          </tr>\n")
              + String("          <tr>\n")
              + String("            <input type='hidden' name='endSession' id='endSession' value='0'>\n")
              + String("            <input type='hidden' name='endDay' id='endDay' value='0'>\n")
              + String("            <td colspan='4' align='center'><button onclick='return submitCommand(1)'>   End Day  </button>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<button onclick='return submitCommand(0)'>    Save    </button>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<button onclick='return submitCommand(6)'>    End Session    </button></td>\n")
              + String("          </tr>\n")
              + String("        </table>\n")
              + String("        <br><br>\n")
              + String("        <table border='1' cellpadding='20' cellspacing='0'>\n")
              + String("          <tr>\n")
              + String("            <input type='hidden' name='buzzer' id='buzzer' value='0'>\n")
              + String("            <input type='hidden' name='vibrator' id='vibrator' value='0'>\n")
              + String("            <input type='hidden' name='autoPilot' id='autoPilot' value='0'>\n")
              + String("            <td align='center'><font face='Arial'>Buzzer Status: <font color='" + (data.isBuzzerOn ? (String)"green'>On" : (String)"red'>Off") + (String)"</font></font><br><br><button onclick='return submitCommand(2)'>" + (data.isBuzzerOn ? (String)"Turn Off" : (String)"Turn On") + (String)"</button></td>\n")
              + String("            <td align='center'><font face='Arial'>Vibrator Status: <font color='" + (data.isVibratorOn ? (String)"green'>On" : (String)"red'>Off") + (String)"</font></font><br><br><button onclick='return submitCommand(8)'>" + (data.isVibratorOn ? (String)"Turn Off" : (String)"Turn On") + (String)"</button></td>\n")
              + String("            <td align='center'><font face='Arial'>Auto Pilot Mode: <font color='" + (data.isAutoPilotMode ? (String)"green'>On" : (String)"red'>Off") + (String)"</font></font><br><br><button onclick='return submitCommand(7)'>" + (data.isAutoPilotMode ? (String)"Turn Off" : (String)"Turn On") + (String)"</button></td>\n")
              + String("          </tr>\n")
              + String("        </table>\n")
              + String("        <br><br><br>\n")
              + String("        <table border='0' cellpadding='20' cellspacing='0'>\n")
              + String("          <tr>\n")
              + String("            <td align='center'><button onclick='return submitCommand(3)'>Factory Reset</button></td>\n")
              + String("            <td align='center'><button onclick='return submitCommand(4)'>Restart</button></td>\n")
              + String("          </tr>\n")
              + String("        </table>\n")
              + String("      </form>\n")
              + String("    </center>\n")
              + FOOTER
              + String("  </body>\n")
              + String("</html>\n");
        
  server.send(200, "text/html", html);
  server.client().stop();
}

void getData() {
  String plain = String("TASBEEH-DATA:")
              + String(data.count) + ':'
              + String(data.targetCount) + ':'
              + String(data.dayCount) + ':'
              + String(data.totalCount) + ':'
              + String(data.step) + ':'
              + String(data.tickDuration) + ':'
              + String(data.minimumTickDuration) + ':'
              + String(data.isBuzzerOn ? "true" : "false") + ':'
              + String(data.isVibratorOn ? "true" : "false") + ':'
              + String(data.isAutoPilotMode ? "true" : "false");
  server.send(200, "text/plain", plain);
  server.client().stop();
}

void saveData() {
  String count = server.arg("count");
  String target = server.arg("target");
  String daily = server.arg("day");
  String total = server.arg("total");
  String step = server.arg("step");
  String tickDuration = server.arg("tickDuration");
  String minTickDuration = server.arg("minTickDuration");
  String buzzer = server.arg("buzzer");
  String vibrator = server.arg("vibrator");
  String autoPilot = server.arg("autoPilot");

  bool wasDataChanged = false;
  
  if(count != NULL) {
    data.count = count.toInt();
    wasDataChanged = true;
  }

  if(target != NULL) {
    data.targetCount = target.toInt();
    wasDataChanged = true;
  }

  if(daily != NULL) {
    data.dayCount = daily.toInt();
    wasDataChanged = true;
  }

  if(total != NULL) {
    data.totalCount = total.toInt();
    wasDataChanged = true;
  }

  if(step != NULL) {
    data.step = step.toInt();
    wasDataChanged = true;
  }

  if(tickDuration != NULL) {
    data.tickDuration = tickDuration.toInt();
    wasDataChanged = true;
  }

  if(minTickDuration != NULL) {
    data.minimumTickDuration = minTickDuration.toInt();
    wasDataChanged = true;
  }

  if(buzzer != NULL) {
    data.isBuzzerOn = buzzer.toInt() == 1;
    wasDataChanged = true;
  }

  if(vibrator != NULL) {
    data.isVibratorOn = vibrator.toInt() == 1;
    wasDataChanged = true;
  }

  if(autoPilot != NULL) {
    data.isAutoPilotMode = autoPilot.toInt() == 1;
    wasDataChanged = true;
  }

  if(wasDataChanged) {
    storeEEPROMData();
    blinkLED(100, true, 1);
    server.send(200, "text/plain", "0");
  } else {
    server.send(200, "text/plain", "1");
  }
  
  server.client().stop();
}

void restartDevice() {
  server.send(200, "text/html", getMessagePageHTML("Operation Successful", "green", "Rebooting the Device..", "/"));
  server.client().stop();
  isESPNeedsToBeRestarted = true;
}

void factoryResetDevice() {
  server.send(200, "text/html", getMessagePageHTML("Operation Successful", "green", "Factory Reset complete, rebooting the Device..", "/"));
  server.client().stop();
  doFactoryResetAndRestart();
}

void setDataToDefaultValues() {
  data.identifier = IDENTIFIER;
  data.count = 0;
  data.dayCount = 0;
  data.totalCount = 0;
  data.targetCount = 200;
  data.step = 1;
  data.tickDuration = 2000;
  data.minimumTickDuration = 2000;
  data.isBuzzerOn = true;
  data.isVibratorOn = true;
  data.isAutoPilotMode = false;
}

void doFactoryResetAndRestart() {

   setDataToDefaultValues();
   storeEEPROMData();
   isESPNeedsToBeRestarted = true;
}

String getMessagePageHTML(String title, String textColor, String message, String backURL) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<html>";
  ptr += "  <head>";
  ptr += "    <title>";
  ptr += title;
  ptr += "    </title>";
  ptr += "  </head>";
  ptr += "  <body>";
  ptr += "    <center>";
  ptr += "      <font face=\"Arial\" color=\"";
  ptr += textColor;
  ptr += "\"><br>";
  ptr += "        <h1>";
  ptr += message;
  ptr += ".</h1>";
  ptr += "        <br>";
  ptr += "        <a href=\"";
  ptr += backURL;
  ptr += "\">Go Back</a>";
  ptr += "      </font>";
  ptr += "    </center>";
  ptr += "  </body>";
  ptr += "</html>";
  return ptr;
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
