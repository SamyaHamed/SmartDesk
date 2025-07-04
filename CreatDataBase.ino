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

void setup() {
  Serial.begin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🔌 Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\n✅ Connected to WiFi");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("✅ SignUp OK");
    signupOK = true;
  } else {
    Serial.printf("❌ SignUp Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  delay(1000);

  if (!signupOK) return;

  String uid1 = "71A20B27";
  Firebase.RTDB.setInt(&fbdo, "/users/" + uid1 + "/Length", 165); delay(200);
  Firebase.RTDB.setBool(&fbdo, "/users/" + uid1 + "/login", true); delay(200);
  Firebase.RTDB.setString(&fbdo, "/users/" + uid1 + "/name", "Eman"); delay(200);
  Firebase.RTDB.setInt(&fbdo, "/users/" + uid1 + "/saved_angle", 30); delay(200);
  Firebase.RTDB.setInt(&fbdo, "/users/" + uid1 + "/saved_length", 50); delay(200);
  Firebase.RTDB.setString(&fbdo, "/users/" + uid1 + "/angleMode", "auto"); delay(200);
  Firebase.RTDB.setString(&fbdo, "/users/" + uid1 + "/lengthMode", "manual"); delay(200);
  Firebase.RTDB.setBool(&fbdo, "/users/" + uid1 + "/GazDetect", false); delay(200);
  Firebase.RTDB.setFloat(&fbdo, "/users/" + uid1 + "/tempreture", 24.5); delay(200);

 Firebase.RTDB.setString(&fbdo, "/users/" + uid1 + "/tasks/task1/name", "Do Homework");
 Firebase.RTDB.setBool(&fbdo, "/users/" + uid1 + "/tasks/task1/done", false);

 Firebase.RTDB.setString(&fbdo, "/users/" + uid1 + "/tasks/task2/name", "Do Homework");
 Firebase.RTDB.setBool(&fbdo, "/users/" + uid1 + "/tasks/task2/done", false);



  String uid2 = "B0EF9D21";
  Firebase.RTDB.setInt(&fbdo, "/users/" + uid2 + "/Length", 160); delay(200);
  Firebase.RTDB.setBool(&fbdo, "/users/" + uid2 + "/login", false); delay(200);
  Firebase.RTDB.setString(&fbdo, "/users/" + uid2 + "/name", "Samya"); delay(200);
  Firebase.RTDB.setInt(&fbdo, "/users/" + uid2 + "/saved_angle", 20); delay(200);
  Firebase.RTDB.setInt(&fbdo, "/users/" + uid2 + "/saved_length", 45); delay(200);
  Firebase.RTDB.setString(&fbdo, "/users/" + uid2 + "/angleMode", "manual"); delay(200);
  Firebase.RTDB.setString(&fbdo, "/users/" + uid2 + "/lengthMode", "auto"); delay(200);
  Firebase.RTDB.setBool(&fbdo, "/users/" + uid2 + "/GazDetect", true); delay(200);
  Firebase.RTDB.setFloat(&fbdo, "/users/" + uid2 + "/tempreture", 22.0); delay(200);

  Firebase.RTDB.setString(&fbdo, "/users/" + uid2 + "/tasks/task1/name", "Do Homework");
 Firebase.RTDB.setBool(&fbdo, "/users/" + uid2 + "/tasks/task1/done", false);

 Firebase.RTDB.setString(&fbdo, "/users/" + uid2 + "/tasks/task2/name", "Do Homework");
 Firebase.RTDB.setBool(&fbdo, "/users/" + uid2 + "/tasks/task2/done", false);


  Firebase.RTDB.setInt(&fbdo, "/drawers/drawer1/number", 1); delay(200);
  Firebase.RTDB.setString(&fbdo, "/drawers/drawer1/item", "Charger"); delay(200);
  Firebase.RTDB.setBool(&fbdo, "/drawers/drawer1/reserved", true); delay(200);
  Firebase.RTDB.setString(&fbdo, "/drawers/drawer1/reserved_by", uid1); delay(200);

  Firebase.RTDB.setInt(&fbdo, "/drawers/drawer2/number", 2); delay(200);
  Firebase.RTDB.setString(&fbdo, "/drawers/drawer2/item", "Notebook"); delay(200);
  Firebase.RTDB.setBool(&fbdo, "/drawers/drawer2/reserved", false); delay(200);
  Firebase.RTDB.setString(&fbdo, "/drawers/drawer2/reserved_by", "uid2"); delay(200);

  Firebase.RTDB.setInt(&fbdo, "/drawers/drawer3/number", 3); delay(200);
  Firebase.RTDB.setString(&fbdo, "/drawers/drawer3/item", "Markers"); delay(200);
  Firebase.RTDB.setBool(&fbdo, "/drawers/drawer3/reserved", true); delay(200);
  Firebase.RTDB.setString(&fbdo, "/drawers/drawer3/reserved_by", uid2); delay(200);

  Serial.println("✅ Data structure created successfully.");
}

void loop() {
}
