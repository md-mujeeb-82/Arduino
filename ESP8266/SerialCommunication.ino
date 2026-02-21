int pushButton = 0;
int vibrator = LED_BUILTIN;

void setup() {
  Serial.begin(115200);
  pinMode(pushButton, INPUT_PULLUP);
  pinMode(vibrator, OUTPUT);
  digitalWrite(vibrator, HIGH);
}

void loop() {
  if(digitalRead(pushButton) == 0) {
    Serial.write(0);
    delay(200);
  }

  if(Serial.available()) {
    int command1 = Serial.read(); // delay
    int command2 = Serial.read(); // number of times

    if(command2 <= 1) {
      digitalWrite(vibrator, LOW);
      delay(command1);
      digitalWrite(vibrator, HIGH);
    } else {
      for(int i=0; i<command2; i++) {
        digitalWrite(vibrator, LOW);
        delay(command1);
        digitalWrite(vibrator, HIGH);
        delay(100);
      }
    }
  }
}
