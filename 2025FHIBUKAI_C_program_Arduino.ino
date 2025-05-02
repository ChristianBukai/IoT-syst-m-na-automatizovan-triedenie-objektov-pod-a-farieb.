// IoT-system-na-automatizovane-triedenie-objektov-podla-farieb
// Autor: Christian Bukai
// EUBA 2025

// Kniznice
#include <Wire.h>                // I2C komunikacia
#include <Servo.h>               // ovladanie servomotorov
#include "Adafruit_TCS34725.h"   // podpora RGB senzora

// Servo pozicie a mapovania
const int POS_LOAD = 165;
const int POS_READ = 110;
const int POS_DROP = 67;
Servo feederServo;
Servo sorterServo;

// Konstanty pre farby
#define COLOR_RED     0
#define COLOR_ORANGE  1
#define COLOR_YELLOW  2
#define COLOR_GREEN   3
#define COLOR_PURPLE  4
#define COLOR_UNKNOWN -1

// Nazvy farieb a ich priradene pozicie (1–5)
const int NUM_COLORS = 5;
String colorNames[NUM_COLORS] = {"YELLOW", "ORANGE", "RED", "PURPLE", "GREEN"};
int colorPositions[NUM_COLORS] = {1, 2, 3, 4, 5};

// Mapa pozicie 1–5 na konkretny servo uhol
int positionAngles[6] = {0, 25, 55, 90, 125, 165};

// RGB senzor s nastavenim integracie
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// Stavove premenne
bool running = false;
String command = "";
String finalDetectedColor = "UNKNOWN";

// Normalizacia RGB podla C a vypocet HUE a klasifikacia farieb
String classifyColor(uint16_t r, uint16_t g, uint16_t b, uint16_t c) {
  if (c == 0) c = 1;

  // normalizacia, ziskanie relativneho pomeru RGB nezavisly od svetelnosti
  float rNorm = (float)r / c;
  float gNorm = (float)g / c;
  float bNorm = (float)b / c;

  float maxVal = max(rNorm, max(gNorm, bNorm)); // najvacsia hodnota zo zloziek
  float minVal = min(rNorm, min(gNorm, bNorm)); // najmensia hosnota zo zloziek
  float delta = maxVal - minVal;                // rozdiel medzi najjasnejsou a najtmavsou zlozkou

  float hue = 0;
  // HSV konverzia a vypocet podla toho ktora zlozka dominuje
  if (delta > 0) {
    if (maxVal == rNorm) hue = 60.0 * fmod(((gNorm - bNorm) / delta), 6);     // cervena zlozka
    else if (maxVal == gNorm) hue = 60.0 * (((bNorm - rNorm) / delta) + 2);   // zelena zlozka
    else hue = 60.0 * (((rNorm - gNorm) / delta) + 4);                        // modra zlozka
  }

  if (hue < 0) hue += 360.0;  // zabezpecenie aby hodnota HUE bola v rozsahu 0-360
  Serial.print("HUE: ");
  Serial.println(hue);

  //klasifikacia podla HUE
  if (hue >=25 && hue < 35) {
    return "YELLOW";
  } else if (hue > 5 && hue < 15 && c > 750) {
    return "ORANGE";
  } else if (hue < 7 || hue >= 357) {
    return "RED";
  } else if (hue > 7 && hue < 19) {
    return "PURPLE";
  } else if (hue > 70 && hue < 90) {
    return "GREEN";
  }
  return "UNKNOWN";
}

// Inicializacia systemu
void setup() {
  Serial.begin(9600);
  feederServo.attach(10);
  sorterServo.attach(9);

// Kontrola dostupnosti senzora
  if (!tcs.begin()) {
    Serial.println("Sensor not found. Check wiring.");
    while (1);
  }
  Serial.println("TCS34725 initialized.");
  Serial.println("Device ready.");
}

// Pomocna funkcia na trasenie podavacom
void wiggleAtDropPosition(int center, int range = 5, int repetitions = 2) {
  for (int i = 0; i < repetitions; i++) {
    feederServo.write(center + range);
    delay(250);
    feederServo.write(center - range);
    delay(250);
  }
  feederServo.write(center);
}

// Pokrocili delay s moznostou prerusenia - potrebna pri zastaveni systemu s diagnostickym vypisom na serial monitor
void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd == "stop") {
        running = false;
        Serial.println("Sorting STOPPED");
        return;
      } else if (cmd == "BUZZ") {
        tone(6, 500, 500);
      } else if (cmd == "start") {
        running = true;
        Serial.println("Sorting STARTED");
      }
    }
  }
}

// Hlavna slucka s diagnostickym vypis do serial monitoru na diagnostiku
void loop() {
  if (Serial.available()) {
    command = Serial.readStringUntil('\n');
    command.trim();

    // kontrola prikazov zo serial monitor
    if (command == "start") {
      running = true;
      Serial.println("Sorting STARTED");
    } else if (command == "stop") {
      running = false;
      Serial.println("Sorting STOPPED");
    } else if (command == "BUZZ") {
      tone(6, 500, 500);
    } else if (command.indexOf('-') != -1) {
      int sep = command.indexOf('-');
      String color = command.substring(0, sep);
      int pos = command.substring(sep + 1).toInt();
      for (int i = 0; i < NUM_COLORS; i++) {
        if (color.equalsIgnoreCase(colorNames[i])) {
          colorPositions[i] = pos;
          Serial.println("Updated mapping: " + color + " - " + String(pos));
          break;
        }
      }
    }
  }

  if (running) {
    runColorCycle();
    delay(500);
  }
}

// Hlavny cyklus triedenia
void runColorCycle() {
  if (!running) return;

  // podania cukrika
  feederServo.write(POS_LOAD);
  smartDelay(500);
  if (!running) return;

  // posunutie cukrika pod senzor
  feederServo.write(POS_READ);
  smartDelay(500);
  if (!running) return;

  // ziskanie 3 RGBC merani
  uint32_t rTotal = 0, gTotal = 0, bTotal = 0, cTotal = 0;
  for (int i = 0; i < 3; i++) {
    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);
    rTotal += r;
    gTotal += g;
    bTotal += b;
    cTotal += c;

    // upozornenie pri nizkom odraze svetla
    if (c < 100) {
      Serial.println("WARNING: Low light detected.");
      tone(6, 500, 500);
    }

    // urcenie farby z merani - vypis n serial monitor ako diagnostika
    String tempColor = classifyColor(r, g, b, c);
    Serial.print("Reading ");
    Serial.print(i + 1);
    Serial.print(": R=");
    Serial.print(r);
    Serial.print(" G=");
    Serial.print(g);
    Serial.print(" B=");
    Serial.print(b);
    Serial.print(" C=");
    Serial.print(c);
    Serial.print(" => ");
    Serial.println(tempColor);

    smartDelay(250);
    if (!running) return;
  }

  // priemer RGBC z 3 merani
  uint16_t rAvg = rTotal / 3;
  uint16_t gAvg = gTotal / 3;
  uint16_t bAvg = bTotal / 3;
  uint16_t cAvg = cTotal / 3;
  finalDetectedColor = classifyColor(rAvg, gAvg, bAvg, cAvg);
  if (!running) return;

  // odoslanie rozpoznanej farby cez serial
  if (finalDetectedColor != "UNKNOWN") {
    int colorPosition = 0;
    for (int i = 0; i < NUM_COLORS; i++) {
      if (finalDetectedColor == colorNames[i]) {
        colorPosition = colorPositions[i];
        break;
      }
    }
    if (colorPosition >= 1 && colorPosition <= 5) {
      String msg = finalDetectedColor + "-" + String(colorPosition);
      Serial.println(msg); 
    }
  }

  if (!running) return;

  // podla mapovania sa zisti cielovy kontajner
  int targetPos = 90;
  for (int i = 0; i < NUM_COLORS; i++) {
    if (finalDetectedColor == colorNames[i]) {
      int posIndex = colorPositions[i];
      if (posIndex >= 1 && posIndex <= 5) {
        targetPos = positionAngles[posIndex];
      }
      break;
    }
  }
  if (!running) return;

  // otocenia serva na triedenie - vypis na serial monitor ako diagnostika
  Serial.print("Sorting ");
  Serial.print(finalDetectedColor);
  Serial.print(" to angle ");
  Serial.println(targetPos);

  sorterServo.write(targetPos);

  smartDelay(500);
  if (!running) return;

  // vysypanie cukrika
  feederServo.write(POS_DROP);
  smartDelay(1000);
  if (!running) return;

  // zatrasenie cukrikom vratenia serva na zaciatocnu poziciu
  wiggleAtDropPosition(POS_DROP);
  smartDelay(500);
  if (!running) return;
  feederServo.write(POS_LOAD);
}