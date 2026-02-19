#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"
#include "audio.h"

AudioGeneratorMP3 *mp3;
AudioFileSourcePROGMEM *file;
AudioOutputI2SNoDAC *out;
AudioFileSourceID3 *id3;


// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
//  (void)cbData;
//  Serial.printf("ID3 callback for: %s = '", type);

//  if (isUnicode) {
//    string += 2;
//  }
//  
//  while (*string) {
//    char a = *(string++);
//    if (isUnicode) {
//      string++;
//    }
//    Serial.printf("%c", a);
//  }
//  Serial.printf("'\n");
//  Serial.flush();
}

int playCount = 0;

void setup()
{
  WiFi.mode(WIFI_OFF); 
//  Serial.begin(115200);
  delay(1000);
//  Serial.printf("Sample MP3 playback begins...\n");

//  audioLogger = &Serial;
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();
}

void loop()
{
  if(mp3->isRunning() && mp3->loop()) {
  } else {
    if(playCount > 0) {
      mp3->stop();
    }
    playAudio();
  }
}

void playAudio() {
  switch(playCount) {
    case 0:
            file = new AudioFileSourcePROGMEM(beepData, sizeof(beepData));
            playCount++;
            break;
    case 1:
            file = new AudioFileSourcePROGMEM(beep100CompletedData, sizeof(beep100CompletedData));
            playCount++;
            break;
    case 2:
            file = new AudioFileSourcePROGMEM(beepCompletedData, sizeof(beepCompletedData));
            playCount++;
            break;
    case 3:
            file = new AudioFileSourcePROGMEM(speechData, sizeof(speechData));
            playCount++;
            break;
    case 4:
            file = new AudioFileSourcePROGMEM(speech100CompletedData, sizeof(speech100CompletedData));
            playCount++;
            break;
    case 5:
            file = new AudioFileSourcePROGMEM(speechCompletedData, sizeof(speechCompletedData));
            playCount++;
            break;
    default:
            file = new AudioFileSourcePROGMEM(beepData, sizeof(beepData));
            playCount = 1;
            delay(2000);
  }
  id3 = new AudioFileSourceID3(file);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  mp3->begin(id3, out);
}
