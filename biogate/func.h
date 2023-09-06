#include <HardwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include "time.h"

uint8_t id;
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// offsets for time
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

const char* ntpServer = "time.google.com";
const char* ssid = "DESKTOP-N54MTE2 0401";
const char* password = "q5;04E63";

// this is for static ip
IPAddress local_IP(192, 168, 137, 31);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

int loginPin = 14;
int logoutPin = 27;
int settingsPin = 25;
int ledPin = 12;
bool loggedIn = 0;
bool settings_mode = 0;

String mode;
String name;
String student_id;
String logs;
String loc;
String finger_id_string;
String settings_message = "enter e to enroll fingerprint\nenter d to delete fingerprint";


// prints time to serial
void printTimeToSerial() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print(&timeinfo, "%H:%M:%S %d-%m-%y");
}

// prints time to logs.txt file
void printTimeToSD(){
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  File file = SD.open("/logs.txt", FILE_APPEND);
  file.print(&timeinfo, "%H:%M:%S %d-%m-%y");
  file.close();
}

// reads Serial and returns number entered
uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (!Serial.available());
    num = Serial.parseInt();
  }
  return num;
}

// returns location of file from uint8_t id
String location(uint8_t finger_id){
  char buffer[4];
  finger_id_string = itoa(finger_id, buffer, 10);
  loc = "/data/"+finger_id_string+".txt";
  return loc;
}

// saves data for new users
void saveData(String name, uint8_t finger_id, String student_id, bool delete_prev=0){
  loc = location(finger_id);
  if (delete_prev) { SD.remove(loc); }
  File file = SD.open(loc, FILE_WRITE, 1);
  if (!file){
    Serial.println("failed to open file");
  }
  file.print(name);
  file.print(",");
  file.print(student_id);
  file.close();
}


// gets name of student from id(String)
String getNameFromID(String finger_id_string){
  loc = "/data/"+finger_id_string+".txt";
  File file = SD.open(loc);
  String data = file.readString();
  file.close();

  int index_of_sep = data.indexOf(',');
  name = data.substring(0, index_of_sep);
  return name;
}

// fetches student id from finger id(String)
String getStudentIDFromID(String finger_id_string){
  loc = "/data/"+finger_id_string+".txt";
  File file = SD.open(loc);
  String data = file.readString();
  file.close();

  int index_of_sep = data.indexOf(',');
  student_id = data.substring(index_of_sep+1, data.length());
  return student_id;
}

// this is to keep record of last enrolled fingerprint, it is needed when enrolling new users
void saveLastRegisteredID(uint8_t finger_id){
  File file = SD.open("/last_reg_id.txt", FILE_WRITE, 1);
  file.println(finger_id);
  file.close();
}

// returns last id saved by saveLastRegisteredID() function
uint8_t getLastRegisteredID(){
  File file = SD.open("/last_reg_id.txt");
  String finger_id_string = file.readString();
  file.close();

  int finger_id_int = finger_id_string.toInt();
  uint8_t finger_id = finger_id_int;
  return finger_id;
}

// takes string log and saves it to a text file
void saveLogs(String log_data){
  File logs = SD.open("/logs.txt", FILE_APPEND, 1);
  if (!logs){
    Serial.println("failed to open file");
    return;
  }
  printTimeToSD();
  logs.print(log_data);
  logs.print('\n');
  printTimeToSerial();
  Serial.println(log_data);
  logs.close();
  return;
}

// returns list of all registered students
void dumpNames(){
  int last_id = getLastRegisteredID();
  for (int i=0; i<=last_id; i++){
    String id_string = String(i);
    String name = getNameFromID(id_string);
    String student_id = getStudentIDFromID(id_string);
    Serial.println(id_string + " - " + name + "(" + student_id + ")");
  }
}
