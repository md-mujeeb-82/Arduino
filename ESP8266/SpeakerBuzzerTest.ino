
#define SPEAKER_PIN 5

#define BUZZER_FREQUENCY          125
#define BUZZER_FREQUENCY_2        150

void setup() {
  pinMode(SPEAKER_PIN, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  for(int i=0; i<2; i++) {
    Serial.print("Frequency: ");
    Serial.println(i == 0 ? BUZZER_FREQUENCY : BUZZER_FREQUENCY_2);
    Serial.print("* ");
    blinkLED(20, 1, i == 0 ? BUZZER_FREQUENCY : BUZZER_FREQUENCY_2);
    delay(1400);
    Serial.print("* ");
    blinkLED(100, 1, i == 0 ? BUZZER_FREQUENCY : BUZZER_FREQUENCY_2);
    delay(1400);
    Serial.println("* ");
    blinkLED(1000, 1, i == 0 ? BUZZER_FREQUENCY : BUZZER_FREQUENCY_2);
    delay(1400);
    Serial.println("");
  }
  while(true);
}

void blinkLED(int millisecs, int count, int whichFrequency) {
  for(int i=0; i<count; i++) {
    changeBuzzerState(true, whichFrequency);
    delay(millisecs);
    changeBuzzerState(false, whichFrequency);
    if(i < count-1) {
      delay(millisecs);
    }
  }
}

void changeBuzzerState(bool state, int whichFrequency) {
  if(state) {
    analogWrite(SPEAKER_PIN, whichFrequency);
  } else {
    analogWrite(SPEAKER_PIN, 0);
  }
}
