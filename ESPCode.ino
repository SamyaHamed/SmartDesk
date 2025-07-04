#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "EMAN"
#define WIFI_PASSWORD "123456789"
#define API_KEY " "
#define DATABASE_URL " "

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;
String currentUID = "";

#define RXD2 16
#define TXD2 17

unsigned long lastReconnectAttempt = 0;
String currentUID1 = "71A20B27";  
String currentUID2 = "B0EF9D21";

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected to WiFi");

  const long gmtOffset_sec = 2 * 3600;       
  const int daylightOffset_sec = 1 * 3600;   
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  while (now < 1700000000) {  
    delay(500);
    Serial.print("*");
    now = time(nullptr);
  }
  Serial.println("\nTime synchronized");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Signup OK");
    signupOK = true;
  } else {
    Serial.printf("Firebase Signup Failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (signupOK && Firebase.ready()) {
    if (Firebase.RTDB.setBool(&fbdo, "/users/" + currentUID1 + "/login", false)) {
      Serial.println("Login set to false for user 1");
    } else {
      Serial.println("Failed to set login for user 1");
    }

    if (Firebase.RTDB.setBool(&fbdo, "/users/" + currentUID2 + "/login", false)) {
      Serial.println("Login set to false for user 2");
    } else {
      Serial.println("Failed to set login for user 2");
    }
  } else {
    Serial.println("Firebase not ready or signup failed.");
  }
}

void loop() {
  maintainConnection();

  if (Serial2.available()) {
    String incoming = Serial2.readStringUntil('\n');
    incoming.trim();
    Serial.println("Received: " + incoming);

    if (incoming.startsWith("UID:")) {
      currentUID = incoming.substring(4);
      currentUID.trim();
      sendToSerial2("Current UID: " + currentUID);

      if (signupOK) {
        String path = "/users/" + currentUID + "/login";
        if (Firebase.RTDB.setBool(&fbdo, path, true)) {
          sendToSerial2("Login updated");
        } else {
          sendToSerial2("Login failed: " + fbdo.errorReason());
        }
      }
      delay(50);
    }
    else if (incoming.startsWith("store,")) {
      if (currentUID != "") {
        String itemName = incoming.substring(6);
        storeItem(currentUID, itemName);
      } else sendToSerial2("Please send UID first");
    }

    else if (incoming.startsWith("extract,")) {
      if (currentUID != "") {
        String itemName = incoming.substring(8);
        extractItem(currentUID, itemName);
      } else sendToSerial2("Please send UID first");
    }

    else if (incoming.startsWith("save,")) {
      if (currentUID != "") {
        int comma1 = incoming.indexOf(',');
        int comma2 = incoming.indexOf(',', comma1 + 1);

        if (comma1 != -1 && comma2 != -1) {
          int angle = incoming.substring(comma1 + 1, comma2).toInt();
          int length = incoming.substring(comma2 + 1).toInt();

          FirebaseJson json;
          json.set("saved_angle", angle);
          json.set("saved_length", length);

          String path = "/users/" + currentUID;
          if (Firebase.RTDB.updateNode(&fbdo, path, &json)) {
            sendToSerial2("Saved angle and length for " + currentUID);
          } else {
            sendToSerial2("Failed to save data: " + fbdo.errorReason());
          }
        } else {
          sendToSerial2("Invalid format. Use: save,<angle>,<length>");
        }
      } else {
        sendToSerial2("Please send UID first.");
      }
      delay(50);
    }

    else if (incoming == "recover") {
      if (currentUID != "") {
        String path = "/users/" + currentUID;
        int angle = 0, length = 0;

        bool ok1 = Firebase.RTDB.getInt(&fbdo, path + "/saved_angle");
        if (ok1) angle = fbdo.to<int>();

        bool ok2 = Firebase.RTDB.getInt(&fbdo, path + "/saved_length");
        if (ok2) length = fbdo.to<int>();

        if (ok1 && ok2) {
          Serial2.println("Recovered: Angle=" + String(angle) + ", Length=" + String(length));
        } else {
          sendToSerial2("Failed to recover data: " + fbdo.errorReason());
        }
      } else {
        sendToSerial2("Please send UID first.");
      }
      delay(50);
    }

    else if (incoming == "logout") {
      if (currentUID != "") {
        if (Firebase.RTDB.setBool(&fbdo, "/users/" + currentUID + "/login", false)) {
          sendToSerial2("Logged out: " + currentUID);
          currentUID = "";
        } else {
          sendToSerial2("Logout failed: " + fbdo.errorReason());
        }
      } else {
        sendToSerial2("No user logged in");
      }
      delay(50);
    }
    else {
      sendToSerial2("Unknown command: " + incoming);
    }
  }
}

void sendToSerial2(String msg) {
  Serial2.println(msg);
  Serial.println("[To Arduino] " + msg);
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected to WiFi");
}

void maintainConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnectAttempt > 5000) {
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastReconnectAttempt = millis();
    }
  }

  if (!Firebase.ready()) {
    Serial.println("Reconnecting to Firebase...");
    Firebase.begin(&config, &auth);
  }
}

// Include the previously updated storeItem and extractItem functions here as well
void storeItem(String uid, String itemName) {
  for (int i = 1; i <= 3; i++) {
    String basePath = "/drawers/drawer" + String(i);

    // Check reservation status
    if (Firebase.RTDB.getBool(&fbdo, basePath + "/reserved")) {
      bool isReserved = fbdo.boolData();

      if (isReserved) {
        // Already reserved – check if it's for this user
        if (Firebase.RTDB.getString(&fbdo, basePath + "/reserved_by") && fbdo.stringData() == uid) {
          // Look for empty item slot
          for (int j = 1; j <= 3; j++) {
            String itemPath = basePath + "/items/item" + String(j);
            if (Firebase.RTDB.getString(&fbdo, itemPath) && fbdo.stringData() == "empty") {
              Firebase.RTDB.setString(&fbdo, itemPath, itemName);
              Serial2.println("Stored in " + String(i)); 
              return;
            }
          }

          Serial2.println("No space in drawer " + String(i) + " to store the item.");  
          return;
        }
      } else {
        // Not reserved → reserve and store
        Firebase.RTDB.setBool(&fbdo, basePath + "/reserved", true);
        Firebase.RTDB.setString(&fbdo, basePath + "/reserved_by", uid);
        Firebase.RTDB.setInt(&fbdo, basePath + "/number", i);

        for (int j = 1; j <= 3; j++) {
          String itemPath = basePath + "/items/item" + String(j);
          if (Firebase.RTDB.getString(&fbdo, itemPath) && fbdo.stringData() == "empty") {
            Firebase.RTDB.setString(&fbdo, itemPath, itemName);
            Serial2.println("Stored in " + String(i));  
            return;
          }
        }

        Serial2.println("NoSpaceFound.");  
        return;
      }
    } else {
      Serial2.println("Error reading drawer " + String(i) + " status.");  
    }
  }

  Serial2.println("NoDrawerFound.");  
}

void extractItem(String uid, String itemName) {
  for (int i = 1; i <= 3; i++) {
    String basePath = "/drawers/drawer" + String(i);

    // Check if reserved by this user
    if (Firebase.RTDB.getString(&fbdo, basePath + "/reserved_by") && fbdo.stringData() == uid) {

      // Search items in the drawer
      for (int j = 1; j <= 3; j++) {
        String itemPath = basePath + "/items/item" + String(j);

        if (Firebase.RTDB.getString(&fbdo, itemPath) && fbdo.stringData() == itemName) {
          Firebase.RTDB.setString(&fbdo, itemPath, "empty");

          // Check if drawer is now empty → release it
          bool allEmpty = true;
          for (int k = 1; k <= 3; k++) {
            String checkPath = basePath + "/items/item" + String(k);
            if (Firebase.RTDB.getString(&fbdo, checkPath) && fbdo.stringData() != "empty") {
              allEmpty = false;
              break;
            }
          }

          if (allEmpty) {
            Firebase.RTDB.setBool(&fbdo, basePath + "/reserved", false);
            Firebase.RTDB.setString(&fbdo, basePath + "/reserved_by", "");
          }

          Serial2.println("Extracted from" + String(i));  
          return;
        }
      }
    }
  }

  Serial2.println("TheItemNotFound.");  
}