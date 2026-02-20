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
    if(Serial.read() == 0) {
      digitalWrite(vibrator, LOW);
      delay(100);
      digitalWrite(vibrator, HIGH);
    }
  }
}
