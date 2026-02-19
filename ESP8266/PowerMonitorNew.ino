#include <stdlib.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <MCP3XXX.h>

#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)
#define OFFSET_VOLTAGE ADC_COUNTS >> 1;
#define READVCC_CALIBRATION_CONST 1126400L

#ifndef STASSID
#define STASSID   "Ahmads"
#define STAPSK    "Xg#609590"
#define HOSTNAME  "PowerMonitor"
#define POWER_CHECK_INTERVAL 2000  // 5 Seconds
#define SAMPLING_WINDOW      30    // Milli Seconds
#endif

const char* ssid                  = STASSID;
const char* password              = STAPSK;
const float stepUPS1              = 0.024759322113618; // 1 Step
const float stepUPS2              = 0.0249188892455211; // 1 Step
const double AC_CYCLES            = 50;
const double AC_MES_TIMEOUT       = 2000;
const double VOLTAGE_CALIBRATOR_1 = 523.56;
const double VOLTAGE_CALIBRATOR_2 = 523.56;
const double VOLTAGE_CALIBRATOR_3 = 523.56;
const double PHASE_CALIBRATOR_1   = 1.7;
const double PHASE_CALIBRATOR_2   = 1.7;
const double PHASE_CALIBRATOR_3   = 1.7;

int    sampleV1, sampleV2, sampleV3;                   //sample holds the raw analog read value
double lastFilteredV1, lastFilteredV2, lastFilteredV3;
double filteredV1, filteredV2, filteredV3; ;           //Filtered is the raw analog value minus the DC offset
double offsetV1, offsetV2, offsetV3;                   //Low-pass filter output
double phaseShiftedV1, phaseShiftedV2, phaseShiftedV3; //Holds the calibrated phase shifted voltage.
double sqV1, sqV2, sqV3;
double sumV1, sumV2, sumV3;                            //sq = squared, sum = Sum
int    startV1, startV2, startV3;                      //Instantaneous voltage at start of sample window.
boolean lastVCross1, lastVCross2, lastVCross3;
boolean checkVCross1, checkVCross2, checkVCross3;      //Used to measure number of times threshold is crossed.
double  Vrms1, Vrms2, Vrms3;
    
char strVoltage[10];
long previousTimePowerCheck;

// Set web server port number to 80
ESP8266WebServer server(80);

WiFiClient wifi;
PubSubClient mqtt(wifi);

MCP3008 adc;

const char BROKER[] = "192.168.68.173";
int        PORT     = 1883;
const char TOPIC_STATE_VOLTAGE_PHASE1[]  = "ahmads/acVoltage/phase1";
const char TOPIC_STATE_VOLTAGE_PHASE2[]  = "ahmads/acVoltage/phase2";
const char TOPIC_STATE_VOLTAGE_PHASE3[]  = "ahmads/acVoltage/phase3";
const char TOPIC_STATE_UPS_1[]           = "ahmads/ups/1";
const char TOPIC_STATE_UPS_2[]           = "ahmads/ups/2";
const char TOPIC_POWER_STATUS[]          = "ahmads/powerStatus";

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
  Serial.begin(115200);
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

  Serial.println(adc.analogRead(2));
//  Serial.println(adc.analogRead(3));
//  Serial.println(adc.analogRead(4));

  

//  analogVoltage(1);  // Measure the AC voltage. Arguments = (# of AC cycles, timeout)
//  analogVoltage(2);  // Measure the AC voltage. Arguments = (# of AC cycles, timeout)
//  analogVoltage(3);  // Measure the AC voltage. Arguments = (# of AC cycles, timeout)
//  Serial.print(adc.analogRead(2));
//  Serial.print(' ');
//  Serial.print(adc.analogRead(3));
//  Serial.print(' ');
//  Serial.println(adc.analogRead(4));
  
//  int currentTime = millis();
//  
//  // Check if power is down
//  if(currentTime - previousTimePowerCheck > POWER_CHECK_INTERVAL) {
//    
//    dtostrf((getACPhaseVoltage(1)), 3, 0, strVoltage);
//    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE1, strVoltage);
//    dtostrf((getACPhaseVoltage(2)), 3, 0, strVoltage);
//    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE2, strVoltage);
//    dtostrf((getACPhaseVoltage(3)), 3, 0, strVoltage);
//    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE3, strVoltage);
//
//    dtostrf(getUPSBatteryPercentage(1), 3, 0, strVoltage);
//    mqtt.publish(TOPIC_STATE_UPS_1, strVoltage);
//    dtostrf(getUPSBatteryPercentage(2), 3, 0, strVoltage);
//    mqtt.publish(TOPIC_STATE_UPS_2, strVoltage);
//
//    mqtt.publish(TOPIC_POWER_STATUS, getPowerStatus());
//    previousTimePowerCheck = currentTime;
//
//    Serial.print("Phase 1:  ");
//    Serial.print(getACPhaseVoltage(1));
//    Serial.print(", Phase 2: ");
//    Serial.print(getACPhaseVoltage(2));
//    Serial.print(", Phase 3: ");
//    Serial.print(getACPhaseVoltage(3));
//  }
}

void reconnect() {
  // Reconnect and publish
  if (mqtt.connect(HOSTNAME, "mqtt", "Mummy@123")) {
    
    dtostrf((getACPhaseVoltage(1)), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE1, strVoltage);
    dtostrf((getACPhaseVoltage(2)), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE2, strVoltage);
    dtostrf((getACPhaseVoltage(3)), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_VOLTAGE_PHASE3, strVoltage);
  
    dtostrf(getUPSBatteryPercentage(1), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_UPS_1, strVoltage);
    dtostrf(getUPSBatteryPercentage(2), 3, 0, strVoltage);
    mqtt.publish(TOPIC_STATE_UPS_2, strVoltage);

    mqtt.publish(TOPIC_POWER_STATUS, getPowerStatus());
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
  html += "         <td colspan='3' align='center'>Power Status: <font color='blue'>" + (String) getPowerStatus() + "</font></td>";
  html += "       </tr>";
  html += "       <tr>";
  html += "         <td align='center'>Phase 1: <font color='blue'>" + (String) getACPhaseVoltage(1) + "V</font></td>";
  html += "         <td align='center'>Phase 2: <font color='blue'>" + (String) getACPhaseVoltage(2) + "V</font></td>";
  html += "         <td align='center'>Phase 3: <font color='blue'>" + (String) getACPhaseVoltage(3) + "V</font></td>";
  html += "       </tr>";
  html += "     </table>";
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

const char* getPowerStatus() {

    if(getACPhaseVoltage(1) < 100 || getACPhaseVoltage(2) < 100 || getACPhaseVoltage(3) < 100) {
       return "No";
    } else {
       return "Yes";
    }
}

float getACPhaseVoltage(int phaseNumber) {
  if(phaseNumber == 1) {

    return  Vrms1;
     
  } else if(phaseNumber == 2) {

    return 220; //Vrms2;
    
  } else {

    return 220; //Vrms3;
  }
}

float getUPSVoltage(int upsNumber) {

  if(upsNumber == 1) {
    int val = adc.analogRead(0);
    return 11; //stepUPS1 * val;
  } else {
    int val = adc.analogRead(1);
    return 12; //stepUPS2 * val;
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

//-------------------------------------------------------------------------------------------------
//  Voltage calculation from a window sample of the analog input from the Grove AC Voltage Sensor
//-------------------------------------------------------------------------------------------------
void analogVoltage(unsigned int phaseNumber)
{

  int cycles = AC_CYCLES/2; // Converting cycles to zero crossings

  int SupplyVoltage = boardVcc();

  unsigned int crossCount = 0;      // Used to measure number of times threshold is crossed.
  unsigned int numberOfSamples = 0; // This is now incremented

  //-------------------------------------------------------------------------------------------------------------------------
  // 1) Waits for the waveform to be close to 'zero' (mid-scale adc) part in sin curve.
  //-------------------------------------------------------------------------------------------------------------------------
  unsigned long start = millis(); // timer for the timeout.

  while (1) // wait for the sine signal to zero cross, break if timeout
  {
    if(phaseNumber == 1) {
      startV1 = adc.analogRead(phaseNumber+1); // using the voltage waveform
      if ((startV1 < (ADC_COUNTS * 0.51)) && (startV1 > (ADC_COUNTS * 0.49)))
        break; // check its within range to start from here
      if ((millis() - start) > AC_MES_TIMEOUT)
        break;
    } 
//    else if(phaseNumber == 2) {
//       startV2 = adc.analogRead(phaseNumber+1); // using the voltage waveform
//      if ((startV2 < (ADC_COUNTS * 0.51)) && (startV2 > (ADC_COUNTS * 0.49)))
//        break; // check its within range to start from here
//      if ((millis() - start) > AC_MES_TIMEOUT)
//        break;
//    } else {
//       startV3 = adc.analogRead(phaseNumber+1); // using the voltage waveform
//      if ((startV3 < (ADC_COUNTS * 0.51)) && (startV3 > (ADC_COUNTS * 0.49)))
//        break; // check its within range to start from here
//      if ((millis() - start) > AC_MES_TIMEOUT)
//        break;
//    }
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 2) Voltage measurement loop
  //-------------------------------------------------------------------------------------------------------------------------
  start = millis();

  while ((crossCount < cycles) && ((millis() - start) < AC_MES_TIMEOUT))
  {
    numberOfSamples++;         // Count number of times looped.

    if(phaseNumber == 1) {
        lastFilteredV1 = filteredV1; // Used for delay/phase compensation
    
        //-----------------------------------------------------------------------------
        // A) Read in raw voltage and current samples
        //-----------------------------------------------------------------------------
        sampleV1 = adc.analogRead(phaseNumber+1); // Read in raw voltage signal
    
        //-----------------------------------------------------------------------------
        // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
        //     then subtract this - signal is now centred on 0 counts.
        //-----------------------------------------------------------------------------
        offsetV1 = offsetV1 + ((sampleV1 - offsetV1) / ADC_COUNTS);
        filteredV1 = sampleV1 - offsetV1;
    
        //-----------------------------------------------------------------------------
        // C) Root-mean-square method voltage
        //-----------------------------------------------------------------------------
        sqV1 = filteredV1 * filteredV1; // 1) square voltage values
        sumV1 += sqV1;                 // 2) sum
    
        //-----------------------------------------------------------------------------
        // E) Phase calibration
        //-----------------------------------------------------------------------------
        phaseShiftedV1 = lastFilteredV1 + PHASE_CALIBRATOR_1 * (filteredV1 - lastFilteredV1);
    
        //-----------------------------------------------------------------------------
        // G) Find the number of times the voltage has crossed the initial voltage
        //    - every 2 crosses we will have sampled 1 wavelength
        //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
        //-----------------------------------------------------------------------------
        lastVCross1 = checkVCross1;
        if (sampleV1 > startV1)
          checkVCross1 = true;
        else
          checkVCross1 = false;
        if (numberOfSamples == 1)
          lastVCross1 = checkVCross1;
    
        if (lastVCross1 != checkVCross1)
          crossCount++;
    } 
//    else if(phaseNumber == 2) {
//         lastFilteredV2 = filteredV2; // Used for delay/phase compensation
//    
//        //-----------------------------------------------------------------------------
//        // A) Read in raw voltage and current samples
//        //-----------------------------------------------------------------------------
//        sampleV2 = adc.analogRead(phaseNumber+1); // Read in raw voltage signal
//    
//        //-----------------------------------------------------------------------------
//        // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
//        //     then subtract this - signal is now centred on 0 counts.
//        //-----------------------------------------------------------------------------
//        offsetV2 = offsetV2 + ((sampleV2 - offsetV2) / ADC_COUNTS);
//        filteredV2 = sampleV2 - offsetV2;
//    
//        //-----------------------------------------------------------------------------
//        // C) Root-mean-square method voltage
//        //-----------------------------------------------------------------------------
//        sqV2 = filteredV2 * filteredV2; // 1) square voltage values
//        sumV2 += sqV2;                 // 2) sum
//    
//        //-----------------------------------------------------------------------------
//        // E) Phase calibration
//        //-----------------------------------------------------------------------------
//        phaseShiftedV2 = lastFilteredV2 + PHASE_CALIBRATOR_2 * (filteredV2 - lastFilteredV2);
//    
//        //-----------------------------------------------------------------------------
//        // G) Find the number of times the voltage has crossed the initial voltage
//        //    - every 2 crosses we will have sampled 1 wavelength
//        //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
//        //-----------------------------------------------------------------------------
//        lastVCross2 = checkVCross2;
//        if (sampleV2 > startV2)
//          checkVCross2 = true;
//        else
//          checkVCross2 = false;
//        if (numberOfSamples == 1)
//          lastVCross2 = checkVCross2;
//    
//        if (lastVCross2 != checkVCross2)
//          crossCount++;
//    } else {
//         lastFilteredV3 = filteredV3; // Used for delay/phase compensation
//    
//        //-----------------------------------------------------------------------------
//        // A) Read in raw voltage and current samples
//        //-----------------------------------------------------------------------------
//        sampleV3 = adc.analogRead(phaseNumber+1); // Read in raw voltage signal
//    
//        //-----------------------------------------------------------------------------
//        // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
//        //     then subtract this - signal is now centred on 0 counts.
//        //-----------------------------------------------------------------------------
//        offsetV3 = offsetV3 + ((sampleV3 - offsetV3) / ADC_COUNTS);
//        filteredV3 = sampleV3 - offsetV3;
//    
//        //-----------------------------------------------------------------------------
//        // C) Root-mean-square method voltage
//        //-----------------------------------------------------------------------------
//        sqV3 = filteredV3 * filteredV3; // 1) square voltage values
//        sumV3 += sqV3;                 // 2) sum
//    
//        //-----------------------------------------------------------------------------
//        // E) Phase calibration
//        //-----------------------------------------------------------------------------
//        phaseShiftedV3 = lastFilteredV3 + PHASE_CALIBRATOR_3 * (filteredV3 - lastFilteredV3);
//    
//        //-----------------------------------------------------------------------------
//        // G) Find the number of times the voltage has crossed the initial voltage
//        //    - every 2 crosses we will have sampled 1 wavelength
//        //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
//        //-----------------------------------------------------------------------------
//        lastVCross3 = checkVCross3;
//        if (sampleV3 > startV3)
//          checkVCross3 = true;
//        else
//          checkVCross3 = false;
//        if (numberOfSamples == 1)
//          lastVCross3 = checkVCross3;
//    
//        if (lastVCross3 != checkVCross3)
//          crossCount++;
//    }
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Post loop calculations
  //-------------------------------------------------------------------------------------------------------------------------
  // Calculation of the root of the mean of the voltage and current squared (rms)
  // Calibration coefficients applied.

  if(phaseNumber == 1) {
    double V_RATIO = VOLTAGE_CALIBRATOR_1 * ((SupplyVoltage / 1000.0) / (ADC_COUNTS));
    Vrms1 = V_RATIO * sqrt(sumV1 / numberOfSamples);

    // Reset accumulators
    sumV1 = 0;
  }
//  else if(phaseNumber == 2) {
//    double V_RATIO = VOLTAGE_CALIBRATOR_2 * ((SupplyVoltage / 1000.0) / (ADC_COUNTS));
//    Vrms2 = V_RATIO * sqrt(sumV2 / numberOfSamples);
//
//    // Reset accumulators
//    sumV2 = 0;
//  } else {
//    double V_RATIO = VOLTAGE_CALIBRATOR_3 * ((SupplyVoltage / 1000.0) / (ADC_COUNTS));
//    Vrms3 = V_RATIO * sqrt(sumV3 / numberOfSamples);
//
//    // Reset accumulators
//    sumV3 = 0;
//  }
  
  //--------------------------------------------------------------------------------------
}

long boardVcc()
{

  long result;

  #if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined(__AVR_ATmega644__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_AT90USB1286__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    ADCSRB &= ~_BV(MUX5);                         // Without this the function always returns -1 on the ATmega2560
  #elif defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #endif
  
  #if defined(__AVR__)
    delay(2);                                     // Wait for the reference voltage to stabilize
    ADCSRA |= _BV(ADSC);                          // Convert
    while (bit_is_set(ADCSRA, ADSC))
      ;
    result = ADCL;
    result |= ADCH << 8;
    result = READVCC_CALIBRATION_CONST / result;  // 1100mV*1024 ADC steps
    return result;
  #elif defined(__arm__)
    return (3300);                                // Arduino Due
  #else
    return (3300);                                // Assuming other architectures works with 3.3V!
  #endif
}
