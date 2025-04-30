// kniznice
#include <WiFi.h>          	// Pripojenie k Wi-Fi sieti
#include <WebServer.h>     	// Jednoduchy HTTP server
#include <HardwareSerial.h> // Komunikacia cez UART

#define RXD2 16				// RX pin pre Serial2 
#define TXD2 17				// TX pin pre Serial2

// konfiguracia siete a stavove premenne
const char* ssid = "7578";
const char* password = "arduino1";
int buzzThreshold = 10;
bool stopOnFull = false;
int containerFill[6] = {0};

// logika a mapovanie farieb
WebServer server(80);
String detectedColors[100];
int colorCount = 0;
int colorTally[5] = {0};
int colorToPosition[5] = {1, 2, 3, 4, 5};

// ziskanie casu - pre logovanie
String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[9];
  sprintf(buffer, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  return String(buffer);
}

// poslanie mapovania na arduino
void sendColorMappingToArduino() {
  const char* colorNames[5] = {"YELLOW", "ORANGE", "RED", "PURPLE", "GREEN"};
  for (int i = 0; i < 5; i++) {
    String msg = String(colorNames[i]) + "-" + String(colorToPosition[i]);
    Serial2.println(msg);
    delay(10);
  }
}

// hlavna stranka - html, css, js
void handleRoot() {
  const char* colorNames[5] = {"YELLOW", "ORANGE", "RED", "PURPLE", "GREEN"};

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Color Sorter</title>";

  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f8f9fa; color: #333; padding: 20px; }";
  html += "h2, h3, h4 { color: #007BFF; }";
  html += "ul { list-style: none; padding-left: 0; }";
  html += "li { margin-bottom: 6px; }";
  html += "input, button { padding: 6px 10px; margin: 4px 0; border: 1px solid #ccc; border-radius: 4px; }";
  html += "button { background-color: #007BFF; color: white; border: none; cursor: pointer; }";
  html += "button:hover { background-color: #0056b3; }";
  html += "form { margin-bottom: 20px; }";
  html += "progress { width: 120px; height: 16px; vertical-align: middle; }";
  html += "div#colorLog, div#tally { margin-bottom: 20px; padding: 10px; background: #fff; border-radius: 8px; box-shadow: 0 0 6px rgba(0,0,0,0.1); }";
  html += ".container { display: flex; gap: 30px; flex-wrap: wrap; }";
  html += ".left-column, .right-column { flex: 1; min-width: 300px; }";
  html += ".left-column { max-width: 45%; }";
  html += ".right-column { max-width: 50%; }";
  html += "#colorLog ul { max-height: 240px; overflow-y: auto; padding-right: 8px; margin: 0; }";
  html += "#colorLog { height: 260px; overflow: hidden; }";
  html += "</style>";

  html += "<script>function updateLog(){fetch('/data').then(r=>r.text()).then(d=>{const logDiv=document.getElementById('colorLog');if(logDiv)logDiv.innerHTML=d;});fetch('/tally').then(r=>r.text()).then(d=>{const tallyDiv=document.getElementById('tally');if(tallyDiv)tallyDiv.innerHTML=d;});fetch('/filllevels').then(r=>r.text()).then(d=>{const fillDiv=document.getElementById('fillLevels');if(fillDiv)fillDiv.innerHTML=d;});}window.addEventListener('load',()=>{updateLog();setInterval(updateLog,1000);});</script>";

  html += "</head><body><div class='container'><div class='left-column'>";

  html += "<h2>Received Colors:</h2><div id='colorLog'><ul>";
  int start = max(0, colorCount - 10);
  for (int i = start; i < colorCount; i++) {
    html += "<li>" + String(i + 1) + ". " + detectedColors[i] + "</li>";
  }
  html += "</ul></div><p>Total received: " + String(colorCount) + "</p>";

  html += "<h3>Color Counts:</h3><ul id='tally'></ul>";

  html += "<h3>Container Fill Levels:</h3><div id='fillLevels'></div>";

  html += "<h3>Sorting Control:</h3><div style='display: flex; gap: 10px; flex-wrap: wrap;'>";
  html += "<form action='/start'><button type='submit'>Start Sorting</button></form>";
  html += "<form action='/stop'><button type='submit'>Stop Sorting</button></form>";
  html += "<form action='/clear'><button type='submit'>Clear Log</button></form>";
  html += "<form action='/export'><button type='submit'>Export Log (CSV)</button></form>";
  html += "</div>";

  html += "</div><div class='right-column'>";

  html += "<h3>Simulate Color:</h3><form action='/simulate'><input type='text' name='color' placeholder='Enter color (e.g., RED)'><button type='submit'>Submit</button></form>";

  html += "<h3>Color Sorting Mapping:</h3><form action='/setmap'>";
  html += "YELLOW: <input type='number' name='yellow' min='1' max='5' value='" + String(colorToPosition[0]) + "'><br>";
  html += "ORANGE: <input type='number' name='orange' min='1' max='5' value='" + String(colorToPosition[1]) + "'><br>";
  html += "RED: <input type='number' name='red' min='1' max='5' value='" + String(colorToPosition[2]) + "'><br>";
  html += "PURPLE: <input type='number' name='purple' min='1' max='5' value='" + String(colorToPosition[3]) + "'><br>";
  html += "GREEN: <input type='number' name='green' min='1' max='5' value='" + String(colorToPosition[4]) + "'><br>";
  html += "<button type='submit'>Save Mapping</button></form>";

  html += "<h4>Current Color Mapping:</h4><ul>";
  for (int i = 0; i < 5; i++) {
    html += "<li>" + String(colorNames[i]) + " → position " + String(colorToPosition[i]) + "</li>";
  }
  html += "</ul>";

  html += "<h3>Capacity Settings</h3><form action='/setthreshold'>Buzz threshold: <input type='number' name='threshold' min='1' max='100' value='" + String(buzzThreshold) + "'><button type='submit'>Save</button></form>";

  html += "<form action='/setstopmode' id='stopForm'><label><input type='checkbox' name='stop' value='1' onchange='document.getElementById(\"stopForm\").submit();'" + String(stopOnFull ? " checked" : "") + "> Stop sorting on full</label></form>";

  html += "<div style='position: fixed; bottom: 10px; right: 10px; font-size: 12px; color: #777;'>Color Sorter - Christian Bukai</div>";

  html += "</div></div></body></html>";

  server.send(200, "text/html", html);
}

// ovladanie triedenia a simulacia
void handleClear() {
  colorCount = 0;
  for (int i = 0; i < 5; i++) colorTally[i] = 0;
  for (int i = 1; i <= 5; i++) containerFill[i] = 0;
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "Log cleared");
}

void handleSimulate() {
  if (server.hasArg("color")) {
    String msg = server.arg("color");
    msg.trim();
    msg.toUpperCase();

    if (msg.length() > 0 && colorCount < 100) {
      detectedColors[colorCount++] = getTimestamp() + " - " + msg;

      int colorIndex = -1;
      if (msg == "YELLOW") colorIndex = 0;
      else if (msg == "ORANGE") colorIndex = 1;
      else if (msg == "RED") colorIndex = 2;
      else if (msg == "PURPLE") colorIndex = 3;
      else if (msg == "GREEN") colorIndex = 4;

      if (colorIndex != -1) {
        colorTally[colorIndex]++;
        int pos = colorToPosition[colorIndex];
        if (pos >= 1 && pos <= 5) containerFill[pos]++;
        if (containerFill[pos] >= buzzThreshold) {
          Serial2.println("BUZZ");
          if (stopOnFull) Serial2.println("stop");
        }
      }
    }
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "Simulated");
}

void handleStart() {
  Serial2.println("start");
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "Started");
}

void handleStop() {
  Serial2.println("stop");
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "Stopped");
}

// inicializacia servera
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.println(WiFi.localIP());

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  server.on("/", handleRoot);
  server.on("/clear", handleClear);
  server.on("/simulate", handleSimulate);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/data", []() {
    String html = "<ul>";
    int start = max(0, colorCount - 10);
    for (int i = start; i < colorCount; i++) {
      html += "<li>" + String(i + 1) + ". " + detectedColors[i] + "</li>";
    }
    html += "</ul>";
    server.send(200, "text/html", html);
  });
  server.on("/tally", []() {
    String html = "<ul>";
    const char* colorNames[5] = {"YELLOW", "ORANGE", "RED", "PURPLE", "GREEN"};
    for (int i = 0; i < 5; i++) {
      html += "<li>" + String(colorNames[i]) + ": " + String(colorTally[i]) + "</li>";
    }
    html += "</ul>";
    server.send(200, "text/html", html);
  });
  server.on("/filllevels", []() {
    String html = "<ul>";
    for (int i = 1; i <= 5; i++) {
      html += "<li>Container " + String(i) + ": <progress value='" + String(containerFill[i]) + "' max='" + String(buzzThreshold) + "'></progress> " + String(containerFill[i]) + " / " + String(buzzThreshold) + "</li>";
    }
    html += "</ul>";
    server.send(200, "text/html", html);
  });
  server.on("/export", []() {
    String csv = "id,timestamp,color\n";
    for (int i = 0; i < colorCount; i++) {
      int sep = detectedColors[i].indexOf(" - ");
      if (sep != -1) {
        String timestamp = detectedColors[i].substring(0, sep);
        String color = detectedColors[i].substring(sep + 3);
        csv += String(i + 1) + "," + timestamp + "," + color + "\n";
      }
    }
    server.send(200, "text/csv", csv);
  });
  server.on("/setmap", []() {
    if (server.hasArg("yellow")) colorToPosition[0] = server.arg("yellow").toInt();
    if (server.hasArg("orange")) colorToPosition[1] = server.arg("orange").toInt();
    if (server.hasArg("red")) colorToPosition[2] = server.arg("red").toInt();
    if (server.hasArg("purple")) colorToPosition[3] = server.arg("purple").toInt();
    if (server.hasArg("green")) colorToPosition[4] = server.arg("green").toInt();
    sendColorMappingToArduino();
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Mapping updated");
  });
  server.on("/setthreshold", []() {
    if (server.hasArg("threshold")) buzzThreshold = server.arg("threshold").toInt();
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Threshold updated");
  });
  server.on("/setstopmode", []() {
    stopOnFull = server.hasArg("stop");
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Stop mode updated");
  });

  server.begin();
  Serial.println("Web server started.");
  sendColorMappingToArduino();
}

// hlavna slucka
void loop() {
  server.handleClient();

  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();

    if (msg.length() > 0 && colorCount < 100) {
      int sep = msg.indexOf('-');
      String color = sep != -1 ? msg.substring(0, sep) : msg;
      color.toUpperCase();

      int colorIndex = -1;
      if (color == "YELLOW") colorIndex = 0;
      else if (color == "ORANGE") colorIndex = 1;
      else if (color == "RED") colorIndex = 2;
      else if (color == "PURPLE") colorIndex = 3;
      else if (color == "GREEN") colorIndex = 4;

      int pos = (colorIndex != -1) ? colorToPosition[colorIndex] : -1;
      if (pos >= 1 && pos <= 5) {
        if (containerFill[pos] >= buzzThreshold && stopOnFull) {
          Serial2.println("BUZZ");
          Serial2.println("stop");
          return;
        }
        detectedColors[colorCount++] = getTimestamp() + " - " + color;
        colorTally[colorIndex]++;
        containerFill[pos]++;

        Serial.println("Received: " + color);

        if (containerFill[pos] >= buzzThreshold) {
          Serial2.println("BUZZ");
        }
      }
    }
  }
}
