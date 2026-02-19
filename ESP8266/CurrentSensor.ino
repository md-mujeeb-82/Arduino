
void setup()
{
  Serial.begin(115200);
}

void loop()
{ 
   float current = ((5.0/1024.)*analogRead(A0)-2.5)/357;
  
  delay(1000);
	Serial.println(current);
}
