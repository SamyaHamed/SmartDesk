#include <Wire.h>
#include <NewPing.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_BMP280.h>
#include <MQUnifiedsensor.h>

bool systemLocked = true; 
int extractedDrawerNumber=-1;
int storedDrawerNumber =-1;
//---------------Angle------------------------//
#define T_0_TO_15_UP     10956
#define T_0_TO_15_DOWN   12500  
#define T_15_TO_22_UP     8942
#define T_15_TO_22_DOWN   9900
#define T_22_TO_35_UP     7700
#define T_22_TO_35_DOWN   8720
//---------- Sounds-----------------------//
#define SOUND_EMAN            1
#define SOUND_SAMYA           2
#define SOUND_INVALID_CARD    3
#define SOUND_EMERGENCY       4
#define SOUND_STORE_ITEM      5
#define SOUND_CLOSE_DRAWER    6
#define SOUND_NO_SPACE        7
#define SOUND_NO_DRAWER       8
#define SOUND_GAS_DETECTED    9
#define SOUND_TEMP_HIGH       10
#define SOUND_TEMP_LOW        11
#define SOUND_ITEM_NOT_FOUND  12
#define SOUND_EXTRACT_ITEM    13 

int currentAngle = 0;  // Start at 0 degrees
int tempCounter = 0;
bool logoutPendingLock = false; 

//-----voice-----------------------------------//

unsigned long lastEmergencySoundTime = 0;
const unsigned long emergencySoundInterval = 3000;

#define mp3Serial Serial3 
void sendCommand(uint8_t command, uint16_t parameter) {
  uint8_t commandLine[10] = {
    0x7E, 0xFF, 0x06, command, 0x00,
    (uint8_t)(parameter >> 8),
    (uint8_t)(parameter & 0xFF),
    0x00, 0x00, 0xEF
  };
  uint16_t checksum = 0 - (commandLine[1] + commandLine[2] + commandLine[3] +
                           commandLine[4] + commandLine[5] + commandLine[6]);
  commandLine[7] = (uint8_t)(checksum >> 8);
  commandLine[8] = (uint8_t)(checksum & 0xFF);

  for (uint8_t i = 0; i < 10; i++) {
    mp3Serial.write(commandLine[i]);
  }
}
void setupMP3() {
  Serial.begin(9600);
  mp3Serial.begin(9600);
  delay(1000);
  sendCommand(0x3F, 0);  
  delay(500);
  sendCommand(0x06, 20);  
  delay(500);

}
void playSound(uint8_t soundType) {
  switch (soundType) {
    case SOUND_EMAN:
      sendCommand(0x03, 1);
      break;
    case SOUND_SAMYA:
      sendCommand(0x03, 2);
      break;
    case SOUND_INVALID_CARD:
      sendCommand(0x03, 3);
      break;
    case SOUND_EMERGENCY:
      sendCommand(0x03, 4);
      break;
    case SOUND_STORE_ITEM:
      sendCommand(0x03, 5);
      break;
    case SOUND_CLOSE_DRAWER:
      sendCommand(0x03, 6);
      break;
    case SOUND_NO_SPACE:
      sendCommand(0x03, 7);
      break;
    case SOUND_NO_DRAWER:
      sendCommand(0x03, 8);
      break;
    case SOUND_GAS_DETECTED:
      sendCommand(0x03, 9);
      break;
    case SOUND_TEMP_HIGH:
      sendCommand(0x03, 10);
      break;
    case SOUND_TEMP_LOW:
      sendCommand(0x03, 11);
      break;
    case SOUND_ITEM_NOT_FOUND:
      sendCommand(0x03, 12);
      break;
    case SOUND_EXTRACT_ITEM:
      sendCommand(0x03, 13);
      break;
    default:
      return;
  }

}

//----steper Motors----------------------------//

// Motor 1

const int IN1D1 = 23;
const int IN2D1 = 25;
const int IN3D1 = 27;
const int IN4D1 = 29;

// Motor 2

const int IN1D2 = 31;
const int IN2D2 = 32;
const int IN3D2 = 33;
const int IN4D2 = 34;

// Motor 3

const int IN1D3 = 35;
const int IN2D3 = 36;
const int IN3D3 = 37;
const int IN4D3 = 38;

const int STEPS_PER_REV = 200; // typical for NEMA 17
const int STEP_DELAY = 5;      // ms between steps

const int ROTATE_DURATION = 25000;
const int OPEN_DURATION = 14000;

void rotateStepper(int drawer, bool clockwise, int duration) {
  int IN1, IN2, IN3, IN4;
  switch (drawer) {
    case 1: IN1 = IN1D1; IN2 = IN2D1; IN3 = IN3D1; IN4 = IN4D1; break;
    case 2: IN1 = IN1D2; IN2 = IN2D2; IN3 = IN3D2; IN4 = IN4D2; break;
    case 3: IN1 = IN1D3; IN2 = IN2D3; IN3 = IN3D3; IN4 = IN4D3; break;
    default: return;
  }

  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    if (clockwise) {
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); delay(5);

      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); delay(5);

      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); delay(5);

      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); delay(5);
    } else {
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); delay(5);

      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); delay(5);

      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); delay(5);

      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); delay(5);
    }
  }

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void openDrawer(int drawerNum) {

  rotateStepper(drawerNum, true, ROTATE_DURATION);
}

void closeDrawer(int drawerNum) {
  rotateStepper(drawerNum, false, ROTATE_DURATION);
  playSound(SOUND_CLOSE_DRAWER);
}

//---------------Hight and Recover Angle------------------------//
int recoveredAngle = -1;
int recoveredLength = -1;

long avgHeight ;
long handDist ;
//-----GAS-----------//

int warmupReadingsSkipped = 0;
const int warmupReadingsLimit = 6;

bool gasDetected = false;
unsigned long lastGasSoundTime = 0;
unsigned long lastFlashTime = 0;
//bool gasDetected = false;
bool flashOn = false;

const unsigned long gasSoundInterval = 3000; 
const unsigned long flashInterval = 200;    


//--------EmergencyButon-------------//
const int buttonPin = 2;      
volatile bool buttonState = LOW;
bool isEmergency = false;
volatile bool emergencyRequested = false;

//---RGB/LDR-------------------
#define RED_PIN 12
#define GREEN_PIN 11
#define BLUE_PIN 10
#define ledPin 3
#define ldrPin A10
const int step = 25;
int mode = 0;
int brightness = 0;
bool brightnessChanged = false;

//-------------------------------FLASHING RED for Emergency ---------------------------------------
// Red LED
bool redFlashing = false;
unsigned long redPreviousMillis = 0;
const long redFlashInterval = 100;
int redFlashState = LOW;
unsigned long redFlashStartTime = 0;
const long redFlashDuration = 4000; 

void updateRedFlash(int redPin) {
  if (redFlashing) {
    unsigned long currentMillis = millis();

    if (currentMillis - redFlashStartTime >= redFlashDuration) {
      analogWrite(redPin, 0);
      redFlashing = false;
      return;
    }

    if (currentMillis - redPreviousMillis >= redFlashInterval) {
      redPreviousMillis = currentMillis;
      redFlashState = !redFlashState;
      analogWrite(redPin, redFlashState ? 255 : 0);
    }
  }
}

void EmergencyflashRed(int redPin) {
  redFlashing = true;
  redFlashStartTime = millis();
}
//---------flashing login 
void flashGreen(int greenPin) {
  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    analogWrite(greenPin, 255);
    delay(100);
    analogWrite(greenPin, 0);
    delay(100);
  }
}
void flashRed(int redPin) {
  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    analogWrite(redPin, 255);
    delay(100);
    analogWrite(redPin, 0);
    delay(100);
  }
}
#define TRIG_PIN_2 7
#define ECHO_PIN_2 6
#define TRIG_PIN_3 8
#define ECHO_PIN_3 9
#define MAX_DISTANCE 200
NewPing sonar1(TRIG_PIN_2, ECHO_PIN_2, MAX_DISTANCE);
NewPing sonar2(TRIG_PIN_2, ECHO_PIN_2, MAX_DISTANCE);
NewPing sonar3(TRIG_PIN_3, ECHO_PIN_3, MAX_DISTANCE);

const int SIT_HEIGHT = 85;
const int STAND_HEIGHT = 110;
const int MAX_TABLE_HEIGHT = 120;
const int margin_height = 5;
String heightMode = "idle";
long targetHeight = 0;

// ------------------ RELAYS ------------------
#define RELAY1 24
#define RELAY2 26
#define RELAY3 28
#define RELAY4 30

// ------------------ MFRC522 ------------------
#define SS_PIN 53
#define RST_PIN 5
MFRC522 mfrc522(SS_PIN, RST_PIN);
String validUIDs[] = {"71A20B27","B0EF9D21"};
// ------------------ MQ135 ------------------
#define Board "Arduino Mega"
#define Voltage_Resolution 5
#define ADC_Bit_Resolution 10
#define MQ135_pin A0
#define Type "MQ-135"
#define RatioMQ135CleanAir 3.6

MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ135_pin, Type);

void moveAngleUp() {
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, HIGH);
}

void moveAngleDown() {
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, LOW);
}

void stopAngleMotor() {
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
}

// ------------------ UltraSonic------------------
void moveHeightDown() {
  digitalWrite(RELAY3, LOW);
  digitalWrite(RELAY4, HIGH);
}
void moveHeightUp() {
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, LOW);
}
void stopHeightMotor() {
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);
}
long getAverageDistance() {
  long d1 = sonar1.ping_cm();
  long d2 = sonar2.ping_cm();
  if (d1 == 0) d1 = MAX_DISTANCE;
  if (d2 == 0) d2 = MAX_DISTANCE;
  return (d1 + d2) / 2;
}
long getHandDistance() {
  long d = sonar3.ping_cm();
  if (d == 0) d = MAX_DISTANCE;
  return d;
}

// ------------------ MFRC522------------------

bool isValidCard(String uidString) {
  for (int i = 0; i < sizeof(validUIDs) / sizeof(validUIDs[0]); i++) {
    if (uidString == validUIDs[i]) return true;
  }
  return false;
}

void sendCommandToNextion(const char* cmd) {
  Serial1.print(cmd);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
}

void sendOkToNextion() {
  sendCommandToNextion("msg.txt=\"Access Granted!\"");
  delay(1000); 
  sendCommandToNextion("page 7");
}



void unlockSystem() {
  systemLocked = false;
  sendOkToNextion();
}

void lockSystem() {
  systemLocked = true;
  sendCommandToNextion("msg.txt=\"Access Denied!\"");
}

// ------------------ BMP280 ------------------
Adafruit_BMP280 bmp;
void setupBMP280() {
  if (!bmp.begin(0x76)) {
    Serial.println("ERRORE IN BMP280!");
    while (1);
  }
}
// ------------------ MQ135 ------------------
void setupMQ135() {
  MQ135.setRegressionMethod(1);
  MQ135.setA(110.47); MQ135.setB(-2.862);
  MQ135.init();
  MQ135.setRL(10);
   delay(20000);
  MQ135.setR0(4.09);
}

//----------------emergency button ---------------
void buttonISR() {
  emergencyRequested = true;
}

void setupEmergencyButon() {
  pinMode(buttonPin, INPUT_PULLUP);  
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, CHANGE);
}

void sendEmergencyToNextion() {
  sendCommandToNextion("page 9"); 
sendCommandToNextion("Emergency.txt=\"Emergency Activated\""); 
}

void EmergencyButon() {
  isEmergency = true;
  Serial2.println("logout");
  stopAngleMotor();
  stopHeightMotor();
  sendEmergencyToNextion();
}

void resetSystem() {
  isEmergency = false;
  systemLocked = true;
  heightMode = "idle";
  stopAngleMotor();
  stopHeightMotor();
  analogWrite(RED_PIN, 0); 
  Serial1.print("page 1");
  Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
  mfrc522.PCD_Init(); 
  delay(500); 
}

void moveToAngle(int targetAngle) {

  int direction = (targetAngle > currentAngle) ? 1 : -1;
  int timeToMove = computeTime(currentAngle, targetAngle);

  if (direction == 1) {
    moveAngleUp();
  } else {
    moveAngleDown();
  }

  unsigned long startTime = millis();
  while (millis() - startTime < timeToMove) {
    if (digitalRead(buttonPin) == LOW) {
      EmergencyButon();   
      return;  
    }
    delay(10); 
  }

  stopAngleMotor();  
  currentAngle = targetAngle; 

int computeTime(int fromAngle, int toAngle) {
  if (fromAngle == toAngle) return 0;

  bool goingUp = toAngle > fromAngle;

  if (fromAngle == 0 && toAngle == 15)
    return goingUp ? T_0_TO_15_UP : T_0_TO_15_DOWN;

  else if (fromAngle == 15 && toAngle == 22)
    return goingUp ? T_15_TO_22_UP : T_15_TO_22_DOWN;

  else if (fromAngle == 22 && toAngle == 35)
    return goingUp ? T_22_TO_35_UP : T_22_TO_35_DOWN;

  else if (fromAngle == 0 && toAngle == 22)
    return goingUp ? (T_0_TO_15_UP + T_15_TO_22_UP) : (T_15_TO_22_DOWN + T_0_TO_15_DOWN);

  else if (fromAngle == 0 && toAngle == 35)
    return goingUp ? (T_0_TO_15_UP + T_15_TO_22_UP + T_22_TO_35_UP)
                   : (T_22_TO_35_DOWN + T_15_TO_22_DOWN + T_0_TO_15_DOWN);

  else if (fromAngle == 15 && toAngle == 35)
    return goingUp ? (T_15_TO_22_UP + T_22_TO_35_UP)
                   : (T_22_TO_35_DOWN + T_15_TO_22_DOWN);

  else if (fromAngle == 22 && toAngle == 0)
    return T_15_TO_22_DOWN + T_0_TO_15_DOWN + 200;

  else if (fromAngle == 35 && toAngle == 0)
    return T_22_TO_35_DOWN + T_15_TO_22_DOWN + T_0_TO_15_DOWN + 250;

  else if (fromAngle == 35 && toAngle == 22)
    return T_22_TO_35_DOWN;

  else if (fromAngle == 22 && toAngle == 15)
    return T_15_TO_22_DOWN;

  else if (fromAngle == 15 && toAngle == 0)
    return T_0_TO_15_DOWN + 200; 

  return 0;  
}

//-----------------NEXTION----------------------------//
#define CMD_BUFFER_SIZE 100
char cmdBuffer[CMD_BUFFER_SIZE];
byte cmdIndex = 0;

void handleNextionInput() {
  while (Serial1.available()) {
    char c = Serial1.read();

    if (c == '\n') {
      cmdBuffer[cmdIndex] = '\0'; 
      cmdIndex = 0;  

      Serial.print("Received from Nextion: ");
      Serial.println(cmdBuffer);

      if (strcmp(cmdBuffer, "laptop") == 0) {
        moveToAngle(0);
      } 
      else if (strcmp(cmdBuffer, "read") == 0) {
        moveToAngle(22);
      } 
      else if (strcmp(cmdBuffer, "write") == 0) {
         moveToAngle(15);
      } 
      else if (strcmp(cmdBuffer, "draw") == 0) {
        moveToAngle(35);
      }

      else if (strcmp(cmdBuffer, "sit") == 0) {
        targetHeight = SIT_HEIGHT;
        heightMode = "auto";

      } 
      else if (strcmp(cmdBuffer, "stand") == 0) {
        targetHeight = STAND_HEIGHT;
        heightMode = "auto";
      }

      else if (strcmp(cmdBuffer, "auto") == 0) {
        mode = 0;
      }

      else if (strcmp(cmdBuffer, "plus") == 0) {
       brightness += step;
       brightness = constrain(brightness, 0, 255);
       mode = 1;
      brightnessChanged = true;


      } 
      else if (strcmp(cmdBuffer, "minus") == 0) {
        brightness -= step;
       brightness = constrain(brightness, 0, 255);
      mode = 1;
      brightnessChanged = true;
      }

      else if (strcmp(cmdBuffer, "save") == 0) {
      
       String dataToSend = "save," + String(currentAngle) + "," + String(avgHeight);
      Serial2.println(dataToSend);  
      }  

      else if (strcmp(cmdBuffer, "recover") == 0) {
        Serial2.println("recover");
      }

      else if (strncmp(cmdBuffer, "store,", 6) == 0) {
       String itemName = String(cmdBuffer + 6);  
       String commandToESP = "store," + itemName;
       Serial2.println(commandToESP);
      }

      else if (strncmp(cmdBuffer, "extract,", 8) == 0) {
      String itemName = String(cmdBuffer + 8); 
      String commandToESP = "extract," + itemName;
      Serial2.println(commandToESP);
    }

    else if (strcmp(cmdBuffer, "Logout") == 0) {
  Serial2.println("logout");

  if (currentAngle != 0) {
    moveToAngle(0);
    delay(10); 
  }

  avgHeight = getAverageDistance();  

  if (abs(avgHeight - SIT_HEIGHT) > margin_height) {
    targetHeight = SIT_HEIGHT;
    heightMode = "auto";
    logoutPendingLock = true; 
    delay(20);
  } else {
    systemLocked = true; 
  }

  analogWrite(RED_PIN, 0);

  Serial1.print("page 1");
  Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);

  mfrc522.PCD_Init(); 
  delay(500);
}

    } else {
      if (cmdIndex < CMD_BUFFER_SIZE - 1) {
        cmdBuffer[cmdIndex++] = c;
      }
    }
  }
}

// ------------------ Setup ------------------
void setup() {
  Serial2.setTimeout(100);
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial2.begin(9600);


  SPI.begin(); mfrc522.PCD_Init();
  Wire.begin();

  pinMode(RELAY1, OUTPUT); pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT); pinMode(RELAY4, OUTPUT);
  pinMode(ledPin, OUTPUT); analogWrite(ledPin, brightness);
  pinMode(RED_PIN, OUTPUT); pinMode(GREEN_PIN, OUTPUT); pinMode(BLUE_PIN, OUTPUT);

//stepers
  int pins[] = {IN1D1, IN2D1, IN3D1, IN4D1, IN1D2, IN2D2, IN3D2, IN4D2, IN1D3, IN2D3, IN3D3, IN4D3};
  for (int i = 0; i < 12; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }
  stopAngleMotor(); stopHeightMotor();
  mfrc522.PICC_HaltA(); mfrc522.PCD_StopCrypto1();
  setupBMP280(); setupMP3();setupMQ135(); setupEmergencyButon();

  Serial1.print("page 0");
  Serial1.write(0xFF); Serial1.write(0xFF); Serial1.write(0xFF);
}

void listenToSerial2() {
  if (Serial2.available()) {
    String incoming = Serial2.readStringUntil('\n');
    incoming.trim();

    String cleanIncoming = "";
    for (int i = 0; i < incoming.length(); i++) {
      if (isPrintable(incoming[i])) cleanIncoming += incoming[i];
    }

    incoming = cleanIncoming;
    if (incoming.startsWith("Recovered:")) {
  int angleIndex = incoming.indexOf("Angle=");
  int lengthIndex = incoming.indexOf("Length=");

  if (angleIndex != -1 && lengthIndex != -1) {
    String angleStr = incoming.substring(angleIndex + 6, incoming.indexOf(',', angleIndex));
    String lengthStr = incoming.substring(lengthIndex + 7);

    recoveredAngle = angleStr.toInt();
    recoveredLength = lengthStr.toInt();

    if (currentAngle != recoveredAngle) {
     moveToAngle(recoveredAngle);
     delay(10);
     }

    if ( avgHeight != recoveredLength){
    targetHeight = recoveredLength;
    heightMode = "auto";
    }
    
  } else {
     Serial.println("Invalid Recovery Data!");
  }
}

    else if (incoming.startsWith("Extracted from")) {
      String drawerStr = incoming.substring(String("Extracted from").length());
      drawerStr.trim();  
      extractedDrawerNumber = drawerStr.toInt();
      playSound(SOUND_EXTRACT_ITEM);
      openDrawer(extractedDrawerNumber);
      delay(OPEN_DURATION);
      closeDrawer(extractedDrawerNumber);
      delay(10);
    }
    else if (incoming.startsWith("Stored in ")) {
      String numberStr = incoming.substring(10);  
      numberStr.trim();
      storedDrawerNumber = numberStr.toInt();

      playSound(SOUND_STORE_ITEM);
      openDrawer(storedDrawerNumber);
      delay(OPEN_DURATION);
      closeDrawer(storedDrawerNumber);
      delay(10);
    }
    else if (incoming.startsWith("No space")) {
      
      playSound(SOUND_NO_SPACE);
      sendCommandToNextion("t0.txt=\"No Space in Your Drawer!\"");
      delay(10);
    }
    else if (incoming.startsWith("NoDrawer")) {
      playSound(SOUND_NO_DRAWER);
      sendCommandToNextion("t0.txt=\"No Drawer Available!\"");
      delay(10);
    }
    else if (incoming.startsWith("TheItem")) {
      playSound(SOUND_ITEM_NOT_FOUND);
      sendCommandToNextion("t0.txt=\"The Item Not Found!\"");
      delay(10);
    }
  }
}

// ------------------ Loop ------------------
void loop() {
  if (!systemLocked) {
  static bool lastEmergencyState = HIGH;
  bool currentEmergencyState = digitalRead(buttonPin);

  if (currentEmergencyState == LOW && lastEmergencyState == HIGH) {
    EmergencyButon(); 
  } else if (currentEmergencyState == HIGH && lastEmergencyState == LOW) {
    resetSystem();    
  }
  lastEmergencyState = currentEmergencyState;
  }
  if (isEmergency) {
    unsigned long currentTime = millis();
    if (currentTime - lastEmergencySoundTime >= emergencySoundInterval) {
      playSound(SOUND_EMERGENCY);
      lastEmergencySoundTime = currentTime;
    }

    EmergencyflashRed(RED_PIN);
    updateRedFlash(RED_PIN); 
     return;
  }
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";
      uidString += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidString.toUpperCase();

    Serial.println(uidString);

    if (uidString == validUIDs[0]) {
      Serial2.println("UID:" + uidString);
      playSound(SOUND_EMAN);
      flashGreen(GREEN_PIN);
      unlockSystem();
    } else if (uidString == validUIDs[1]) {
      Serial2.println("UID:" + uidString);
      playSound(SOUND_SAMYA);
      flashGreen(GREEN_PIN);
      unlockSystem();
    } else {
      playSound(SOUND_INVALID_CARD); // reject Card
      flashRed(RED_PIN);
      lockSystem();
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(1000); 
  }


  if (systemLocked) {
    delay(1000);
    return;
  }

   avgHeight = getAverageDistance();
   handDist = getHandDistance();
  handleNextionInput();
  listenToSerial2();
if (heightMode == "idle" && handDist <= 40 && avgHeight < MAX_TABLE_HEIGHT) {
  heightMode = "hand";
}

if (heightMode == "auto") {
  if (avgHeight < targetHeight - margin_height) {
    moveHeightUp();
  } else if (avgHeight > targetHeight + margin_height) {
    moveHeightDown();
  } else {
    stopHeightMotor();
    static int stableCount = 0;
    stableCount++;
    if (stableCount >= 5) { 
      heightMode = "idle";
      stableCount = 0;
    }
  }
}
else if (heightMode == "hand") {
  if (handDist >=10 && handDist <= 40 && avgHeight < MAX_TABLE_HEIGHT) {
    moveHeightUp();
  } else {
    stopHeightMotor();
    heightMode = "idle";
  }
}
else {
  stopHeightMotor();
}
if (logoutPendingLock && heightMode == "idle") {
    systemLocked = true;
    logoutPendingLock = false;
  }
if (tempCounter == 0) {
    float temp = bmp.readTemperature();

    if (temp > 24) {
      playSound(SOUND_TEMP_HIGH);
      tempCounter = 1;
    }
    else if (temp <= 24) {
      playSound(SOUND_TEMP_LOW);
      tempCounter = 1;
    }
  }

if (warmupReadingsSkipped < warmupReadingsLimit) {
  warmupReadingsSkipped++;
  MQ135.update();
  return;  
}

MQ135.update();
float gas_ppm = MQ135.readSensor();


  unsigned long currentTime = millis();

  if (gas_ppm > 50) {
    if (!gasDetected) {
      gasDetected = true;
    }
    if (currentTime - lastGasSoundTime >= gasSoundInterval) {
      playSound(SOUND_GAS_DETECTED);
      lastGasSoundTime = currentTime;
    }

    if (currentTime - lastFlashTime >= flashInterval) {
      flashOn = !flashOn;
      analogWrite(BLUE_PIN, flashOn ? 255 : 0);
      lastFlashTime = currentTime;
    }

  } else {
    if (gasDetected) {
      gasDetected = false;
    }

  // Turn off LED and reset timers
  analogWrite(BLUE_PIN, 0);
  lastGasSoundTime = 0;
  lastFlashTime = 0;
  flashOn = false;
}
  analogWrite(GREEN_PIN, 0);
  if (mode == 0) {
  int ldrValue = analogRead(ldrPin);
  digitalWrite(ledPin, ldrValue > 400 ? HIGH : LOW);
} else if (brightnessChanged) {
  analogWrite(ledPin, brightness);
  brightnessChanged = false;
}
  updateRedFlash(RED_PIN);
  delay(100);
}