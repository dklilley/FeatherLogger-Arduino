/*
 * This sketch is designed to run on an Adafruit M0 Bluefruit LE board.
 * 
 * The FeatherLogger is a flexible, general purpose data logger, designed to
 * be useful for many different projects. The FeatherLogger stores data in
 * files on an SD card, which are retrieved via Bluetooth LE by companion iOS
 * and Android apps.
 * 
 * Author: Duncan Lilley
 * Date: 10/18/16
 */

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "RTClib.h"

#include "BluefruitConfig.h"
#include "FeatherLoggerConfig.h"

/*=========================================================================
    APPLICATION SETTINGS

    FACTORYRESET_ENABLE       Perform a factory reset when running this sketch
   
                              Enabling this will put your Bluefruit LE module
                              in a 'known good' state and clear any config
                              data set in previous sketches or projects, so
                              running this at least once is a good idea.
   
                              When deploying your project, however, you will
                              want to disable factory reset by setting this
                              value to 0.  If you are making changes to your
                              Bluefruit LE device via AT commands, and those
                              changes aren't persisting across resets, this
                              is the reason why.  Factory reset will erase
                              the non-volatile memory where config data is
                              stored, setting it back to factory default
                              values.
       
                              Some sketches that require you to bond to a
                              central device (HID mouse, keyboard, etc.)
                              won't work at all with this feature enabled
                              since the factory reset will clear all of the
                              bonding data stored on the chip, meaning the
                              central device won't be able to reconnect.
    MINIMUM_FIRMWARE_VERSION  Minimum firmware version to have some new features
    MODE_LED_BEHAVIOUR        LED activity, valid options are
                              "DISABLE" or "MODE" or "BLEUART" or
                              "HWUART"  or "SPI"  or "MANUAL"
    -----------------------------------------------------------------------*/
    #define FACTORYRESET_ENABLE         true
    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
    #define MODE_LED_BEHAVIOUR          "MODE"
/*=========================================================================*/

// Create the Bluefruit object using hardware SPI
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// Declare RTC variables
RTC_PCF8523 rtc;

// Declare SD variables
const int chipSelect = 10;
Sd2Card card;
SdVolume volume;

const char* INFO_FILE = "info.txt";

/*------------------------------ DEBUG CONSTANTS ------------------------------*/
  // Enable this to set the RTC to the date & time this sketch is compiled
  #define RTC_SET               false 

  // Enable this to test the following components on init: SD Card, RTC
  #define TEST_COMPONENTS       false 

  // Enable this to print info as BLE input is received
  #define TEST_BLE              true

  // Enable this for USB debugging - Serial will wait until monitor is opened
  // Disable this for final product
  #define USB_DEBUG             false

/*-----------------------------------------------------------------------------*/

// The pin tied to the battery, for checking voltage
#define BATT_PIN                9

// Helper method for handling errors
void error(const __FlashStringHelper* err) {
  Serial.println(err);
  while(true); // Catch the program in a loop
}

// Temporary Test Logger Files
int sharpPin = A1;
int logLimitCt = 0;
File logFile;

void setup() {
  #if USB_DEBUG
    while(!Serial){}
  #else
    delay(100);
  #endif
  
  // Set up pin modes
  pinMode(BATT_PIN, INPUT);
  

  Serial.begin(115200);
  Serial.println(F("Adafruit Feather M0 Bluefruit LE -- FeatherLogger v0.1"));
  Serial.println(F("------------------------------------------------------"));

  // Initialize Bluefruit Module
  Serial.print(F("Initializing the Bluefruit LE module: "));
  if(!ble.begin(VERBOSE_MODE)) {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println(F("OK!"));
  
  #if FACTORYRESET_ENABLE
    // Perform a factory reset to make sure everything is in a known state
    Serial.print(F("Performing a factory reset: "));
    if(!ble.factoryReset()) {
      error(F("Couldn't factory reset"));
    } else {
      Serial.println(F("OK!"));
    }
  #endif

  // Disable command echo from Bluefruit
  ble.echo(false);

  // Print Bluefruit information
  Serial.println(F("Requesting Bluefruit Info:"));
  ble.info();

  // Turn off verbose debug info
  ble.verbose(false);

  Serial.println(F("******************************************************"));
  
  // Set module to DATA mode
  Serial.println(F("Switching to DATA mode"));
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************************************"));
  
  // Set up RTC card
  Serial.print(F("Setting up RTC: "));
  if(!rtc.begin()) {
    error(F("Couldn't find RTC"));
  }
  Serial.println("OK!");

  if(!rtc.initialized()) {
    Serial.println(F("Initializing RTC"));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  #if RTC_SET
    Serial.println(F("Readjusting RTC"));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  #endif

  Serial.println(F("******************************************************"));
  
  // Set up SD
  
  
  Serial.print(F("Initializing SD Card: "));
  //if(!card.init(SPI_HALF_SPEED, chipSelect)) {
  if(!SD.begin(chipSelect)) {
    error(F("Error - card not found"));
  }
  Serial.println("OK!");

  card.init(SPI_HALF_SPEED, chipSelect);
  volume.init(card);

  // Attempt to open the main volume
  /*Serial.print(F("Opening volume: "));
  if(!volume.init(card)) {
    error(F("Could not find FAT16/FAT32 partition."));
  }
  Serial.println(F("OK!"));*/

  // Open up root directory 
  //Serial.print(F("Opening root directory: "));
  //root = SD.open("", FILE_READ);
  //  Serial.println(F("Could not open root directory"));
  //Serial.println(root.name());
  //Serial.println(F("OK"));

  // Create info.dat if it doesn't exist
  if(!SD.exists(INFO_FILE)) {
    Serial.print(F("Creating info file: "));
    File infoFile = SD.open(INFO_FILE, FILE_WRITE);
    if(!infoFile) {
      error(F("Could not create info file"));
    }
    Serial.println(infoFile.name());
    // Date, Voltage, Storage Remaining, Total Storage
    
    String date = getCurrentDate();

    infoFile.print("startDate=");
    infoFile.println(date);

    infoFile.flush();
    infoFile.close();
  }

  #if TEST_COMPONENTS
    Serial.println(F("******************************************************"));
    testSDCard();
    Serial.println();
    testRTC();
  #endif
  
  Serial.println(F("******************************************************"));
  // Set up logger variables
  
  pinMode(sharpPin, INPUT);

  int status = openLogFile();
  if(status == 0) error(F("File not opened..."));
  else if(status == 1) Serial.println("File opened");
  else if(status == -1) Serial.println("File reopened");


  
  Serial.println();
}



void loop() {
  // Read sensor data and store on SD card
  logData();
  
  // Check for commands from BLE
  if(ble.isConnected()) { // If an app has connected to the FeatherLogger
    handleBleInput();
  }
}

/*
 * Logs data from the connected sensors into a file on the SD card
 */
void logData() {
  // Temporary method body, just hard code everything

  int sharpVal = analogRead(sharpPin);

  logFile.print("Sharp Sensor Value: ");
  logFile.println(sharpVal);
  logFile.print("Date: ");

  DateTime now = rtc.now();
  logFile.print(now.month());
  logFile.print("/");
  logFile.print(now.day());
  logFile.print("/");
  logFile.print(now.year());
  logFile.print("  ");
  logFile.print(now.hour());
  logFile.print(":");
  logFile.print(now.minute());
  logFile.print(":");
  logFile.println(now.second());

  logFile.flush();
  
  logLimitCt++;
  if(logLimitCt == 10) {
    logLimitCt = 0;
    logFile.close();
    int status = openLogFile();
    if(status == 0) error(F("File not opened..."));
    else if(status == 1) Serial.println("File opened");
    else if(status == -1) Serial.println("File reopened");
  }
  
  delay(2000);
}

int openLogFile() {
  String fileName = getLogFileName();
  int code = 1; // No error, file doesn't exist
  
  if(SD.exists(fileName)) code = -1; // No error, file already exists

  logFile = SD.open(fileName, FILE_WRITE);

  if(!logFile) return 0; // File not opened, return error

  return code;
}

String getLogFileName() {
  String fileName = "";
  
  DateTime now = rtc.now();

  int month = now.month();
  int day = now.day();
  int year = now.year();

  if(month < 10) fileName += "0";
  fileName += month;
  if(day < 10) fileName += "0";
  fileName += day;
  fileName += year;
  fileName += ".txt";

  return fileName;
}

/**
 * Checks for input from Bluetooth (from the app) and handles it depending
 * on the command received.
 */
void handleBleInput() {
  String data = "";

  // Get data sent from app
  while(ble.available()) {
    int c = ble.read();
    data += (char)c;
  }

  if(data != "") { // If input was received
    #if TEST_BLE
      Serial.println(F("----------------------------------------"));
      Serial.print(F("Data Received: "));
      Serial.println(data);
      Serial.println(F("----------------------------------------"));
    #endif
    
    if(data == "REQ+DOWNLOAD") {
      reqDownload();
    } else if(data == "REQ+DATA") {
      reqData();
    } else if(data.startsWith("REQ+FILE")) {
      reqFiles(data);
    } else {
      badCommand(data);
    }
    
  }
}

/*-------------------- BLE Response Handlers --------------------*/

void reqDownload() {
  // Send back a list of the files on the SD card, excluding the info.dat file
  String data = "%FILES%&";
  File root = SD.open("/");
  root.rewindDirectory();
  while(true) {
    File entry = root.openNextFile();

    if(!entry) { // No more files
      break;
    }

    String fileName = entry.name();

    if(fileName != INFO_FILE) {
      data += entry.name();
      data += '&';
    }
  }
  data += "%END%";

  ble.print(data);
  ble.flush(); //TODO
}

/**
 * App has requested for general information about the FeatherLogger
 */
void reqData() {
  // TODO: Currently just sending dummy data
  //ble.println(F("%INFO%&7/3/16&3.5v&125mb&256mb&%END%"));
  
  File infoFile = SD.open(INFO_FILE, FILE_READ);

  String fileDate = "";
  while(true) {
    int c = infoFile.read();
    if(c == -1 || c == '\n') break;
    fileDate += (char)c;
  }
  infoFile.close();

  String date = fileDate.substring(fileDate.indexOf('=') + 1);
  
  float voltage = getCurrentVoltage();
  uint32_t storageRemaining = getCurrentStorageKilobytes();
  uint32_t maxStorage = getMaxStorageKilobytes();

  String data = "%INFO%&";
  data += date; 
  data += '&';
  data += voltage; 
  data += '&';
  data += storageRemaining; 
  data += '&';
  data += maxStorage;
  data += "&%END%";

  ble.print(data);
} 

void reqFiles(String data) {
  // TODO: Currently supports just one file at a time
  int delim = data.indexOf('&');
  String file = data.substring(delim+1);
  
  sendFile(file);
}

// TODO: Others

/**
 * Called when an unknown command is received
 */
void badCommand(String data) {
  char cmd[data.length()];
  data.toCharArray(cmd, data.length()+1); // Convert command to *char

  ble.print(F("%BADCMD%&"));
  ble.print(cmd);
  ble.print(F("&%END%"));
}

/*---------------------------------------------------------------*/



#if TEST_COMPONENTS

void testSDCard() {
  Serial.println(F("---------- SD Test ----------"));
/*
  Sd2Card card;
  card.init(SPI_HALF_SPEED, chipSelect);
  
  SdVolume volume;
  volume.init(card);
*/

  // Print the type of SD card
  Serial.print(F("Card type: "));
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  // Print the type and size of the first FAT-type volume
  uint32_t volumeSize;
  Serial.print(F("Volume type is FAT"));
  Serial.println(volume.fatType(), DEC);

  volumeSize = volume.blocksPerCluster() * volume.clusterCount() * 512;
  volumeSize /= (1024*1024);
  Serial.print(F("Volume size (megabytes): "));
  Serial.println(volumeSize);

  // Print any files found on the card
  Serial.println(F("Files found on the card:"));

  // list all files in the card with date and size
  SdFile rootDir;
  rootDir.openRoot(volume);
  rootDir.ls(LS_R | LS_DATE | LS_SIZE);
}

void testRTC() {
  Serial.println(F("---------- RTC Test ----------"));

  DateTime now = rtc.now();

  Serial.println(F("Current Date and Time:"));
  Serial.print(F("\t"));

  Serial.print(now.month(), DEC);
  Serial.print(F("/"));
  Serial.print(now.day(), DEC);
  Serial.print(F("/"));
  Serial.println(now.year(), DEC);
  Serial.print(F("\t"));
  Serial.print(now.hour(), DEC);
  Serial.print(F(":"));
  Serial.print(now.minute(), DEC);
  Serial.print(F(":"));
  Serial.println(now.second(), DEC);
}

#endif

/**
 * Gets the string representation of the current date from the RTC
 */
String getCurrentDate() {
  DateTime now = rtc.now();
  String date = String(now.month(), DEC);
  date += '/';
  date += now.day();
  date += '/';
  date += now.year();

  return date;
}

/**
 * Gets the current battery voltage measured by the Feather
 */
float getCurrentVoltage() {
  float measuredVoltage = analogRead(BATT_PIN);
  measuredVoltage *= 2;    // Value is divided by 2 by voltage divider
  measuredVoltage *= 3.3;  // Feather is 3.3 device
  measuredVoltage /= 1024; // Convert to voltage

  return measuredVoltage;
}

/**
 * Gets the amount of storage remaining on the SD card, in kilobytes
 */
uint32_t getCurrentStorageKilobytes() {
  return (uint32_t)(getCurrentStorageBytes() / 1024);
}

/**
 * Gets the amount of storage remaining on the SD card, in bytes
 */
uint64_t getCurrentStorageBytes() {
  uint64_t maxStorage = getMaxStorageBytes();
  uint64_t usedStorage = 0;

  File root = SD.open("/");
  //root.rewindDirectory();
  while(true) {
    File entry = root.openNextFile();
    
    if(!entry) break; // No more files
    
    usedStorage += (uint64_t)entry.size();

    entry.close();
  }

  root.close();
  
  return maxStorage - usedStorage;
}

/**
 * Gets the maximum amount of storage of the SD card, in kilobytes
 */
uint32_t getMaxStorageKilobytes() { 
  return (uint32_t)(getMaxStorageBytes() / 1024);
}

/**
 * Gets the maximum amount of storage of the SD card, in bytes
 */
uint64_t getMaxStorageBytes() {
  uint64_t volumeSize = volume.blocksPerCluster() * volume.clusterCount() * 512;
  return volumeSize;
}

/**
 * Sends the contents of the specified file over BLE
 */
void sendFile(String fileName) {
  if(logFile) logFile.close();

  
  File file = SD.open(fileName, FILE_READ);
  
  if(file) {
    ble.print("%FILEDL%&");
    ble.print(fileName);
    ble.print("&");

    // Attempt at quicker communication... Try again later
    /*
    int sendBufferSize = 200;

    char data[sendBufferSize];
    uint16_t count = 0;

    while(true) {
      int16_t c = file.read();
      if(c == -1) {
        // End of File
        ble.write(data, count);
        break;
      }
      if(count == sendBufferSize) {
        ble.write(data, count);
        count = 0;
      }

      data[count++] = c;
    }
    */
    
    int c = file.read();
    if(c == -1) Serial.println("File is already at end..");
    while(c != -1) {
      ble.print((char)c);
      c = file.read();
    }
    
    
    file.close();
    
    ble.print("&%END%");
    ble.flush();
    Serial.println("-- Finished Sending File --");
  } else {
    Serial.print("Error opening file: ");
    Serial.println(fileName);
  }

  openLogFile();
}

