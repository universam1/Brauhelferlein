#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <PID_v1.h>
#include <ArduinoJson.h>

const char* APssid = "Brauhaus";
const char* STAssid = "uni_5";
const char* STApassword = "xxxxxx";

#define ONE_WIRE_BUS 12  // DS18B20 on ESP pin12
#define MISCHERPORT 2
#define RELAISPORT 16
float triggerC = 0.5;

#define resolution 12 // 12bit resolution == 750ms update rate
#define OWinterval (800 / (1 << (12 - resolution))) 
#define WindowSize (OWinterval * 6)
#define minWindow 150

#define OFF HIGH
#define ON LOW

#define JSONSize 150

//                    addr, en,rw,rs,d4,d5,d6,d7
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

#define CONFIG_VERSION 1
#define CONFIG_START 10
struct StoreStruct{
  uint8_t version;
  double solltemp, consKp, consKi, consKd;
} myStore;

uint32_t lastrun, lastSetpoint = 0,windowStartTime;
uint8_t mischerOff = 0;
uint8_t mischerOn = 0;
uint16_t mischerPower = 0;
bool shouldReboot = false;
double temperature = 25, Output = 0;
PID myPID(&temperature, &Output, &myStore.solltemp,myStore.consKp,myStore.consKi,myStore.consKd, DIRECT);

AsyncWebServer server(80);
AsyncEventSource events("/events"); // event source (Server-Sent events)

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
DeviceAddress tempDeviceAddress;

void loadConfig() {
  EEPROM.get(CONFIG_START, myStore);

  if (myStore.version != CONFIG_VERSION)
  {
    myStore.version = CONFIG_VERSION;
    myStore.solltemp = 24;
    myStore.consKp = 5000;
    myStore.consKi = 0;
    myStore.consKd = 0;
    saveConfig();
  }
}

void saveConfig() {
  lcd.clear(); lcd.home(); lcd.print("EE save: ");
  EEPROM.put(CONFIG_START, myStore);
  EEPROM.commit();
  lcd.print("    OK");
}

void setup(){
  Serial.begin(115200);
  Serial.println("sensors begin");

  // Start up the DS18B20
  DS18B20.begin();
  DS18B20.getAddress(tempDeviceAddress, 0);
  DS18B20.setResolution(tempDeviceAddress, resolution);
  DS18B20.setWaitForConversion(false);

  // Init the Display
  Wire.begin(4, 5);
  lcd.begin (16,2);
  lcd.backlight();
  lcd.clear();
  lcd.home(); 

  // Load PID config
  EEPROM.begin(512);
  loadConfig();

  analogWriteFreq(10000);
  digitalWrite(RELAISPORT,HIGH);
  pinMode(RELAISPORT, OUTPUT);

  digitalWrite(MISCHERPORT,HIGH);
  pinMode(MISCHERPORT, OUTPUT);

  WifiStart();
  
  // Start Web server configuration
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
    Serial.printf("Client reconnected! Last message ID that it gat is: %u\n", client->lastId());
  }});
  server.addHandler(&events);

  // respond to GET requests on URL /heap
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

// respond to GET requests on URL homepage
  server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request){

  //Check if POST (but not File) parameter exists
  if(request->hasParam("Tsoll", true)) {
    AsyncWebParameter* p = request->getParam("Tsoll", true);
    if (myStore.solltemp != p->value().toFloat()) {
      myStore.solltemp = p->value().toFloat();
      lastSetpoint=0;
    }
  }
  if(request->hasParam("Mon", true)) {
    AsyncWebParameter* p;
    p = request->getParam("Mon", true);
    mischerOn = p->value().toInt();

    p = request->getParam("Moff", true);
    mischerOff = p->value().toInt();

    p = request->getParam("MPw", true);
    mischerPower = p->value().toInt();
  }

  AsyncResponseStream *response = request->beginResponseStream("text/html");

  response->addHeader("Server","ESP Async Web Server");
  response->print(R"V0G0N(
<!DOCTYPE html>
<html>
<head>
<title>Braustube</title>
<link rel="stylesheet" href='https://fonts.googleapis.com/icon?family=Material+Icons'>
<link rel='stylesheet' href='https://code.getmdl.io/1.2.1/material.light_blue-amber.min.css'>
<script defer src='https://code.getmdl.io/1.2.1/material.min.js'></script>
<script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>
<script type="text/javascript">
var chart,gchart, allchart, data, gdata, j, alldata, iv = 1;

function enEvents() {
if (!!window.EventSource) {
var source = new EventSource("/events");
source.addEventListener("message", function(e) {
j = JSON.parse(e.data);
document.getElementById("timer").innerHTML = j.timer;
gdata.setValue(0, 1, j.temp.toFixed(2));
alldata.setValue(0, 1, j.sollT.toFixed(2));
alldata.setValue(1, 1, j.load);
data.addRow([new Date(), j.temp, j.sollT]);
while (data.getNumberOfRows() > 1000) data.removeRow(0);
chart.draw(data, {width: 800,height: 400,vAxis: {scaleType: 'allmaximized',minorGridlines: {count: 5}}});
gchart.draw(gdata, {width: 300,height: 300,min: 25,max: 100,greenFrom: 63,greenTo: 78,minorTicks: 5});
allchart.draw(alldata, {width: 500,height: 200,minorTicks: 5});}, false);}}

function send() {
var f = document.getElementById("myform"),
formData = new FormData(f),
xhr = new XMLHttpRequest();
xhr.open("POST", "/");
xhr.send(formData);
};

window.addEventListener ('submit', function (e) {
e.preventDefault();
send();
return false;})

function chartInit() {
chart = new google.visualization.LineChart(document.getElementById('chart_div'));
gchart = new google.visualization.Gauge(document.getElementById('gchart_big'));
allchart = new google.visualization.Gauge(document.getElementById('gchart_rest'));

data = new google.visualization.DataTable();
data.addColumn('datetime', 'Date');
data.addColumn('number', 'Temp');
data.addColumn('number', 'Soll');

gdata = google.visualization.arrayToDataTable([['Label', 'Value'],['Temp', 80]]);
alldata = google.visualization.arrayToDataTable([['Label', 'Value'],['Soll', 80],['Power', 30]]);

var an = document.getElementById('sliderAn');
an.oninput = function() {document.getElementById("MAan").innerHTML = this.value};

var aus = document.getElementById('sliderAus');
aus.oninput = function() {document.getElementById("MAaus").innerHTML = this.value};

var ts = document.getElementById('tsoll')
ts.oninput = function() {document.getElementById("soll").value = this.value}

var pw = document.getElementById('sliderMpw')
pw.oninput = function() {document.getElementById("Mpw").innerHTML = this.value}

var inputs = document.getElementsByClassName("mdl-slider");
for (var i = 0; i < inputs.length; i++) inputs[i].onchange = send;
enEvents();
}
google.charts.load('current', {'packages': ['corechart', 'gauge']});
google.charts.setOnLoadCallback(chartInit);

</script></head><body>
<div class="mdl-layout mdl-js-layout mdl-layout--fixed-header">
<header class="mdl-layout__header">
<div class="mdl-layout__header-row">
<span class="mdl-layout-title">Braustube</span>
<div class="mdl-layout-spacer"></div>
<nav class="mdl-navigation">
<a class="mdl-navigation__link" href="/settings">Einstellungen</a>
<a class="mdl-navigation__link" href="/update">Firmware</a></nav></div></header>
<div class="mdl-layout__drawer">
<span class="mdl-layout-title">Menu</span>
<nav class="mdl-navigation">
<a class="mdl-navigation__link" href="/settings">Einstellungen</a>
<a class="mdl-navigation__link" href="/update">Firmware</a></nav></div>
<main class="mdl-layout__content mdl-color--grey-100">
<div class="page-content">
<div class="mdl-grid">
<div class="mdl-cell mdl-cell--4-col mdl-shadow--4dp mdl-color--white" id="gchart_big"></div>
<div class="mdl-cell mdl-cell--5-col mdl-shadow--4dp mdl-color--white mdl-cell--bottom" id="gchart_rest"></div>
<form action="#" id="myform">
<table class="mdl-data-table  mdl-js-data-table mdl-cell mdl-shadow--6dp"><thead>
<tr><th>Einstellungen</th><th>Wert</th><th>Slider</th></tr></thead><tbody>
<tr><td><i class="material-icons">opacity</i> Soll Temperatur:</td>
<td><div style="width:30px" class="mdl-textfield mdl-js-textfield">
<input class="mdl-textfield__input" type="text" id="soll" name="Tsoll"></div></td>
<td><p style="width:400px"><input id="tsoll" class="mdl-slider mdl-js-slider" type="range" min="45" max="100"></p></td></tr>
<tr><td><i class="material-icons">watch_later</i> T erreicht seit:</td><td></td><td id="timer"></td></tr>
<tr><td><i class="material-icons">rotate_right</i> Mischer aktiv::</td><td id="MAan"></td>
<td><p style="width:400px"><input name="Mon" id="sliderAn" class="mdl-slider mdl-js-slider" type="range" min="0" max="20" value="0"></p></td></tr>
<tr><td><i class="material-icons">replay_30</i> Mischer pausiert:</td><td id="MAaus"></td>
<td><p style="width:400px"><input name="Moff" id="sliderAus" class="mdl-slider mdl-js-slider" type="range" min="0" max="120" value="0"></p>
<tr><td><i class="material-icons">toys</i> Mischer Drehzahl:</td><td id="Mpw"></td>
<td><p style="width:400px"><input name="MPw" id="sliderMpw" class="mdl-slider mdl-js-slider" type="range" min="0" max="100" value="100"></p>
</td></tr></td></tr></tbody></table></form>
<div class="mdl-shadow--4dp mdl-color--white mdl-cell mdl-cell--8-col">
<div id="chart_div"></div></div>
</div></div></main></div></body></html>
)V0G0N");
    request->send(response);
  });



// respond to  requests on URL /settings for PID settings
  server.on("/settings", HTTP_ANY, [](AsyncWebServerRequest *request){

  //Check if POST (but not File) parameter exists
    if(request->hasParam("Kp", true)) {
      myStore.consKp = request->getParam("Kp", true)->value().toFloat();
      myStore.consKi = request->getParam("Ki", true)->value().toFloat();
      myStore.consKd = request->getParam("Kd", true)->value().toFloat();
      myPID.SetTunings(myStore.consKp, myStore.consKi, myStore.consKd);
      resetPID();
      saveConfig();
    }

  AsyncResponseStream *response = request->beginResponseStream("text/html");

  response->addHeader("Server","ESP Async Web Server");
  response->printf(R"V0G0N(<!DOCTYPE html><html><head><title>Settings</title>
</head><body>
<h1>PID settings</h1>
Output: %-7.2f<br>
<form action='/settings' method='post' target='_self'>
kP: <input type='text' name='Kp' value="%.0f" /><br>
kI: <input type='text' name='Ki' value="%.0f" /><br>
kD: <input type='text' name='Kd' value="%.0f" /><br>
<input type='submit' value='Apply'></form>
</body></html>  )V0G0N",Output,myPID.GetKp(),myPID.GetKi(),myPID.GetKd());
    request->send(response);
  });

  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
      Serial.printf("Update Start: %s\n", filename.c_str());
      //WiFiUDP::stopAll();
      Update.runAsync(true);
      if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
        Update.printError(Serial);
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
      }
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
      }
    }
  });

  // Start the web server
  server.begin();

  DS18B20.requestTemperatures();

  windowStartTime = millis();
  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(minWindow, WindowSize);
  myPID.SetTunings(myStore.consKp, myStore.consKi, myStore.consKd);
  myPID.SetSampleTime(OWinterval);
  //turn the PID on
  resetPID();

  delay(2000);
  lcd.clear();
}

inline void relais(bool state) {
  digitalWrite(RELAISPORT,state);
}

void resetPID() {
  myPID.SetMode(MANUAL);
  Output=0;
  myPID.SetMode(AUTOMATIC);
}

void WifiStart() {
  uint8_t i=0;

  lcd.setCursor(0,0); lcd.print("Wifi STA: last");
  WiFi.begin();
  //WiFi.setAutoReconnect(true);
  lcd.setCursor(0,1);

  // We try to connect with the last known AP
  while (WiFi.status() != WL_CONNECTED)  {
    delay(500);
    i++;
    if (i%15 == 0)  {
      lcd.setCursor(0,1);
      lcd.print("                ");
      lcd.setCursor(0,1);
    } 
    else  lcd.print(".");

    // We were unable, so try the hard coded AP
    if (i == 20) {
      lcd.clear();
      lcd.setCursor(0,0);lcd.print("Wifi STA:"); lcd.print(STAssid);
      WiFi.begin(STAssid,STApassword);
      lcd.setCursor(0,1);
    }

    // No APs found, so we spin up own AP
    // TODO: When in AP mode, we have no Internet, so the JS libraries cant be loaded on the client
    // can we serve them from ESP?
    if (i == 40) {
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("Wifi AP & STA");
      WiFi.disconnect();
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(APssid);
      delay(2000);
      lcd.setCursor(0,1);lcd.print("ApIP:");lcd.print(WiFi.softAPIP());
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0,1);lcd.print("IP:");lcd.print(WiFi.localIP());
  }
}

void loop(){
  
StaticJsonBuffer<JSONSize> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();

  if(shouldReboot){
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
  
  // We run yet when we have new Temperatur data from DS18B20
  if (millis()-lastrun > OWinterval) {
    lastrun = millis();

    float tnew = DS18B20.getTempCByIndex(0);
    DS18B20.requestTemperatures();
    
    // test if we have valid temp reading
    if (tnew==DEVICE_DISCONNECTED_C || // DISCONNECTED
        (tnew==85.0 && abs(tnew - temperature) > 2 ) ) // we read 85 uninitialized but we are not close to 85 hot
    {
      Serial.println("DISCONNECTED");
      lcd.setCursor(0,0); lcd.print("-");
    }
    else
    {
      temperature = tnew;
      char buf[10];
      uint32_t passedT = 0;
      
      // calculate the time we are set in
      if (lastSetpoint != 0) passedT = (millis() - lastSetpoint) / 1000;
      else if ( abs(temperature-myStore.solltemp) < triggerC) // or we just reached it
      {
        lastSetpoint = millis();
        // reset the PID error here
        resetPID();
      }

      // myPID.Compute();

      if(millis() - windowStartTime > WindowSize) 
      { //time to shift the Relay Window and recalculate 
        windowStartTime = millis(); 
        myPID.Compute(); 
      } 
      

      snprintf (buf, sizeof(buf), "%03d:%02d", passedT/60, passedT%60);
      root["timer"] = jsonBuffer.strdup(buf);
      lcd.setCursor(10,1); lcd.print(buf);

      Serial.print("T: ");  Serial.println(temperature);
      snprintf (buf, sizeof(buf), "%6.2f", temperature);
      lcd.setCursor(0,0); lcd.print("T:"); lcd.print(buf); lcd.print((char)223);
      root["temp"] = temperature;
      
      snprintf (buf, sizeof(buf), "%6.2f", myStore.solltemp);
      lcd.setCursor(0,1); lcd.print("S:"); lcd.print(buf); lcd.print((char)223);
      root["sollT"] = myStore.solltemp;

      int8_t load = (Output-minWindow)*100.0 / (WindowSize-minWindow);
      snprintf (buf, sizeof(buf), "%3u%%", load);
      lcd.setCursor(12,0); 
      lcd.print(buf);
      root["load"] = load;

      // We send JSON data to the client
      char buffer[JSONSize];
      root.printTo(buffer, sizeof(buffer));
      events.send(buffer, "message");
    }
  }

  // Translation of Power to Relay interval
  if(Output > minWindow && Output > millis() - windowStartTime) relais(ON);
  else relais(OFF);
  
  // Mischer interval calculation
  static uint32_t lastmischer;
  if (millis()-lastmischer > 1000) {
    lastmischer = millis();
    if (mischerOn && (millis() / 1000) % (mischerOn + mischerOff) < mischerOn) {
      analogWrite(MISCHERPORT, 10.23 * (100-mischerPower)); }
    else {
      digitalWrite(MISCHERPORT, true);
    }
  }
}