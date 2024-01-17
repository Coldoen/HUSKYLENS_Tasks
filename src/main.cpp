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
    IDLE,STOP,LABA,DANCE, SEARCH, TURN, CONFIG, READY, APPROACH
    };

etat_e etat;
unsigned long last_millis = 0;
int numColor = 0; // Calculer la prochaine couleur attendu

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

//Distance à laquel le robot va s'arrêter si il est à moins de 140 pixels de la couleur
bool arrColor(HUSKYLENSResult color){

  return (color.height >= 100);
}

void accelerateToBall() {
  cmd_robot(150, 0);
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
    etat = CONFIG;
    pinMode(PWMA, OUTPUT);
    pinMode(DIRA, OUTPUT);
    pinMode(PWMB, OUTPUT);
    pinMode(DIRB, OUTPUT);
    // Ultrasonic sensor setup
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

HUSKYLENSResult getColor(int id_color){

  HUSKYLENSResult color_return;
  if(!huskylens.request()){

    color_return.ID = 1;
    return color_return;
  }
  while(huskylens.available()){

    color_return = huskylens.read();
    if(color_return.ID == id_color){

      return color_return;
    }
  }
  color_return.ID = 1;
  return color_return;
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

    case CONFIG:{

      huskylens.writeAlgorithm(ALGORITHM_COLOR_RECOGNITION);
      newState(IDLE);
    }
    break;
    

    case IDLE:{
    
      if(isMerc(1)){
        newState(READY); //Si une couleur est détecté, passe à l'état READY
      }
    }
    break;

    case READY: 

      if(dealyPassed(2000)){

        huskylens.writeAlgorithm(ALGORITHM_COLOR_RECOGNITION);
        newState(LABA); //Attend deux secondes puis passe à l'état LABA
      }
    break;
  
    case LABA: {

      HUSKYLENSResult color = getColor(numColor);
      if (color.ID != 0) {
        if (isCloseToBall()) {
          newState(APPROACH);
         } else {
          assert(color); // Continue moving towards the ball
        }
      }
  
      if (arrColor(color) && numColor == 4) {

        newState(DANCE); //Quand toutes les couleurs ont été apperçu, passe en mode DANCE

      }else if(arrColor(color) && numColor != 4) {
            
        numColor ++;
        newState(SEARCH); 
      }
    }
  break;

    case APPROACH: {
      accelerateToBall(); // Accelerate towards the ball
      delay(1200); // Give time for the robot to reach the ball (adjust as needed)
      numColor ++;
      if(numColor != 4){
      newState(SEARCH); // Switch to SEARCH mode after hitting the ball
      }else{
        newState(DANCE);
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

    case SEARCH:{
    
      cmd_robot(0, 0);
      if (!isMerc(numColor)) {
        
        if(dealyPassed(500)){
        
          newState(TURN); //Si aucune couleur n'est détecté, passe en mode TURN
        }
      }else{

        newState(LABA); // Retourne en mode LABA lorsque la bonne couleur est détecté
      }
    }
      break;  

    case TURN: {

      cmd_robot(0, -80); 
      if(dealyPassed(500)){;
        newState(SEARCH); //Repasse en mode SEARCH
      }
    }
    break;
  }
}
