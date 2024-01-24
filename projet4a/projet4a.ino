#include <SoftwareSerial.h>
#define rxPin 11            
#define txPin 10            
#define soundPin 2          // pin for the sound sensor
#define logPin 3            // pin the unser can use to get some logs on the bt serial
#define batteryPin A0       // input from a lipo cell
#define relay 4             
#define BATT_THRESHOLD 800

typedef enum {
  NORMAL_S,
  BURST,
  TRIGGER_DELAY
} Shooting_Mode;

typedef enum {
  NORMAL_A,
  LIMITED
} Ammo_Mode;

SoftwareSerial bluetooth(rxPin, txPin);

bool flag_shootDetected = false;
bool flag_logAsked = false;

Shooting_Mode shootingMode = NORMAL_S;
Ammo_Mode ammoMode = NORMAL_A;

int totalShoots = 0;
int ammoLeft = 120;
int ammoPool = 120;
int burstLenght = 0;
int currentBurstLenght = 0;
int triggerDelay_ms = 0;
long lastShootTimer = 0;
long endOfBurstTimer = 0;

bool shouldBeAbleToShoot = true;

int threeChatToAnINT(char a, char b, char c) {
  if (a < '0' || a > '9' || b < '0' || b > '9' || c < '0' || c > '9') { return -1; }  // cheack if the sequence is correct
  return (a - '0') * 100 + (b - '0') * 10 + (c - '0'); //substract the ASCII code of 0 to get the value in int
}

bool triggerDelay()
{
  return !(lastShootTimer + triggerDelay_ms >= millis());
}

bool burst()
{
  if(endOfBurstTimer + 500 >= millis())
  {
    return false;
  }
  else if (lastShootTimer + 200 <= millis())
  {
    currentBurstLenght = 0;
    return true;
  }
  else if (currentBurstLenght >= burstLenght) {
    endOfBurstTimer = millis();
    return false;
  }
  else return true;
}

void printLog() {
  bluetooth.println("===== LOG =====");
  bluetooth.print("Total Shoot: ");
  bluetooth.println(totalShoots);
  bluetooth.print("Shooting Mode: ");
  switch (shootingMode) {
    case NORMAL_S:
      bluetooth.println("Normal");
      break;
    case BURST:
      bluetooth.println("Burst");
      bluetooth.print("Burst Length: ");
      bluetooth.println(burstLenght);
      break;
    case TRIGGER_DELAY:
      bluetooth.println("Trigger Delay");
      bluetooth.print("Delay: ");
      bluetooth.println(triggerDelay_ms);
      break;
  }

  bluetooth.print("Ammo Mode: ");
  switch (ammoMode) {
    case NORMAL_A:
      bluetooth.println("Normal");
      break;
    case LIMITED:
      bluetooth.println("Limited");
      bluetooth.print("Ammo Pool: ");
      bluetooth.println(ammoPool);
      break;
  }

  bluetooth.print("Ammo Left: ");
  bluetooth.println(ammoLeft);
  bluetooth.print("Battery Voltage: ");
  bluetooth.println((float)analogRead(batteryPin) * 0.01466, 2);  //0.01466 = 5*3/1023 (Vref = 5v & 3 cell in the battery)
  bluetooth.println("==============");
  Serial.println(analogRead(batteryPin));
}

void check_BT() {
  char recivedData[32] = {}; //buffer for stocking the bluetooth data incoming
  int index = 0;

  while (bluetooth.available()) {
    recivedData[index] = (char)bluetooth.read();
    index++;
    delay(3);
  }

  if (recivedData[0] == 'R' && recivedData[1] == 'S' && recivedData[2] == 'T') {  //detect the RSTA and RSTM commands
    if (recivedData[3] == 'A') {
      ammoLeft = ammoPool;
      printLog();
    } else if (recivedData[3] == 'M') {
      shootingMode = NORMAL_S;
      ammoMode = NORMAL_A;
      printLog();
    }
  }

  else if (recivedData[0] == 'A') {
    if (recivedData[1] == 'L') {
      ammoMode = LIMITED;
      int param = threeChatToAnINT(recivedData[3], recivedData[4], recivedData[5]);
      if (param != -1) {
        ammoPool = param;
        ammoLeft = ammoPool;
      }
      printLog();
    } else if (recivedData[1] == 'N') {
      ammoMode = NORMAL_A;
      printLog();
    }
  }

  else if (recivedData[0] == 'M') {
    if (recivedData[1] == 'N') {
      shootingMode = NORMAL_S;
      printLog();
    } else if (recivedData[1] == 'B') {
      shootingMode = BURST;
      int param = threeChatToAnINT(recivedData[3], recivedData[4], recivedData[5]);
      if (param != -1) burstLenght = param;
      printLog();
    } else if (recivedData[1] == 'T') {
      shootingMode = TRIGGER_DELAY;
      int param = threeChatToAnINT(recivedData[3], recivedData[4], recivedData[5]);
      if (param != -1) triggerDelay_ms = param * 10;
      printLog();
    }
  }
}

void soundDetected() {
  if (analogRead(batteryPin) <= BATT_THRESHOLD)
    flag_shootDetected = true;
}

void logAsked() {
  flag_logAsked = true;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  pinMode(batteryPin, INPUT);
  pinMode(soundPin, INPUT);
  pinMode(relay, OUTPUT);
  pinMode(13, OUTPUT);
  bluetooth.begin(9600);
  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(soundPin), soundDetected, RISING);
  attachInterrupt(digitalPinToInterrupt(logPin), logAsked, RISING);

  printLog();
}

void loop() {
  check_BT();

  if (flag_shootDetected) {
    lastShootTimer = millis();
    bluetooth.println("===== TIR =====");
    if (ammoMode == LIMITED) {
      ammoLeft--;
      bluetooth.print("Ammo Left: ");
      bluetooth.println(ammoLeft);
      bluetooth.println("===============");
    }
    if (shootingMode == BURST) {
      currentBurstLenght++;
    }
    totalShoots++;
    flag_shootDetected = false;
  }

  if (flag_logAsked) {
    printLog();
    flag_logAsked = false;
  }

  if (ammoMode == LIMITED && ammoLeft <= 0) {
    shouldBeAbleToShoot = false;
  } else if (shootingMode == TRIGGER_DELAY) {
    shouldBeAbleToShoot = triggerDelay();
  } else if (shootingMode == BURST){
    shouldBeAbleToShoot = burst();
  }
  else shouldBeAbleToShoot = true;

  digitalWrite(relay, !shouldBeAbleToShoot);
  digitalWrite(13, shouldBeAbleToShoot);
}
