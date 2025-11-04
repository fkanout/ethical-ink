#include <Arduino.h>
#include <WiFi.h>
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// WiFi credentials
const char* ssid = "hdad-2";
const char* password = "0933253463";

// Audio components
AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

// Athan URL
const char* athanURL = "http://www.mp3quran.net/api/adhan_madinah.mp3";

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("ESP32-S3 MAX98357A Audio - Custom Pins");
    Serial.println("BCLK=17, LRC=18, DIN=21");
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    // Setup I2S output with YOUR custom pins
    out = new AudioOutputI2S();
    out->SetPinout(17, 18, 21);  // BCLK=17, LRC=18, DIN=21
    out->SetOutputModeMono(true);
    out->SetGain(0.5);  // Volume: 0.0 to 4.0
    
    // Create audio stream
    Serial.println("Connecting to audio stream...");
    file = new AudioFileSourceHTTPStream(athanURL);
    
    // Buffer for smooth playback
    buff = new AudioFileSourceBuffer(file, 4096);
    
    // Create MP3 decoder
    mp3 = new AudioGeneratorMP3();
    
    Serial.println("Starting playback...");
    mp3->begin(buff, out);
}

void loop() {
    if (mp3->isRunning()) {
        if (!mp3->loop()) {
            mp3->stop();
            Serial.println("Audio finished");
        }
    } else {
        Serial.println("Audio stopped");
        delay(1000);
    }
}