#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String Allah99Names[99] = {
  "Ar-Rahmaan",
  "Ar-Raheem",
  "Al-Malik",
  "Al-Quddoos",
  "As-Salaam",
  "Al-Mu'min",
  "Al-Muhaymin",
  "Al-'Azeez",
  "Al-Jabbaar",
  "Al-Mutakabbir",
  "Al-Khaaliq",
  "Al-Baari'",
  "Al-Musawwir",
  "Al-Ghaffaar",
  "Al-Qahhaar",
  "Al-Wahhaab",
  "Ar-Razzaaq",
  "Al-Fattaah",
  "Al-Aleem",
  "Al-Qabid",
  "Al-Basit",
  "Al-Khafid",
  "Ar-Rafi'",
  "Al-Mu'izz",
  "Al-Mu'zill",
  "As-Sami'",
  "Al-Basir",
  "Al-Hakam",
  "Al-Adl",
  "Al-Lateef",
  "Al-Khabeer",
  "Al-Haleem",
  "Al-Azeem",
  "Al-Ghafoor",
  "Ash-Shakoor",
  "Al-'Ali",
  "Al-Kabeer",
  "Al-Hafeez",
  "Al-Muqeet",
  "Al-Haseeb",
  "Al-Jaleel",
  "Al-Kareem",
  "Ar-Raqeeb",
  "Al-Mujeeb",
  "Al-Wasi'",
  "Al-Hakeem",
  "Al-Wadood",
  "Al-Majeed",
  "Al-Ba'ith",
  "As-Shaheed",
  "Al-Haqq",
  "Al-Wakeel",
  "Al-Qawiyy",
  "Al-Mateen",
  "Al-Waliyy",
  "Al-Hameed",
  "Al-Muhsi",
  "Al-Mubdi",
  "Al-Mu'id",
  "Al-Muh'yi",
  "Al-Mumeet",
  "Al-Hayyu",
  "Al-Qayyum",
  "Al-Wajid",
  "Al-Majid",
  "Al-Wahid",
  "Al-Ahad",
  "As-Samad",
  "Al-Qadir",
  "Al-Muqtadir",
  "Al-Muqaddim",
  "Al-Mu'akhkhar"
  "Al-Awwal",
  "Al-Akhir",
  "Az-Zahir",
  "Al-Batin",
  "Al-Wali",
  "Al-Muta'ali",
  "Al-Barr",
  "At-Tawwaab",
  "Al-Muntaqim",
  "Al-'Afuw",
  "Ar-Ra'uf",
  "Al Malik al-Mulk",
  "Al Dhul-Jalal wal-Ikram",
  "Al-Muqsit",
  "Al-Jami'",
  "Al-Ghani",
  "Al-Mughni",
  "Al-Mani'",
  "Ad-Dharr",
  "An-Nafi'",
  "An-Noor",
  "Al-Hadi",
  "Al-Badi'",
  "Al-Baqi",
  "Al-Waris",
  "Ar-Rasheed",
  "As-Saboor"
};

char buffer[25];
int currentNameIndex = 0;
long lastTime = -1;

int PIN_BUTTON = 32;
int PIN_SDA = 19;
int PIN_SCL = 21;

void updateDisplay() {
  display.clearDisplay();
  
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setCursor(0, 0);
  String str = String(100);
  Allah99Names[currentNameIndex].toCharArray(buffer, sizeof(buffer));
  display.write(buffer);

  display.setTextSize(1);
  display.setCursor(3,56);
  str = String(700);
  str.toCharArray(buffer, 5);
  display.write(buffer);

  display.setCursor(34,56);
  display.write(false ? "WiFi:ON" : "WiFi:OFF");

  display.setCursor(86,56);
  display.write(true ? "Vbr:ON" : "Vbr:OFF");

  display.display();
}

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Wire.begin(PIN_SDA, PIN_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  updateDisplay();
  lastTime = millis();
}

void loop() {
  if(digitalRead(PIN_BUTTON) == LOW) {
    while(digitalRead(PIN_BUTTON) == LOW) {
      // Do Nothing
    }
  }

  long currentTime = millis();

  if(currentTime - lastTime > 1000) {
    lastTime = currentTime;
    currentNameIndex++;
    if(currentNameIndex >= 99) {
      currentNameIndex = 0;
    }
    updateDisplay();
  }
}
