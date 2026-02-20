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
    Serial.print("TICK");
    delay(200);
  }

  if(Serial.available()) {
    String value = Serial.readString();
    int val = value.toInt();
    if(val == 0) {
      digitalWrite(vibrator, LOW);
      delay(100);
      digitalWrite(vibrator, HIGH);
    }
  }
}
