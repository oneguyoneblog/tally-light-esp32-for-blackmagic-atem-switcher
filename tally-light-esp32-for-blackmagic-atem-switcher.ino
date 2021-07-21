/*****************
  Tally light ESP32 for Blackmagic ATEM switcher

  Version 2.0

  A wireless (WiFi) tally light for Blackmagic Design
  ATEM video switchers, based on the M5StickC ESP32 development
  board and the Arduino IDE.

  For more information, see:
  https://oneguyoneblog.com/2020/06/13/tally-light-esp32-for-blackmagic-atem-switcher/

  Based on the work of Kasper Skårhøj:
  https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering

******************/

#include <M5StickC.h>
#include <WiFi.h>
#include <SkaarhojPgmspace.h>
#include <ATEMbase.h>
#include <ATEMstd.h>

IPAddress clientIp(192, 168, 178, 170);        	// IP address of the ESP32
IPAddress switcherIp(192, 168, 178, 173);	      // IP address of the ATEM switcher
ATEMstd AtemSwitcher;

// http://www.barth-dev.de/online/rgb565-color-picker/
#define GRAY  0x0020 //   8  8  8
#define GREEN 0x0200 //   0 64  0
#define RED   0xF800 // 255  0  0

const char* ssid = "yournetwork";
const char* password =  "yourpassword";

int inputNumber = 1; // The lowest/first camera input
int lowCameraNumber = inputNumber;
int highCameraNumber = 8; // The highest input with a cameras
int cameraOffset = 0; // The number of inputs on your ATEM before the first camera. Eg. We label Input 3 as CAM1.
int ledPin = 10;

int PreviewTallyPrevious = 1;
int ProgramTallyPrevious = 1;

void setup() {

  Serial.begin(9600);

  // Start the Ethernet, Serial (debugging) and UDP:
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  // initialize the M5StickC object
  M5.begin();
  pinMode(ledPin, OUTPUT);  // LED: 1 is on Program (Tally)
  digitalWrite(ledPin, HIGH); // off

  // Initialize a connection to the switcher:
  AtemSwitcher.begin(switcherIp);
  AtemSwitcher.serialOutput(0x80);
  AtemSwitcher.connect();
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()){
    inputNumber = inputNumber + 1;
    if (inputNumber > highCameraNumber) {
      inputNumber = lowCameraNumber;
    }
    drawLabel(BLACK, GRAY, HIGH, inputNumber);
    delay(500);
  }
  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  int ProgramTally = AtemSwitcher.getProgramTally(inputNumber);
  int PreviewTally = AtemSwitcher.getPreviewTally(inputNumber);

  if ((ProgramTallyPrevious != ProgramTally) || (PreviewTallyPrevious != PreviewTally)) { // changed?

    if ((ProgramTally && !PreviewTally) || (ProgramTally && PreviewTally) ) { // only program, or program AND preview
      drawLabel(RED, BLACK, LOW, inputNumber);
    } else if (PreviewTally && !ProgramTally) { // only preview
      drawLabel(GREEN, BLACK, HIGH, inputNumber);
    } else if (!PreviewTally || !ProgramTally) { // neither
      drawLabel(BLACK, GRAY, HIGH, inputNumber);
    }

  }

  ProgramTallyPrevious = ProgramTally;
  PreviewTallyPrevious = PreviewTally;
}

void drawLabel(unsigned long int screenColor, unsigned long int labelColor, bool ledValue, int inputNumber) {
  digitalWrite(ledPin, ledValue);
  M5.Lcd.fillScreen(screenColor);
  M5.Lcd.setTextColor(labelColor, screenColor);
  M5.Lcd.drawString(String(cameraNumber - cameraOffset), 15, 40, 8);
}