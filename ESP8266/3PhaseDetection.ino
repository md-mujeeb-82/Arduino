#include <MCP3XXX.h>

MCP3008 adc;

int phase1, phase2, phase3;
int prev1Ph1, prev1Ph2, prev1Ph3;
int prev2Ph1, prev2Ph2, prev2Ph3;

long lasttime = -1;
long lasttimeGap = -1;

bool phase1Trigger = false;
bool phase2Trigger = false;
bool phase3Trigger = false;
bool stopTrigger = false;
bool gapTrigger = true;

void setup() {
  Serial.begin(9600);
  adc.begin();

  prev1Ph1 = prev2Ph1 = adc.analogRead(0);
  prev1Ph2 = prev2Ph2 = adc.analogRead(1);
  prev1Ph3 = prev2Ph3 = adc.analogRead(2);
  lasttime = millis();
  lasttimeGap = millis();
}

void loop(){

  gapTrigger = millis() - lasttimeGap > 20;
  
  phase1 = adc.analogRead(0);
  phase2 = adc.analogRead(1);
  phase3 = adc.analogRead(2);

//  if(stopTrigger) {
//   Serial.print("Phs1: "); Serial.print(phase1);
//   Serial.print(", Phs2: "); Serial.print(phase2);
//   Serial.print(", Phs3: "); Serial.println(phase3);
//  }

  if(prev1Ph1 < prev2Ph1 && prev2Ph1 > phase1 && !phase1Trigger && !phase2Trigger && !phase3Trigger && gapTrigger && stopTrigger) {
    Serial.println("");
    Serial.print("Phase 1 Edge, ");
    Serial.println(millis());
    
    phase1Trigger = false;
    phase2Trigger = true;
    phase3Trigger = true;
  }

  if(prev1Ph2 < prev2Ph2 && prev2Ph2 > phase2 && phase2Trigger && gapTrigger  && stopTrigger) {
    Serial.print("Phase 2 Edge, ");
    Serial.println(millis());
    phase2Trigger = false;

    Serial.print("Phase 3 Edge, ");
    Serial.println(millis());
    phase3Trigger = false;

    lasttimeGap = millis();
  }

  if(prev1Ph3 < prev2Ph3 && prev2Ph3 > phase3 && phase3Trigger && gapTrigger  && stopTrigger) {
    Serial.print("Phase 3 Edge, ");
    Serial.println(millis());
    phase3Trigger = false;

    Serial.print("Phase 2 Edge, ");
    Serial.println(millis());
    phase2Trigger = false;

    lasttimeGap = millis();
  }

//  Serial.print("Phase_1:");
//  Serial.print(prev1Ph1);
//  Serial.print(",");
//  Serial.print("Phase_2:");
//  Serial.print(prev2Ph1);
//  Serial.print(",");
//  Serial.print("Phase_3:");
//  Serial.println(phase1);

//  if(stopTrigger && prev1Ph1 != prev2Ph1 && prev2Ph1 != phase1) {
//    if(prev1Ph1 < prev2Ph1 && prev2Ph1 > phase1) {
//      Serial.println("Phase 1");
//    }
//  
//    if(prev1Ph2 < prev2Ph2 && prev2Ph2 > phase2) {
//      Serial.println("Phase 2");
//    }
//  
//    if(prev1Ph3 < prev2Ph3 && prev2Ph3 > phase3) {
//      Serial.println("Phase 3");
//    }
//  }

  prev1Ph1 = prev2Ph1;
  prev2Ph1 = phase1;
  prev1Ph2 = prev2Ph2;
  prev2Ph2 = phase2;
  prev1Ph3 = prev2Ph3;
  prev2Ph3 = phase3;

  if(millis() - lasttime > 1000) {
    stopTrigger = true;
  }

  if(millis() - lasttime > 2000) {
    stopTrigger = false;
  }
}
