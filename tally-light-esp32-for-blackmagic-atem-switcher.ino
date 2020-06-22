/*****************

  Tally light ESP32 for Blackmagic ATEM switcher

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
#include <ATEM.h>

// Some RGB color values, see http://www.barth-dev.de/online/rgb565-color-picker/
#define GRAY  0x0020 //   8   8   8
#define GREEN 0x0200 //   0  64   0
#define RED   0xF800 // 255   0   0

const char* ssid = "yournetwork";
const char* password =  "yourpassword";

int cameraNumber = 4;   // the camera this tally light is representing (1-4)
int ledPin = 10;        // The pin of the red LED

int PreviewTallyPrevious = 1;
int ProgramTallyPrevious = 1;

IPAddress ip(192, 168, 178, 170); // IP address of this ESP32

ATEM AtemSwitcher(IPAddress(192, 168, 178, 173), 56417); // IP address of the switcher

void setup() {

  Serial.begin(9600);

  WiFi.begin(ssid, password); // Connect to WiFi

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  M5.begin();   // initialize the M5StickC object

  pinMode(ledPin, OUTPUT);    // Set ledPin as digital output
  digitalWrite(ledPin, HIGH); // Turn the ledPin off (high)

  AtemSwitcher.serialOutput(0x80);
  AtemSwitcher.connect();     // Connect to the ATEM switcher
}

void loop() {

  AtemSwitcher.runLoop();   // Check for and respond to packets, keep the connection alive

  if (AtemSwitcher.isConnectionTimedOut())  {
    Serial.println("Connection to ATEM Switcher has timed out - reconnecting!\n");
    AtemSwitcher.connect();
  }

  int ProgramTally = AtemSwitcher.getProgramTally(cameraNumber);
  int PreviewTally = AtemSwitcher.getPreviewTally(cameraNumber);

  if ((ProgramTallyPrevious != ProgramTally) || (PreviewTallyPrevious != PreviewTally)) { // Has preview or program status changed?

    if ((ProgramTally && !PreviewTally) || (ProgramTally && PreviewTally) ) { // This camera is only program or both preview and program?
      drawLabel(RED, BLACK, LOW);
    } else if (PreviewTally && !ProgramTally) {                               // This camera is only preview, not program
      drawLabel(GREEN, BLACK, HIGH);
    } else if (!PreviewTally || !ProgramTally) {                              // This camera is not preview and not program
      drawLabel(BLACK, GRAY, HIGH);
    }

  }

  ProgramTallyPrevious = ProgramTally; // Store the last program status
  PreviewTallyPrevious = PreviewTally; // Store the last preview status
}

void drawLabel(unsigned long int screenColor, unsigned long int labelColor, bool ledValue) {
  digitalWrite(ledPin, ledValue);                     // Turn red LED on or off
  M5.Lcd.fillScreen(screenColor);                     // Fill the LCD with a color
  M5.Lcd.setTextColor(labelColor, screenColor);       // Set the text color
  M5.Lcd.drawString(String(cameraNumber), 15, 40, 8); // Display camera number
}
