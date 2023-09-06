#include "func.h"

void setup() {
  pinMode(loginPin, INPUT);
  pinMode(logoutPin, INPUT);
  pinMode(settingsPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // initialises all serials required
  Serial.begin(57600);
  Serial.println();
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  // configuring static ip
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA failed to configure");
  }

  if (finger.verifyPassword()) {
    Serial.println("fingerprint connected");
  } else {
    Serial.println("fingerprint sensor not found");
    ESP.restart();
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  WiFi.begin(ssid, password);

  Serial.println("connecting");
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
    count++;
    // this restarts board if it takes more than 90 seconds to connect to wifi
    if (count == 90) {
      Serial.println("couldnt connect to wifi");
      Serial.println("restarting...\n");
      ESP.restart();
    }
  }

  Serial.println("");
  Serial.printf("connected, ");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // restart board if the time shown isn't right
  printTimeToSerial();
  Serial.println();

  // just prints number of fingers registered
  finger.getTemplateCount();
  Serial.print(finger.templateCount);
  Serial.println(" fingers registered");

  if (!SD.begin()){
    Serial.println("mounting sd card failed");
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE){
    Serial.println("No sd card found");
  }

  Serial.println("SD card found\n");
  Serial.println("Started at : ");
  printTimeToSerial();
  Serial.println("\n");
  dumpNames();
}

void loop() {
  // this checks if the login button is pressed
  if (digitalRead(loginPin)) {
    int finger = getFingerprintIDez();
    
    // this is to check if someone is already logged in, and if it is, it logs them out 
    if (loggedIn){
      name = getNameFromID(finger_id_string);
      student_id = getStudentIDFromID(finger_id_string);
      // getting name and student ids again because someone might change settings while someone is logged in, and that may cause weird results
      printTimeToSD();
      logs = " : " + name + "(" + student_id + ") logged out";
      saveLogs(logs);
    }

    // if the finger is -1, it means some error occured while reading fingerprint
    if (finger != -1) {
      loggedIn = 1;
      finger_id_string = String(finger);
      digitalWrite(ledPin, 1);

      name = getNameFromID(finger_id_string);
      student_id = getStudentIDFromID(finger_id_string);

      printTimeToSD();
      logs = " : " + name + "(" + student_id + ")" + " logged in";
      saveLogs(logs); // prints logs to logs.txt as well as serial

    } else {
      digitalWrite(ledPin, 0);
      loggedIn = 0;

      printTimeToSD();
      logs = " : some error occured while getting fingerprint";
      saveLogs(logs);
    }
  }

  // this just logs out last person that logged in
  if (digitalRead(logoutPin) && loggedIn) {
    digitalWrite(ledPin, 0);
    loggedIn = 0;
    String name = getNameFromID(finger_id_string);
    String student_id = getStudentIDFromID(finger_id_string);
 
    printTimeToSD();
    logs = " : " + name + "(" + student_id + ")" + " logged out";
    saveLogs(logs);
  }

  // this checks if the settings pin is pulled to high
  if (digitalRead(settingsPin)) {
    if (!settings_mode) {
      Serial.println("\nSettings mode enabled");
      Serial.println(settings_message);
      settings_mode = 1;

      printTimeToSD();
      saveLogs(" : Settings mode enabled");
    }

    // reads mode from terminal
    mode = Serial.readStringUntil('\n');
    mode.trim();
    
    // enroll mode, it enrolls new users
    if (mode == "e") {
      Serial.println("enrolling mode");
      Serial.println("enrolling fingerprint");
      id = getLastRegisteredID()+1;

      name = "";
      student_id = "";
      Serial.println("enter name");
      while (name == ""){ // check if the name is not blank
        name = Serial.readStringUntil('\n');
        name.trim();
      }

      Serial.println("enter student id");
      while (student_id == ""){ // check if the student id entered is not blank
        student_id = Serial.readStringUntil('\n');
        student_id.trim();
      }

      Serial.println("Enrolling "+name+"("+student_id+")");

      // getFingerprintEnroll returns 1 if it is successfull otherwise returns 0 or -1
      if (getFingerprintEnroll() == 1){
        // saves data and logs for registered finger
        saveLastRegisteredID(id);
        saveData(name, id, student_id);

        printTimeToSD();
        logs = " : " + name + "(" + student_id + ") Enrolled";
        saveLogs(logs);
        Serial.println(settings_message);
      }
    }

    // delete mode, this deletes already registered user
    else if (mode == "d") {
      Serial.println("delete mode");
      Serial.println("-------------------------------");
      // dumps all names for user to select which one to delete
      dumpNames();
      Serial.println("-------------------------------");
      Serial.println("Enter ID to delete");
      // reads number from serial
      id = readnumber();
      if (id == 0) {  // ID #0 not allowed
        return;
      }
      Serial.print("Deleting ID #");
      Serial.println(id);

      // deleteFingerPrint returns -1 if it fails
      if (deleteFingerprint(id) != -1){
        SD.remove(location(id));
        Serial.println("deleted");

        printTimeToSD();
        char buff[4];
        String id_string = itoa(id, buff, 10);
        name = getNameFromID(id_string);
        student_id = getNameFromID(id_string);
        logs = " : " + name + "(" + student_id + ") was deleted";
        saveLogs(logs);
      }
    } else if (!mode) {
      Serial.println(settings_message);
    }
  }

  if (!digitalRead(settingsPin) && settings_mode) {  // this means settings mode was previously 1 and the value of settings pin was set to low
    // Serial.println("Settings mode disabled\n");
    settings_mode = 0;
    printTimeToSD();
    logs = " : Settings mode disabled";
    saveLogs(logs); 
  }
  delay(100);
}

// returns finger id if successful, else returns -1
int getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -1;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return -1;
    default:
      Serial.println("Unknown error");
      return -1;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -1;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return -1;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return -1;
    default:
      Serial.println("Unknown error");
      return -1;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return -1;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return -1;
  } else {
    Serial.println("Unknown error");
    return -1;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  return finger.fingerID;
}

// returns finger id if successful, else returns -1
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;

  // found a match!
  // Serial.print("Found ID #"); Serial.print(finger.fingerID);
  // Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

// returns -1 if it fails
uint8_t getFingerprintEnroll() {

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}

// returns -1 if it fails
uint8_t deleteFingerprint(uint8_t id) {
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
  } else {
    Serial.print("Unknown error: 0x");
    Serial.println(p, HEX);
  }

  return p;
}
