#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Stepper.h>
#include <ArduinoJson.h>  //json 형식으로 데이터 불러올라고
// #include <Servo.h>

//-------온습도 센서--------------
#define DHT_PIN 20
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);
long dhtTimer = -60000;
void checkDHT();
//----------미세먼지----------
#define DUST_PIN A10
#define DUST_LED_PIN 14
#define DUST_SAMPLING 280
#define DUST_WAITING 40
#define DUST_STOPTIME 9680
//--------2대연결----------
int reqArduino = 8;
int reqArduinoM = 7;
//--------창문모터--------
int STEPSPERREV = 512;
Stepper stepper(STEPSPERREV, 6, 8, 7, 9);
float dustValue = 0;
float dustDensityung = 0;
float dustTemp = 0.0;
bool windowFlag = false;
//--------RFID--------
#define SS_PIN 53  //RFID
#define RST_PIN 5  //RFID
MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
byte nuidPICC[4];

//--------현관,거실--------
int red_LED = 3;    //현관LED
int green_LED = 4;  //거실LED
int HC_SR = 45;     //사람감지

//--------시간--------
unsigned long time = 0;              //아두이노 켜진시간
unsigned long entranceLED_time = 0;  //비교할 시간
unsigned long DUST_time = 0;         //먼지 비교시간
int value = 0;                       //사람감지 센서 값
boolean entrance_door = false;       //도어락 열렸는지
//--------함수--------
void entranceLED(boolean in);  // 현관 LED 함수 선언
void RFID();                   //RFID
//void readDustSensor();   //미세먼지 자동창문
void openWindow();   //자동창문
void closeWindow();  //자동창문
//---------서버와 시리얼 통신 위한 전역변수와 함수--------------------
char serialBuffer[128];     // 담을 버퍼
int serialBufferIndex = 0;  // 버퍼의 크기(글자 수)
void checkSerial();              // 시리얼 체크하고 분석할 부분
void handleSerialData();         // 서버랑 통신 한 것 핸들링 하기


void setup() {
  Serial.begin(9600);

  SPI.begin();                    //SPI시작
  rfid.PCD_Init();                //RFID 시작
  for (byte i = 0; i < 6; i++) {  //RFID초기화
    key.keyByte[i] = 0xFF;
  }
  stepper.setSpeed(15);  //창문모터 스피드

  pinMode(DUST_PIN, INPUT);  //먼지새끼 A10
  pinMode(DUST_LED_PIN, OUTPUT);

  pinMode(red_LED, OUTPUT);      // 현관 LED
  pinMode(green_LED, OUTPUT);    // 거실 LED
  pinMode(HC_SR, INPUT);         //사람감지
  pinMode(reqArduino, OUTPUT);   //서보모터
  pinMode(reqArduinoM, OUTPUT);  //DC모터
  time = millis();               //아두이노 켜진시간 확인
  DUST_time = time;
}

void loop() {
  checkSerial();  //  JSOn 형식으로 서버랑 통신하기 위함.
  RFID();         //  여기서 도어락 true false 판별
  //readDustSensor();
  checkDHT();
  dust();               //먼지새끼
  if (entrance_door)    //사람 들어오면
    entranceLED(true);  //LED가능 활성화
}

void checkSerial() {
  if (Serial.available()) {                                 // 시리얼 동작하면?
    serialBuffer[serialBufferIndex] = (char)Serial.read();  // 시리얼 버퍼를 쭉 읽고..

    if (serialBuffer[serialBufferIndex] == '\n') {  // 띄어쓰기 전까지 읽으면?
      handleSerialData();                           // 통해서 풀고 핸들링하는 함수
      serialBufferIndex = 0;                        // 버퍼는 초기화 해주고
    } else {
      serialBufferIndex++;  // 아니면 글자수 하나 늘린다.
    }
  }
}

void handleSerialData() {  // 서버랑 통신해서 핸들링하는 함수 --> 서버랑 한건 앞으로 여기서 핸들링 할 수 있게 해야함...

  DynamicJsonDocument doc(128);
  deserializeJson(doc, serialBuffer);

  if (doc["type"] == "rfid") {
    if (doc["result"] == 1) {
      if (!entrance_door) {
        entrance_door = true;
        //문열림
        digitalWrite(reqArduino, 1);
      } else {
        entrance_door = false;
        digitalWrite(green_LED, LOW);
        digitalWrite(red_LED, LOW);
        //문닫힘
        digitalWrite(reqArduino, 0);
      }

    } else {
    }
  } else if (doc["type"] == "window") {
    if (doc["active"] == "open") {
      // 여기에 열리는 함수
    } else if (doc["active"] == "close") {
      // 여기에 닫히는 함수
    } else {
    }
  }
}

void dust() {
  time = millis();  //아두이노 켜진시간 확인
  if (DUST_time + 2000 <= time) {
    DUST_time = time;

    digitalWrite(DUST_LED_PIN, LOW);
    delayMicroseconds(280);
    dustValue = analogRead(DUST_PIN);
    delayMicroseconds(40);
    digitalWrite(DUST_LED_PIN, HIGH);
    delayMicroseconds(9680);
    dustDensityung = (0.17 * (dustValue * (5.0 / 1024)) - 0.1) * 1000;
    Serial.println(dustDensityung);

    DynamicJsonDocument doc(256);
    doc["messageType"] = "readDust";
    doc["dust"] = dustDensityung;
    serializeJson(doc, Serial);
    Serial.println();

    if (dustDensityung != 0) {
      if (dustDensityung <= 500 && windowFlag == false) {
        digitalWrite(reqArduinoM, 1);
        windowFlag = true;
      } else if (dustDensityung >= 500 && windowFlag == true) {
        digitalWrite(reqArduinoM, 0);
        windowFlag = false;
      }
    }
  } else
    return;
}

void RFID() {
  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;
  //카드 타입 읽음
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  //MIFARE방식이 맞는지 확인
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI && piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    return;
  }
  String Ckey = "";
  for (byte i = 0; i < 4; i++) {
    Ckey += String(rfid.uid.uidByte[i], HEX);
  }
  for (int i = 0; i < sizeof(Ckey) / sizeof(char); i++) {
    Ckey[i] = toupper(Ckey[i]);
  }

  char hexPtr[8];
  memset(hexPtr, 0, 8);
  sprintf(hexPtr, "%02X%02X%02X%02X", rfid.uid.uidByte[0], rfid.uid.uidByte[1], rfid.uid.uidByte[2], rfid.uid.uidByte[3]);

  DynamicJsonDocument doc(256);     // json 생성 하고..
  doc["messageType"] = "readRfid";  // 메시지 구독한 형식...
  doc["rfidTag"] = hexPtr;          // 16진수값
  serializeJson(doc, Serial);       // json 형식으로 파싱해서 시리얼로..
  Serial.println();                 // 구분을 위한 띄어쓰기

  //Serial.println(Ckey);

  rfid.PICC_HaltA();  //PICC 종료

  rfid.PCD_StopCrypto1();  //암호화 종료

  // if (Ckey == "3C223364")
  //   if (!entrance_door) {
  //     entrance_door = true;
  //     //문열림
  //     digitalWrite(reqArduino, 1);
  //   } else {
  //     entrance_door = false;
  //     digitalWrite(green_LED, LOW);
  //     digitalWrite(red_LED, LOW);
  //     //문닫힘
  //     digitalWrite(reqArduino, 0);
  //   }
  // else
  //   entrance_door = false;
}

void checkDHT() {
  if (millis() > dhtTimer + 60000) {
    dhtTimer = millis();
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    DynamicJsonDocument doc(256);
    doc["messageType"] = "readHumidity";
    doc["temp"] = temperature;
    doc["humidity"] = humidity;
    serializeJson(doc, Serial);
    Serial.println();
    delay(1);
  }
}

void entranceLED(boolean in) {
  int value = 0;
  time = millis();  //아두이노 켜진시간 확인

  if (in == true) {  //사람이 들어오면
    //------------------------------현관------------------------------
    value = digitalRead(HC_SR);  //인체감지 측정시작
    // Serial.print("entrance : ");
    // Serial.println(value);
    if (value == 1) {  //사람이 있으면
      entranceLED_time = time;
      if (time <= entranceLED_time + 1000)
        digitalWrite(red_LED, HIGH);
      else
        digitalWrite(red_LED, LOW);
    } else {
      if (time <= entranceLED_time + 1000)
        digitalWrite(red_LED, HIGH);
      else
        digitalWrite(red_LED, LOW);
    }
    //———————————————거실———————————————
    int Light = analogRead(A7);
    if (Light >= 800)
      digitalWrite(green_LED, LOW);
    else
      digitalWrite(green_LED, HIGH);
  }
}