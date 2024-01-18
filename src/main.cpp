#include<Arduino.h>
#include "HUSKYLENS.h"
#include "SoftwareSerial.h"

//MOTEURS
#define PWMA 6
#define DIRA 7
#define PWMB 9
#define DIRB 8

// Ultrasonic Sensor Pins
#define TRIG_PIN 10
#define ECHO_PIN 11

int err_I = 0;
HUSKYLENS huskylens;

enum etat_e {
    IDLE,STOP,LABA,DANCE, TURN_LEFT, TURN_RIGHT, SEARCH, GOBACK, READY
    };

etat_e etat;
unsigned long last_millis = 0;
int numColor = 1; // Calculer la prochaine couleur attendu

//Configuration des moteurs
void cmd_moteurs(int mG, int mD){

  analogWrite(PWMA, (mG > 0)? mG : -mG);
  digitalWrite(DIRA, (mG >0));
  analogWrite(PWMB, (mD > 0)? mD : -mD);
  digitalWrite(DIRB, (mD > 0));
  }

//Calcul de la vitesse et de la vitesse angulaire
  void cmd_robot(int linear, int angular){

    int mG = (linear) + (angular) >> 1;
    int mD = (linear) - (angular) >> 1;
    cmd_moteurs(mG, mD);

  }

// Code de la caméra
void printResult(HUSKYLENSResult result){
    if (result.command == COMMAND_RETURN_BLOCK){
        Serial.println(String()+F("Block:xCenter=")+result.xCenter+F(",yCenter=")+result.yCenter+F(",width=")+result.width+F(",height=")+result.height+F(",ID=")+result.ID);
    }
    else if (result.command == COMMAND_RETURN_ARROW){
        Serial.println(String()+F("Arrow:xOrigin=")+result.xOrigin+F(",yOrigin=")+result.yOrigin+F(",xTarget=")+result.xTarget+F(",yTarget=")+result.yTarget+F(",ID=")+result.ID);
    }
    else{
        Serial.println("Object unknown!");
    }
}

//Fonction prennant une id de couleur et retourne 1 si elle est détecté à la caméra
int isMerc(int id_color1){

  if(!huskylens.request()){

    return 0;
  }

  while(huskylens.available()){

    HUSKYLENSResult color = huskylens.read();

    if(color.ID == id_color1){

      return 1;
    }
  }
  return 0;
}

void accelerateToBall() {
  cmd_robot(120, 0);
}

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  float duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and return)
  return distance;
}

bool isCloseToBall() {
  float distance = measureDistance();
  return (distance < 30); // Example threshold in centimeters
}

//GRAPH (Fonction permettant de passer d'un état à l'autre)
void newState(etat_e new_state){

  etat = new_state;
  last_millis = millis();
  Serial.println(etat);
}

//Calcul le temps qui passe - Descartes (1630)
bool dealyPassed(int dealy_ms){

  return((millis() - last_millis) >= dealy_ms);
}


//HUSKYLENS green line >> SDA; blue line >> SCL
void printResult(HUSKYLENSResult result);

void setup() {
    etat = IDLE;
    pinMode(PWMA, OUTPUT);
    pinMode(DIRA, OUTPUT);
    pinMode(PWMB, OUTPUT);
    pinMode(DIRB, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    Serial.begin(115200);
    Wire.begin();
    while (!huskylens.begin(Wire))
    {
        Serial.println(F("Begin failed!"));
        Serial.println(F("1.Please recheck the \"Protocol Type\" in HUSKYLENS (General Settings>>Protocol Type>>I2C)"));
        Serial.println(F("2.Please recheck the connection."));
        delay(100);
    }
}

// Check if the expected color ID is detected
bool getColor(int expectedId) {
  if (!huskylens.request()) {
    return false;
  }

  while (huskylens.available()) {
    HUSKYLENSResult color = huskylens.read();
    if (color.ID == expectedId) {
      return true;  // Expected color ID found
    }
  }
  return false;  // Expected color ID not found
}


  //Valeur, formule et calcul des vitesses du robot 
  void assert(HUSKYLENSResult color){

    float coef_P_lin = 1.2; //Tester
    float coef_P_ang = -0.5;//Tester 
    
    int in_lin = color.height;
    int in_ang = color.xCenter;

    int co_lin = 200; //Entre 180 et 200
    int co_ang = 160;
 
    int err_lin = co_lin - in_lin;
    int err_ang = co_ang - in_ang;

    int out_lin = (int) ((float)(err_lin) * coef_P_lin );
    int out_ang = (int)((float)(err_ang) * coef_P_ang);

    out_lin = min(max(out_lin, -255), 255);
    out_ang = min(max(out_ang, -255), 255);

    cmd_robot(out_lin, out_ang);
  }

//LE REOBOT OPERE A PARTIR D'ICI
void loop() {

  switch (etat) {

    case IDLE:{
    
      if(isMerc(1)){
        newState(READY); //Si une couleur est détecté, passe à l'état READY
      }
    }
    break;

    case READY: {

      if(dealyPassed(2000)){

        huskylens.writeAlgorithm(ALGORITHM_COLOR_RECOGNITION);
        newState(LABA); //Attend deux secondes puis passe à l'état LABA
      }
    }
    break;
  
    case LABA: {
  if (getColor(numColor)) {
    // Expected color ID is detected
    while (getColor(numColor)) {
      accelerateToBall(); // Accelerate towards the ball
    }  
    numColor++;
    if (numColor == 5) {
      newState(DANCE);
    } else if (numColor == 2) {
      newState(TURN_RIGHT);
    } else if (numColor == 3) {
      newState(TURN_RIGHT);
    } else if (numColor == 4) {
      newState(TURN_LEFT);
    }
  }
}
break;

    case TURN_LEFT: {

      cmd_robot(0, -100); 
      if(dealyPassed(400))
        newState(SEARCH); //Repasse en mode LABA
      }
    break;

    case TURN_RIGHT: {

      cmd_robot(0, 100); 
      if(dealyPassed(400)){
        newState(SEARCH); //Repasse en mode LABA
      }
    }
    break;

    case GOBACK: {

      cmd_robot(-100, 100); 
      if(dealyPassed(400)){
        newState(SEARCH); 
      }
    }
    break;

     case SEARCH:{
    
      cmd_robot(0, 0);
      if (!isMerc(numColor) && numColor == 2) {
        if(dealyPassed(400)){
          newState(TURN_RIGHT);
        }
        }else if (!isMerc(numColor) && numColor == 3){
        if(dealyPassed(400)){
          newState(GOBACK);
        }
      }else if (!isMerc(numColor) && numColor == 4){
        if(dealyPassed(400)){
          newState(TURN_LEFT);
        }
      }else{
        newState(LABA); // Retourne en mode LABA lorsque la bonne couleur est détecté
      }
    }
      break;  
 
    case DANCE:{

      cmd_robot(0, 500);
      if (dealyPassed(1000)) {

        newState(STOP); // Arrêter de danser après deux dixième de seconde
      }
    }
    break;

    case STOP:{

      cmd_robot(0, 0);
      if(dealyPassed(10000)){ // Attend 10 secondes avant de retourner en mode Dance
      
        newState(DANCE);
      }
    }  
    break;
  }
}