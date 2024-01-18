#include<Arduino.h>
#include "HUSKYLENS.h"
#include "SoftwareSerial.h"

//Déclaration des pins du Moteurs
#define PWMA 6
#define DIRA 7
#define PWMB 9
#define DIRB 8
#define TRIG_PIN 10
#define ECHO_PIN 11

//Sensors ultrasonique pour mesure les distances
HUSKYLENS huskylens;
enum etat_e {IDLE, STOP, LABA, DANCE, TURN_LEFT, TURN_RIGHT, SEARCH, GOBACK, READY};
etat_e etat;
unsigned long last_millis = 0;
int numColor = 1; // Variable globale permettant de trouver la prochaine couleur attendu

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
  return pulseIn(ECHO_PIN, HIGH) * 0.017; // Half the speed of sound
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

bool getColor(int expectedId) {
  if (huskylens.request()) {
    while (huskylens.available()) {
      HUSKYLENSResult color = huskylens.read();
      if (color.ID == expectedId) {
        return true;
      }
    }
  }
  return false;
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
  etat = IDLE;
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
    case IDLE:
      //Si la première couleur est détecté, passe à l'état READY
      if (getColor(1)) newState(READY);
      break;
    case READY:
      //Attend deux secondes puis passe à l'état LABA
      if (delayPassed(2000)) newState(LABA);
      break;
    case LABA:
      // Si la couleur attendue (numColor) est détectée
      if (getColor(numColor)) {
        // Tant que cette couleur est détectée, le robot avance
        while (getColor(numColor)) cmd_robot(120, 0);
        //Sinon incrémente la variable globale numColor de 1 et passe à l'état correspondant
        //à la valeur de numColor
        numColor++;
        newState(numColor == 5 ? DANCE : (numColor == 2 || numColor == 3) ? TURN_RIGHT : TURN_LEFT);
      }
      break;
    case TURN_LEFT:
      // Fait tourner le robot à gauche pendant 4 dixième de seconde puis passe à l'état SEARCH
      cmd_robot(0, -100);
      if (delayPassed(400)) newState(SEARCH);
      break;
    case TURN_RIGHT:
      // Fait tourner le robot à droite pendant 4 dixième de seconde puis passe à l'état SEARCH
      cmd_robot(0, 100);
      if (delayPassed(400)) newState(SEARCH);
      break;
    case GOBACK:
      // Fait reculer le robot vers la droite pendant 4 dixième de seconde 
      //puis passe à l'état SEARCH
      cmd_robot(-100, 100);
      if (delayPassed(400)) newState(SEARCH);
      break;
    case SEARCH:
      cmd_robot(0, 0);
      // Si la couleur attendue n'est pas détectée après 4 dixième de seconde
      if (!getColor(numColor)) {
        // Passe à  l'état correspondant à numColor
        newState(numColor == 2 ? TURN_RIGHT : numColor == 3 ? GOBACK : TURN_LEFT);
      } else {
        // Si la couleur attendue est détectée, passe à l'état LABA
        newState(LABA);
      }
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
  }
}
