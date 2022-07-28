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

#define C_PLUS 1 //CHANGE TO 1 IF YOU USE THE M5STICK-C PLUS


#if C_PLUS == 1
  #include <M5StickCPlus.h>
#else
  #include <M5StickC.h>
#endif
#include <WiFi.h>
#include <SkaarhojPgmspace.h>
#include <PinButton.h>
#include <ATEMbase.h>
#include <ATEMstd.h>
#include <WiFiManager.h>
#include <Preferences.h>


// http://www.barth-dev.de/online/rgb565-color-picker/
#define GREY  0x0020 //   8  8  8
#define GREEN 0x0200 //   0 64  0
#define RED   0xF800 // 255  0  0


PinButton btnM5(37);     //the "M5" button on the device
PinButton btnAction(39); //the "Action" button on the device


IPAddress switcherIp(192, 168, 1, 30);        // IP address of the ATEM switcher
char switcherIpString[15] = "192.168.1.30";   //IP address of the Tally Arbiter Server
int cameraNumber = 1;
char cameraNumberString[2] = "1";
const String camName = "m5StickC " + String(cameraNumber);
bool ledOn = false;
int ledPin = 10;

int PreviewTallyPrevious = 1;
int ProgramTallyPrevious = 1;

ATEMstd AtemSwitcher;
WiFiManager wm; // global wm instance

WiFiManagerParameter custom_server("hostIP", "Blackmagic Atem", switcherIpString, 15, switcherIpString);  
WiFiManagerParameter custom_cam_num("camNum", "Camera number(1-4)", cameraNumberString, 1, cameraNumberString);  
char customhtml[24] = "type=\"checkbox\"";
WiFiManagerParameter custom_led_on("ledOn", "Led on", "T", 2, customhtml, WFM_LABEL_AFTER);

Preferences preferences;

bool portalRunning = false;
int currentScreen = 0;      //0 = Tally Screen, 1 = Settings Screen
int currentBrightness = 11; //12 is Max level

void loadPreferences(){
  Serial.println("Reading preferences");
  preferences.begin("blackmagic-atem", false);
  
  if(preferences.getString("hostIP").length() > 0){
    String newHost = preferences.getString("hostIP");
    Serial.println("Setting Blackmagic Atem host as");
    Serial.println(newHost);
    strcpy(switcherIpString, newHost.c_str());
    custom_server.setValue(switcherIpString, 15);
  }
  if(preferences.getString("camNum").length() > 0){
    String newCamNum = preferences.getString("camNum");
    Serial.println("Setting camera number as");
    Serial.println(newCamNum);
    cameraNumber = newCamNum.toInt();
    strcpy(cameraNumberString, newCamNum.c_str());
    custom_cam_num.setValue(cameraNumberString, 1);
  }
  if(preferences.getString("ledOn").length() > 0){
    String newLedOn = preferences.getString("ledOn");
    Serial.println("Setting led on as");
    Serial.println(newLedOn);
    ledOn = newLedOn == "T";
    if (ledOn) {
      strcat(customhtml, " checked");
    }
  }
  preferences.end();
}


void setup() {
  
  Serial.begin(9600);

  // initialize the M5StickC object
  M5.begin();
  setCpuFrequencyMhz(80);    //Save battery by turning down the CPU clock
  btStop();                  //Save battery by turning off BlueTooth


  loadPreferences();

  
  pinMode(ledPin, OUTPUT);  // LED: 1 is on Program (Tally)
  digitalWrite(ledPin, HIGH); // off

  delay(100); //wait 100ms before moving on

  connectToNetwork(); //starts Wifi connection

  switcherIp.fromString(switcherIpString);
  // Initialize a connection to the switcher:
  AtemSwitcher.begin(switcherIp);
  AtemSwitcher.serialOutput(0x80);
  AtemSwitcher.connect();
}

void logger(String strLog, String strType) {
  if (strType == "info") {
    Serial.println(strLog);
    //M5.Lcd.println(strLog);
  }
  else {
    Serial.println(strLog);
  }
}

void connectToNetwork() {


  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  logger("Connecting to SSID: " + String(WiFi.SSID()), "info");


  
  wm.addParameter(&custom_server);
  wm.addParameter(&custom_cam_num);
  wm.addParameter(&custom_led_on);
  wm.setSaveParamsCallback(saveParamCallback);

  // custom menu via array or vector
  std::vector<const char *> menu = {"wifi","param","info","sep","restart","exit"};
  wm.setMenu(menu);
  // set dark theme
  wm.setClass("invert");
  wm.setConfigPortalTimeout(120); // auto close configportal after n seconds

  bool res;

  res = wm.autoConnect(camName.c_str()); // AP name for setup

  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  }
  else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yay :)");
  }
}


void loop() {
  if(portalRunning){
    wm.process();
  }

  btnM5.update();
  btnAction.update();

  if (btnM5.isClick()) {
    switch (currentScreen) {
      case 0:
        showSettings();
        currentScreen = 1;
        break;
      case 1:
        showTally();
        currentScreen = 0;
        break;
    }
  }

  if (btnAction.isClick()) {
    updateBrightness();
  }

  if(currentScreen == 0){
    evaluateMode();
  }

}

void saveParamCallback() {
  Serial.println("[CALLBACK] saveParamCallback fired");
  
  Serial.println("PARAM Switcher Blackmagic Atem Server = " + getParam("hostIP"));
  String str_hostIP = getParam("hostIP");
  Serial.println("Saving new Blackmagic Atem host");
  Serial.println(str_hostIP);

  Serial.println("PARAM Camera number = " + getParam("camNum"));
  String str_camNum = getParam("camNum");
  Serial.println("Saving new camera number");
  Serial.println(str_camNum);

  Serial.println("PARAM led active = " + getParam("ledOn"));
  String str_ledOn = getParam("ledOn");
  Serial.println("Saving new value of led on");
  Serial.println(str_ledOn);
  
  preferences.begin("blackmagic-atem", false);
  preferences.putString("hostIP", str_hostIP);
  preferences.putString("camNum", str_camNum);
  if(str_ledOn == "T")
    preferences.putString("ledOn", "T");
  else
    preferences.putString("ledOn", "F");
  
  preferences.end();

  ESP.restart();

}

String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void updateBrightness() {
  if(currentBrightness >= 12) {
    currentBrightness = 7;
  } else {
    currentBrightness++;
  }
  M5.Axp.ScreenBreath(currentBrightness);
}

void evaluateMode() {
  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  int ProgramTally = AtemSwitcher.getProgramTally(cameraNumber);
  int PreviewTally = AtemSwitcher.getPreviewTally(cameraNumber);

  if ((ProgramTallyPrevious != ProgramTally) || (PreviewTallyPrevious != PreviewTally)) { // changed?

    if ((ProgramTally && !PreviewTally) || (ProgramTally && PreviewTally) ) { // only program, or program AND preview
      drawLabel(RED, BLACK, LOW);
    } else if (PreviewTally && !ProgramTally) { // only preview
      drawLabel(GREEN, BLACK, HIGH);
    } else if (!PreviewTally || !ProgramTally) { // neither
      drawLabel(BLACK, GREY, HIGH);
    }

  }

  ProgramTallyPrevious = ProgramTally;
  PreviewTallyPrevious = PreviewTally;
  
}

void showTally() {
  if(portalRunning) {
    wm.stopWebPortal();
    portalRunning = false;
  }

  PreviewTallyPrevious = 1;
  ProgramTallyPrevious = 1;
  
  M5.Lcd.setTextColor(GREY, BLACK);
  M5.Lcd.fillScreen(TFT_BLACK);
  //M5.Lcd.println(DeviceName);

  //displays the currently assigned device and tally data
  evaluateMode();
}

void drawLabel(unsigned long int screenColor, unsigned long int labelColor, bool ledValue) {
  if (ledOn) {
    digitalWrite(ledPin, ledValue);
  }
  M5.Lcd.fillScreen(screenColor);
  M5.Lcd.setTextColor(labelColor, screenColor);
  M5.Lcd.setRotation(0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString(String(cameraNumber), 15, 40, 8);
}

void showSettings() {
  wm.startWebPortal();
  portalRunning = true;
  digitalWrite(ledPin, HIGH);
  
  //displays the current network connection and Tally Arbiter server data
  M5.Lcd.setCursor(0, 10);
  M5.Lcd.fillScreen(TFT_BLACK);
  //M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.println("SSID: " + String(WiFi.SSID()));
  M5.Lcd.println(WiFi.localIP());
  M5.Lcd.println();
  M5.Lcd.println("Blackmagic Atem:");
  M5.Lcd.println(String(switcherIpString));
  M5.Lcd.println();
  M5.Lcd.print("Battery: ");
  int batteryLevel = floor(100.0 * (((M5.Axp.GetVbatData() * 1.1 / 1000) - 3.0) / (4.07 - 3.0)));
  batteryLevel = batteryLevel > 100 ? 100 : batteryLevel;
  if(batteryLevel >= 100){
    M5.Lcd.println("Charging...");   // show when M5 is plugged in
  } else {
    M5.Lcd.println(String(batteryLevel) + "%");
  }
}
