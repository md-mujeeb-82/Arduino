//int pushButton = 2;

void setup() {
  Serial.begin(115200);
  //pinMode(pushButton, INPUT);
}

void loop() {
  if(Serial.available() <= 0) {
    return;
  }

  String value = Serial.readString();
  Serial.print(value);
}
