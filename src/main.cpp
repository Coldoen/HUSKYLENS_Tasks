#include<Arduino.h>
#include "HUSKYLENS.h"
#include "SoftwareSerial.h"

//Déclaration des pins du Moteurs
#define PWMA 6
#define DIRA 7
#define PWMB 9
#define DIRB 8
//Sensors ultrasonique pour mesure les distances
#define TRIG_PIN 10
#define ECHO_PIN 11

HUSKYLENS huskylens;
enum etat_e {IDLE,STOP,LABA,DANCE, SEARCH, TURN, CONFIG, READY, APPROACH};
etat_e etat;
unsigned long last_millis = 0;
int numColor = 0; // Variable globale permettant de trouver la prochaine couleur attendu

//Configuration des moteurs
void cmd_moteurs(int mG, int mD){
  analogWrite(PWMA, abs(mG));
  digitalWrite(DIRA, (mG >0));
  analogWrite(PWMB, abs(mD));
  digitalWrite(DIRB, (mD > 0));
}

//Calcul de la vitesse et de la vitesse angulaire
void cmd_robot(int linear, int angular){
  cmd_moteurs((linear + angular) >> 1, (linear - angular) >> 1);
}

//Calcul de la distance entre le robot et l'ID de la couleur
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  return pulseIn(ECHO_PIN, HIGH) * 0.017;
}

//Retourne vrai si le robot est à moins de 10 cm de l'ID de la couleur 
bool isCloseToBall() {
  return (measureDistance() < 10);
}

//Fonction permettant de passer d'un état à l'autre
void newState(etat_e new_state){
  etat = new_state;
  last_millis = millis();
}

//Calcul le temps qui passe - Descartes (1630)
bool delayPassed(int delay_ms){
  return (millis() - last_millis) >= delay_ms;
}

HUSKYLENSResult getColor(int id_color){
  HUSKYLENSResult color_return = {0, 0, 0, 0, 0, 0, 0, 0};
  if (huskylens.request()) {
    while (huskylens.available()) {
      color_return = huskylens.read();
      if (color_return.ID == id_color) break;
    }
  }
  return color_return;
}

//Valeur, formule et calcul des vitesses du robot 
void assert(HUSKYLENSResult color){
  float coef_P_lin = 1.2, coef_P_ang = -0.5;
  int co_lin = 200, co_ang = 160;
  int err_lin = co_lin - color.height;
  int err_ang = co_ang - color.xCenter;
  int out_lin = constrain((int)(err_lin * coef_P_lin), -255, 255);
  int out_ang = constrain((int)(err_ang * coef_P_ang), -255, 255);
  cmd_robot(out_lin, out_ang);
}

//Initialisation/Démarage du robot
void setup() {
  etat = CONFIG;
  pinMode(PWMA, OUTPUT);
  pinMode(DIRA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(DIRB, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.begin(115200);
  Wire.begin();
  while (!huskylens.begin(Wire)) delay(100);
}

//LE ROBOT OPERE A PARTIR D'ICI
void loop() {
  switch (etat) {
    case CONFIG:
    //Configure la caméra en roconnaissance de couleur lors du démarage, pui passe à l'état IDLE
      huskylens.writeAlgorithm(ALGORITHM_COLOR_RECOGNITION);
      newState(IDLE);
      break;
    case IDLE:
    //Si la première couleur est détecté, passe à l'état READY
      if (huskylens.request() && huskylens.available() && huskylens.read().ID == 1) newState(READY);
      break;
    case READY: 
    //Attend deux secondes puis passe à l'état LABA
      if (delayPassed(2000)) newState(LABA);
      break;
    case LABA: {
      HUSKYLENSResult color = getColor(numColor);
      if (color.ID != 0) {
        if (isCloseToBall()) {
          newState(APPROACH); //Si la balle est assez proche, passe à l'état APPROACH
        } else {
          assert(color); //Sinon, continuer d'avancer normalement
        }
      }
      //Si toutes les couleurs ont été visualisé, passe à l'état DANCE
      if (color.height >= 100 && numColor == 4) newState(DANCE);
      //Sinon incrémente la variable globale numColor de 1 et passe à l'état SEARCH
      else if (color.height >= 100) {
        numColor++;
        newState(SEARCH);
      }
      break;
    }
    case APPROACH:
    //Avance pendant 1 seconde 2 dixième à pleine balle (vous l'avez ?) afin de renverser la balle
      cmd_robot(150, 0);
      delay(1200);
      //Puis incrémenter numColor de 1 et procède à la même vérification qu'à la fin de l'état LABA
      numColor++;
      newState(numColor != 4 ? SEARCH : DANCE);
      break;
    case DANCE:
    //Etat dans lesquel le robot va alterner avec STOP lorsqu'il aura effectuer toutes ses tâches
    //Danse en tourant sur lui même pendant 1/2 seconde puis s'arrête pendant 10 secondes puis 
    //recommence
      cmd_robot(0, 500);
      if (delayPassed(1000)) newState(STOP);
      break;
    case STOP:
      cmd_robot(0, 0);
      if (delayPassed(10000)) newState(DANCE);
      break;
    case SEARCH:
    //Si la caméra ne détecte pas la couleur d'ID numColor après 1/2 seconde (incrémenté précédement dans l'état LABA)
    //Passe à l'état TURN
      cmd_robot(0, 0);
      if (!huskylens.request() || !huskylens.available() || huskylens.read().ID != numColor) {
        if (delayPassed(500)) newState(TURN);
      } else {
        //Sinon, la couleur est détecté par la caméra, le robot va LABA en passant à l'état LABA !
        newState(LABA);
      }
      break;
    case TURN:
    //Tourne vers la gauche pendant 1/2 seconde puis retourne à l'état SEARCH
      cmd_robot(0, -80);
      if (delayPassed(500)) newState(SEARCH);
      break;
  }
}
