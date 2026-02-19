/* =====================
   This software is licensed under the MIT License:
   https://github.com/spacehuhntech/esp8266_deauther
   ===================== */

extern "C" {
    // Please follow this tutorial:
    // https://github.com/spacehuhn/esp8266_deauther/wiki/Installation#compiling-using-arduino-ide
    // And be sure to have the right board selected
  #include "user_interface.h"
}

#include "EEPROMHelper.h"

#include "src/ArduinoJson-v5.13.5/ArduinoJson.h"
#if ARDUINOJSON_VERSION_MAJOR != 5
// The software was build using ArduinoJson v5.x
// version 6 is still in beta at the time of writing
// go to tools -> manage libraries, search for ArduinoJSON and install version 5
#error Please upgrade/downgrade ArduinoJSON library to version 5!
#endif // if ARDUINOJSON_VERSION_MAJOR != 5
#include "oui.h"
#include "language.h"
#include "functions.h"
#include "settings.h"
#include "Names.h"
#include "SSIDs.h"
#include "Scan.h"
#include "Attack.h"
#include "CLI.h"
#include "DisplayUI.h"
#include "A_config.h"
#include "repeater.h"

#include "led.h"

// Run-Time Variables //
Names names;
SSIDs ssids;
Accesspoints accesspoints;
Stations     stations;
Scan   scan;
Attack attack;
CLI    cli;
DisplayUI displayUI;

simplebutton::Button* resetButton;

#include "wifi.h"

uint32_t autosaveTime = 0;
uint32_t currentTime  = 0;
unsigned long resume_attack = 0;
bool update_attack = false;
bool booted = false;
int a = 0;
unsigned long indikator_kedip = 0;
int indikator_nyala = NYALA;
int delay_strobe = 100;
        
void setup() {
  String macc = WiFi.macAddress();
    // for random generator
    EEPROMHelper::begin(EEPROM_SIZE);
    #ifdef DEFAULT_ESP8266
    pinMode(STROBE,OUTPUT);
    digitalWrite(STROBE,MATI);
    #endif
    // start serial
    Serial.begin(115200);
    Serial.println();
    
    randomSeed(os_random());
  
    // start SPIFFS
    prnt(SETUP_MOUNT_SPIFFS);
    // bool spiffsError = !LittleFS.begin();
    LittleFS.begin();
    prntln(/*spiffsError ? SETUP_ERROR : */ SETUP_OK);
pinMode(D3, INPUT_PULLUP);
    // Start EEPROM
    

#ifdef FORMAT_SPIFFS
    prnt(SETUP_FORMAT_SPIFFS);
    //LittleFS.format();
    prntln(SETUP_OK);
#endif // ifdef FORMAT_SPIFFS

#ifdef FORMAT_EEPROM
    prnt(SETUP_FORMAT_EEPROM);
    EEPROMHelper::format(EEPROM_SIZE);
    prntln(SETUP_OK);
#endif // ifdef FORMAT_EEPROM

    // Format SPIFFS when in boot-loop
    if (/*spiffsError || */ !EEPROMHelper::checkBootNum(BOOT_COUNTER_ADDR)) {
        prnt(SETUP_FORMAT_SPIFFS);
        //LittleFS.format();
        prntln(SETUP_OK);

        prnt(SETUP_FORMAT_EEPROM);
        EEPROMHelper::format(0,299);
        EEPROMHelper::format(319,EEPROM_SIZE);
        prntln(SETUP_OK);

        EEPROMHelper::resetBootNum(BOOT_COUNTER_ADDR);
    }

    // get time
    currentTime = millis();
    if(EEPROM.read(400) == 1){
      repeater::update_status(true);
      repeater::run();
      return loop();
    }
    // load settings
    #ifndef RESET_SETTINGS
    settings::load();
    #else // ifndef RESET_SETTINGS
    settings::reset();
    settings::save();
    #endif // ifndef RESET_SETTINGS
    
    wifi::begin();
    wifi_set_promiscuous_rx_cb([](uint8_t* buf, uint16_t len) {
        scan.sniffer(buf, len);
    });

    // start display
    if (settings::getDisplaySettings().enabled) {
        displayUI.setup();
        displayUI.mode = DISPLAY_MODE::INTRO;
    }

    // load everything else
    names.load();
    ssids.load();
    cli.load();

    // create scan.json
    scan.setup();
    
    // dis/enable serial command interface
    if (settings::getCLISettings().enabled) {
        cli.enable();
    } else {
        prntln(SETUP_SERIAL_WARNING);
        Serial.flush();
        Serial.end();
    }

    // start access point/web interface
    if (settings::getWebSettings().enabled) wifi::startAP();

    // STARTED
    prntln(SETUP_STARTED);

    // version
    prntln(DEAUTHER_VERSION);

    // setup LED
    led::setup();
    #if defined(NODEMCU) 
    pinMode(D7,INPUT_PULLUP); 
    #endif
    // setup reset button
    resetButton = new ButtonPullup(RESET_BUTTON);
    cli.runCommand("set captivePortal \"false\"");
    
  wifi::mac_address(macc);
  String mac_str;
  int count_state = 0;
}
unsigned long kedip = 0;
int nyala = 0;
void loop() {
    currentTime = millis();
    
    if(attack.resume() == true){
      resume_attack = millis();
      attack.resume_state(false);
      update_attack = true;
    }
    if(millis() - resume_attack >= 15000){
      if(update_attack == true){
        update_attack = false;
        attack.start(false, true, false, false, true,settings::getAttackSettings().timeout * 1000);
      }
    }
    wifi::update();  // manage access point
 if (repeater::status() == true){
   digitalWrite(STROBE,LOW);
 }
 else {
    currentTime = millis();
    
    if(attack.resume() == true){
      resume_attack = millis();
      attack.resume_state(false);
      update_attack = true;
    }
    if(millis() - resume_attack >= 15000){
      if(update_attack == true){
        update_attack = false;
        attack.start(false, true, false, false, true,settings::getAttackSettings().timeout * 1000);
      }
    }
    wifi::update();  // manage access point
    led::update();   // update LED color
    attack.update(); // run attacks
    displayUI.update();
    cli.update();    // read and run serial input
    scan.update();   // run scan
    ssids.update();  // run random mode, if enabled
    //
    // auto-save
    if (settings::getAutosaveSettings().enabled
        && (currentTime - autosaveTime > settings::getAutosaveSettings().time)) {
        autosaveTime = currentTime;
        names.save(false);
        ssids.save(false);
        settings::save(false);
    }
    #if defined(NODEMCU)
    if(digitalRead(D7) == LOW){
    if(a == 0){
      if(!attack.isRunning()){
      a = 1;
      
      cli.runCommand("stopap");
      cli.runCommand("scan aps -c 30s");
      cli.runCommand("attack -da");
      
      displayUI.mode = DISPLAY_MODE::DEAUTH_ALL;
      }
    } else {
      if(attack.isRunning()){
      a = 0;
      scan.stop();
      attack.stop();
      
      displayUI.isPaused = false;
      displayUI.tempSSID.clear();
      displayUI.tempCount = 0;
      
      displayUI.mode = DISPLAY_MODE::MENU;
      
      }
    }
    delay(1000);
  } 
  #endif
    #if defined(DEFAULT_ESP8266)
    if(attack.isRunning() || scan.isScanning()){
      if(scan.deauths > 5){
        if(millis() - indikator_kedip >= 100){
        indikator_kedip = millis();
        if(indikator_nyala == NYALA)indikator_nyala = MATI;
        else indikator_nyala = NYALA;
        digitalWrite(STROBE,indikator_nyala);
      }
      }
      else {
        if(millis() - indikator_kedip >= 600){
        indikator_kedip = millis();
        if(indikator_nyala == NYALA)indikator_nyala = MATI;
        else indikator_nyala = NYALA;
        digitalWrite(STROBE,indikator_nyala);
      }
      }
    }
    else if(lock == true){
      if(millis() - indikator_kedip >= 100){
        indikator_kedip = millis();
        if(indikator_nyala == NYALA)indikator_nyala = MATI;
        else indikator_nyala = NYALA;
      }
      if(indikator_nyala == NYALA)analogWrite(STROBE,225);
      if(indikator_nyala == MATI)analogWrite(STROBE,255);
    }
    else {
      if(millis() - indikator_kedip >= delay_strobe){
        indikator_kedip = millis();
        if(indikator_nyala == NYALA){
          delay_strobe = 70;
          indikator_nyala = MATI;
        }
        else {
          delay_strobe = 5000;
          indikator_nyala = NYALA;
        }
        digitalWrite(STROBE,indikator_nyala);
    }
    }
    
    #endif
    if (!booted) {
        booted = true;
        EEPROMHelper::resetBootNum(BOOT_COUNTER_ADDR);
#ifdef HIGHLIGHT_LED
        displayUI.setupLED();
#endif // ifdef HIGHLIGHT_LED
    }

    resetButton->update();
    if (resetButton->holding(15000)) {
        led::setMode(LED_MODE::SCAN);
        DISPLAY_MODE _mode = displayUI.mode;
        displayUI.mode = DISPLAY_MODE::RESETTING;
        displayUI.update(true);

        settings::reset();
        settings::save(true);

        delay(2000);

        led::setMode(LED_MODE::IDLE);
        displayUI.mode = _mode;
    }
 }
    
    
 
  
}
