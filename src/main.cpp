#include <Arduino.h>

// remove in production
#include <env.h> // << contain env local vars

#include <WiFi.h>
//#include <NTPClient.h>
//#include <WiFiUdp.h>


/*
 * Free from conflicts
 * GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15,
 * GPIO16, GPIO17, GPIO18, GPIO19, GPIO21, GPIO22,
 * GPIO23, GPIO25, GPIO26, GPIO27
 */

/*
 * *** >>> WATERING <<< ***
 *
 * Water pump control definition
 */
#include <ESP32_WaterPumpIO.h>
constexpr size_t RELAYS = 4;                                                     //!< How many relays we control
int gpioRelayPins[RELAYS]{4, 5, 12, 13};                                         //!< GPIO's number we connected the relays
int hourLiterPumpVolumens[RELAYS]{400, 400, 400, 400};                           //! real hour/liter of the pumps
char *pumpNames[RELAYS]{'Growroom_1', 'Growroom_2', 'Growroom_3', 'Growroom_4'}; //! < Add names
auto Pump = ESP32_WaterPumpIO(gpioRelayPins, hourLiterPumpVolumens, reinterpret_cast<char>(pumpNames), RELAYS);
// #end WATERING <<< ***

/*
 * *** >>> DIR & FILE HANDLING <<< ***
 *
 *
 */

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me−no−dev/arduino−esp32fs−plugin */
#include "FS.h"
#include "SPIFFS.h"
#define FORMAT_SPIFFS_IF_FAILED true
#define GROW_PROGRAM_DIR "/growprograms"     //!< File location for the grow programm
#define LED_BUILTIN 2;                       //!< Intern LED on ESP-32-WROOM-32D ??? TODO check

// Replace with your network credentials
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

/*
 * *** >>> TIME & NTP-SERVER CALL <<< ***
 *
 *
 */
#include <time.h>                             // for time() ctime()
#define GMT_OFFSET 7200                      //!< Timezone Berlin, Germany +02:00 hours
#define MY_NTP_SERVER "de.pool.ntp.org"      //!< German Timezone Server
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

// Define NTP Client to get time
/*WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);*/

/* Init time vars */
time_t now;                                  //!< These are the seconds since Epoch (1970) - UTC
tm tm;                                       // the structure tm holds time information in a more convenient way *

void showTime() {
    time(&now); // read the current time
    localtime_r(&now, &tm);             // update the structure tm with the current time
    Serial.print("year:");
    Serial.print(tm.tm_year + 1900);    // years since 1900
    Serial.print("\tmonth:");
    Serial.print(tm.tm_mon + 1);        // January = 0 (!)
    Serial.print("\tday:");
    Serial.print(tm.tm_mday);           // day of month
    Serial.print("\thour:");
    Serial.print(tm.tm_hour);           // hours since midnight 0-23
    Serial.print("\tmin:");
    Serial.print(tm.tm_min);            // minutes after the hour 0-59
    Serial.print("\tsec:");
    Serial.print(tm.tm_sec);            // seconds after the minute 0-61*
    Serial.print("\twday");
    Serial.print(tm.tm_wday);           // days since Sunday 0-6
    if (tm.tm_isdst == 1)               // Daylight Saving Time flag
        Serial.print("\tDST");
    else
        Serial.print("\tstandard");
    Serial.println();
}

// FILE SYSTEM FUNCTION
void testFileIO(fs::FS &fs, const char * path){
   Serial.printf("Testing file I/O with %s\r\n", path);

   static uint8_t buf[512];
   File file = fs.open(path, FILE_WRITE);
   if(!file){
      Serial.println("− failed to open file for writing");
   }
}

void listDir(fs::FS &fs, const char * dirname, const uint8_t levels){
   Serial.printf("Listing directory: %s\r\n", dirname);

   File root = fs.open(dirname);
   if(!root){
      Serial.println("− failed to open directory");
      return;
   }
   if(!root.isDirectory()){
      Serial.println(" − not a directory");
      return;
   }

   File file = root.openNextFile();
   while(file){
      if(file.isDirectory()){
         Serial.print("  DIR : ");
         Serial.println(file.name());
         if(levels){
            listDir(fs, file.name(), levels -1);
         }
      } else {
         Serial.print("  FILE: ");
         Serial.print(file.name());
         Serial.print("\tSIZE: ");
         Serial.println(file.size());
      }
      file = root.openNextFile();
   }
}

void readFile(fs::FS &fs, const char * path){
   Serial.printf("Reading file: %s\r\n", path);

   File file = fs.open(path);
   if(!file || file.isDirectory()){
       Serial.println("− failed to open file for reading");
       return;
   }

   Serial.println("− read from file:");
   while(file.available()){
      Serial.write(file.read());
   }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
   Serial.printf("Writing file: %s\r\n", path);

   File file = fs.open(path, FILE_WRITE);
   if(!file){
      Serial.println("− failed to open file for writing");
      return;
   }
   if(file.print(message)){
      Serial.println("− file written");
   }else {
      Serial.println("− frite failed");
   }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
   Serial.printf("Appending to file: %s\r\n", path);

   File file = fs.open(path, FILE_APPEND);
   if(!file){
      Serial.println("− failed to open file for appending");
      return;
   }
   if(file.print(message)){
      Serial.println("− message appended");
   } else {
      Serial.println("− append failed");
   }
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
   Serial.printf("Renaming file %s to %s\r\n", path1, path2);
   if (fs.rename(path1, path2)) {
      Serial.println("− file renamed");
   } else {
      Serial.println("− rename failed");
   }
}

void deleteFile(fs::FS &fs, const char * path){
   Serial.printf("Deleting file: %s\r\n", path);
   if(fs.remove(path)){
      Serial.println("− file deleted");
   } else {
      Serial.println("− delete failed");
   }
}

void setup() {
    // Initialize Serial Monitor
    Serial.begin(115200);

   // spiffs filesystem mounting must be the first
   if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
      Serial.println("SPIFFS Mount Failed");
      return;
   }
   // test file system remove in production
   listDir(SPIFFS, "/", 1);

   readFile(SPIFFS, GROW_PROGRAM_DIR);

   Serial.println( "Test complete" );

    // use the NTP-timeserver to get local time
    Serial.println("\nNTP TZ DST - bare minimum");
    configTime(0, 0, MY_NTP_SERVER);  // 0, 0 because we will use TZ in the next line
    setenv("TZ", MY_TZ, 1);            // Set environment variable with your time zone
    tzset();

    // start w-lan connection
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize a NTPClient to get time
  //timeClient.begin();
    // Set offset time in seconds to adjust for your timezone, for example:
    // GMT +1 = 3600
    // GMT +8 = 28800
    // GMT -1 = -3600
    // GMT 0 = 0
    //timeClient.setTimeOffset(GMT_OFFSET);
}

void loop() {
    showTime();
    delay(1000); // dirty delay
    /*while(!timeClient.update()) {
        timeClient.forceUpdate();
    }
    // The formattedDate comes with the following format:
    // 2018-05-28T16:00:13Z
    // We need to extract date and time
    const String formattedTime = timeClient.getFormattedTime();
    Serial.println(formattedTime);

    // Extract date
    const int splitT = formattedTime.indexOf("T");
    const String dayStamp = formattedTime.substring(0, splitT);
    Serial.print("DATE: ");
    Serial.println(dayStamp);
    // Extract time
    const String timeStamp = formattedTime.substring(splitT+1, formattedTime.length()-1);
    Serial.print("HOUR: ");
    Serial.println(timeStamp);

    Serial.println(timeClient.getEpochTime());
    delay(1000);*/
}

