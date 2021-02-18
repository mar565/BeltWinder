#include <Arduino.h>

#pragma GCC diagnostic ignored "-Wwrite-strings"

#include <PubSubClient.h> 
#include "FS.h"
#include <EEPROM.h>
#include <IotWebConf.h>
//Debug Level definieren
#define DEBUGLEVEL 0
#include <DebugUtils.h>



// -- Initialer Name des Gerätes. Wird auch als WLAN-SSID im AP-Modus benutzt SSID of the own Access Point.
const char thingName[] = "GW60-ESP";

// -- Initiales Passwort zum Verbinden mit dem Gerät, wenn es im AP-Modus arbeitet.
const char wifiInitialApPassword[] = "GW60-ESP";

#define STRING_LEN 128
#define NUMBER_LEN 32

// -- Konfigurations-Version. Dieser Wert sollte geändert werden, wenn sich etwas an der Konfigurations-Struktur geändert hat.
#define CONFIG_VERSION "BW_v0.1"

// -- Wenn im Bootvorgang auf LOW, startet das Gerät im AP-Modus mit dem initialen Passwort
//    z.B. wenn das Passwort vergessen wurde
#define CONFIG_PIN D0

// -- MQTT Topic Prefix
#define MQTT_TOPIC_PREFIX mqttTopicValue

// -- Callback Deklarationen.
void wifiConnected();
void configSaved();
boolean formValidator();
void mqttSend(char const* mqtt_topic,char const* mqtt_content,bool mqttRetain);
void up();
void down();
bool isNumeric(String str);
void setMaxCount();
void callback(char const* topic, byte* payload, unsigned int length);
void newPosition(int newPercentage);
void calibrationRun();
char * addIntToChar(char* text, int i);

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;
WiFiClient wifiClient;


char mqttServerValue[STRING_LEN];
char mqttPortValue[STRING_LEN];
char mqttUserValue[STRING_LEN];
char mqttPasswordValue[STRING_LEN];
char mqttTopicValue[STRING_LEN];
char mqttUpTopic[STRING_LEN];
char mqttDownTopic[STRING_LEN];
char mqttPauseTopic[STRING_LEN];
char mqttCalibrationTopic[STRING_LEN];
char mqttPositionTopic[STRING_LEN];
char mqttCountTopic[STRING_LEN];
char mqttStateTopic[STRING_LEN];
char mqttInfoTopic[STRING_LEN];
char mqttOnlineStatusTopic[STRING_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfSeparator separator2 = IotWebConfSeparator();
IotWebConfParameter mqttServer = IotWebConfParameter("MQTT Server", "mqttServer", mqttServerValue, STRING_LEN);
IotWebConfParameter mqttPort = IotWebConfParameter("MQTT Port", "mqttPort", mqttPortValue, STRING_LEN);
IotWebConfParameter mqttUser = IotWebConfParameter("MQTT User", "mqttUser", mqttUserValue, STRING_LEN);
IotWebConfParameter mqttPassword = IotWebConfParameter("MQTT Passwort", "mqttPassword", mqttPasswordValue, STRING_LEN, "password");
IotWebConfParameter mqttTopic = IotWebConfParameter("MQTT Topic-Prefix", "mqttTopic", mqttTopicValue, STRING_LEN, "text", "z.B. GW60/Testraum");


// -- MQTT
char const* mqtt_server = mqttServerValue;
char const* mqtt_username = mqttUserValue;
char const* mqtt_password = mqttPasswordValue;
char const* mqtt_clientID = iotWebConf.getThingName();
char message_buff[100];
bool mqttRetain = false;
int mqttQoS = 0;
int mqtt_port = 1883;

// -- Variablen
int dir = 0, count = 0, maxCount, newCount = 0, lastSendPercentage = 0;
float percentage = 0;
bool remote = false, needReset = false, posCertain = false, calMode = false, calUp = false, calDown = false, lastSendPercentageInit = false, newCountInit = false, maxCountInit = false, countRetrived = false, counted = false, posChange = false,posRunUp = false, posRunDown = false;
byte value;

/*
*
*Mqtt Funktionen
*
*/
PubSubClient client(mqtt_server, mqtt_port, callback, wifiClient); // -- MQTT initialisieren

boolean reconnect() {  // -- MQTT Reconnect
  boolean connected = false;
  // -- 3 Verbindungsversuche, dann Neustart
  for(int i = 0; i<2;i++){ 
    if(client.connected()) {
    connected = true;
    break;
    }
    DEBUGPRINTLN1("Versuche MQTT-Verbindung aufzubauen... ");
    // -- Versuche Neu-Verbindung
    if (client.connect(mqtt_clientID, mqtt_username, mqtt_password, mqttOnlineStatusTopic,1, true, "false")) {

      //Stellt sicher, dass ein Minimum an interaktive Topics erstellt werden
      mqttSend(mqttUpTopic, "false", mqttRetain);
      mqttSend(mqttDownTopic, "false", mqttRetain);
      mqttSend(mqttPauseTopic, "false", mqttRetain);
      mqttSend(mqttCalibrationTopic, "false", mqttRetain);
      /*
      ---toDo---
      sprintf(bufIP, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3])
      mqttSend(mqttIpTopic, wifiClient.localIP()., mqttRetain);
      */
      if(maxCountInit){
        mqttSend(mqttInfoTopic, addIntToChar("Endpositon geladen: ",maxCount), mqttRetain);
      }
      else{
        mqttSend(mqttInfoTopic, "Kalibrierung erforderlich!", mqttRetain);
      }
      // -- Re-Abonnierung
      client.subscribe(mqttPositionTopic, mqttQoS);
      client.subscribe(mqttUpTopic, mqttQoS);
      client.subscribe(mqttDownTopic, mqttQoS);
      client.subscribe(mqttPauseTopic, mqttQoS);
      client.subscribe(mqttCalibrationTopic, mqttQoS);
      client.subscribe(mqttCountTopic, mqttQoS);

      DEBUGPRINTLN1("Mqtt Initialisiert");
      DEBUGPRINTLN2("aktueller maxCount: "+String(maxCount));
    }
    else {
      DEBUGPRINTLN1("fehlgeschlagen, rc=");
      DEBUGPRINTLN1(client.state());
      DEBUGPRINTLN3(" versuche erneut");
      //Warte 1 Sekunden vor erneutem Versuch
      delay(1000);
    }
  }
  return connected;
}

//MQTT Callback Funktion
void callback(char const* topic, byte* payload, unsigned int length) {  
  int i = 0; 
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  String message = String(message_buff); // -- in String umwandeln

  DEBUGPRINTLN2("Neue Nachricht empfangen ["+String(topic)+", "+message+"]");

  //Fängt retained message auf um letzte bekannten count zu erhalten
  //wird nur 1x ausgeführt
  if(!countRetrived && strcmp(topic, mqttCountTopic) == 0){
    if(isNumeric(message)){
      int tmpCount = message.toInt();
      //stellt sinnvollen Wert sicher
      if(tmpCount >=0){
        count = tmpCount;
      }
      DEBUGPRINTLN1("count retrieved: "+String(count));
    }
    countRetrived = true;
  }
  //Positionsbefehl
  else if(strcmp(topic, mqttPositionTopic) == 0 && dir == 0 &&isNumeric(message)){
    newPosition(message.toInt());
  }
  //Topic zum kompletten hochfahren
  else if(strcmp(topic, mqttUpTopic) == 0){
    if(posRunUp || posRunDown){
      return;
    }
    if (message.equals("true")) {
      remote = false;
      up();
      mqttSend(mqttUpTopic, "false", mqttRetain);
    }
  }
  //Topic zum kompletten runterfahren
  else if(strcmp(topic, mqttDownTopic) == 0){
    if(posRunUp || posRunDown){
      return;
    }
    if (message.equals("true")) {
      remote = false;
      down();
      mqttSend(mqttDownTopic, "false", mqttRetain);
    }
  }
  //Topic zum pausieren der aktuellen Aktion
  else if(strcmp(topic, mqttPauseTopic) == 0){
    if (message.equals("true")) {
      if(posRunUp || posRunDown){
        return;
      }
      remote = false;
      if (dir == -1) {
        down();    
      } else if (dir == 1) {
        up();
      }
      mqttSend(mqttPauseTopic, "false", mqttRetain);
    }
  }
  //Topic zum Starten einer Kalibrierungsfahrt
  else if(strcmp(topic, mqttCalibrationTopic) == 0){
    if (message.equals("true")) {
      calibrationRun();
      mqttSend(mqttCalibrationTopic, "false", mqttRetain);
    }
  }
}

//Senden mit Verbindungscheck
void mqttSend(char const* mqtt_topic,char const* mqtt_content,bool mqttRetain) {  // MQTT Sende-Routine
  if (client.publish(mqtt_topic, mqtt_content, mqttRetain)) {
    DEBUGPRINTLN1("Nachricht gesendet: "+String(mqtt_topic)+", "+mqtt_content);
  }
  else if(client.connected()) {
    client.publish(mqtt_topic, mqtt_content, mqttRetain);
    DEBUGPRINTLN1("Nachricht gesendet bei 2. Versuch: "+String(mqtt_topic)+", "+String(mqtt_content));
  }
  else if(reconnect()){
    client.publish(mqtt_topic, mqtt_content, mqttRetain);
  }
  else{
    needReset = true;
    DEBUGPRINTLN1("Sendung fehlgeschlagen: "+String(mqtt_topic)+", "+String(mqtt_content));
  }
}

//Prüfen, ob der empfangene Wert nummerisch ist
boolean isNumeric(String str) {  
  if(!str){
    return false;
  }

  if (str.length() == 0) {
    return false;
  }

  boolean seenDecimal = false;

  for(unsigned int i = 0; i < str.length(); ++i) {
    if (isDigit(str.charAt(i))) {
      continue;
    }
    if (str.charAt(i) == '.') {
      if (seenDecimal) {
        return false;
      }
      seenDecimal = true;
      continue;
    }
    return false;
  }
  return true;
}


void setupMqtt(){
  // -- Setze MQTT Callback-Funktion
  client.setCallback(callback);
  
  // -- Bereite dynamische MQTT-Topics vor
  String temp = String(MQTT_TOPIC_PREFIX);
  temp += "/open";
  temp.toCharArray(mqttUpTopic, STRING_LEN);
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/close";
  temp.toCharArray(mqttDownTopic, STRING_LEN);
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/pause";
  temp.toCharArray(mqttPauseTopic, STRING_LEN);
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/state";
  temp.toCharArray(mqttStateTopic, STRING_LEN);
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/info";
  temp.toCharArray(mqttInfoTopic, STRING_LEN);
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/calibration";
  temp.toCharArray(mqttCalibrationTopic, STRING_LEN);
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/position";
  temp.toCharArray(mqttPositionTopic, STRING_LEN); 
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/alive";
  temp.toCharArray(mqttOnlineStatusTopic, STRING_LEN);
  temp = String(MQTT_TOPIC_PREFIX);
  temp += "/count";
  temp.toCharArray(mqttCountTopic, STRING_LEN);
}
/*
*
*Gurtwickler Programm
*
*/

//Funktion für hoch
void up() {
  if (dir != -1){
    if(!calMode && !posCertain){
      posRunUp = true;
    }
    //Simulierter Tastendruck "up"
    digitalWrite(D2, LOW);
    delay(300);
    digitalWrite(D2, HIGH);
    delay(500);
  }   
}

//Funktion für runter
void down() { 
  if (dir != 1){
    if(!calMode && !posCertain){
      posRunDown = true;
    }
    //Simulierter Tastendruck "down"
    digitalWrite(D1, LOW);
    delay(300);
    digitalWrite(D1, HIGH);
    delay(500);
  }
}

//Speicherung der unteren Position
void setMaxCount() {
  if(count>0){
    //Speicher count-Wert
    EEPROM.write(10, count);
    //Speicher maxCount init-Flag
    EEPROM.write(11, 19);
    EEPROM.write(12, 92);
    EEPROM.write(13, 42);
    EEPROM.commit();
    maxCount = EEPROM.read (10);

    posCertain = true;
    maxCountInit = true;
    DEBUGPRINTLN1("Neuer maximaler Count gesetzt:"+String(maxCount)+"]");
    mqttSend(mqttInfoTopic, addIntToChar("Neue maximale Position gesetzt. count: ",maxCount), mqttRetain);
  }
  else{
    mqttSend(mqttInfoTopic, addIntToChar("Maximaler Count fehlerhaft: ",count), mqttRetain);
    DEBUGPRINTLN1("Maximaler Count fehlerhaft:"+String(maxCount)+"]");
  }
}


/*
int unCounted = 0;
//Überwacht den Sensor und meldet Probleme
void sensorGuard(){
  if((digitalRead(D3) || digitalRead(D4)&&!posChange){
    bewegungOhneImpuls++;
  }
  else{
    bewegungOhneImpuls=0;
  }
  if(bewegungOhneImpuls>5){
    mqttSend(mqttInfoTopic,"Sensorfehler. Es werden keine Impulse gezählt.");
  }
}
*/

//Zählen der Positions-Impulse
void countPosition(){
  //sensorGuard();
  if (digitalRead(D5) == LOW && digitalRead(D3) == HIGH && dir == 1 && !counted) {
    count++;
    DEBUGPRINTLN3("Neuer count: " + String(count));
    if(!calMode && count > maxCount){
      count = maxCount;
    }
    counted = true; //sicherstellen, dass nur einmal pro LOW-Impuls gezählt wird
    posChange = true; //Erfassen, dass sich die Position verändert hat
    
  } 
  else if (digitalRead(D5) == LOW && digitalRead(D4) == HIGH && dir == -1 && !counted) {
    if (count > 0){
      count--;
    }
    DEBUGPRINTLN3("Neuer count: " + String(count));
    counted = true; // -- sicherstellen, dass nur einmal pro LOW-Impuls gezählt wird
    posChange = true; //Erfassen, dass sich die Position verändert hat
  } else if (digitalRead(D5) == HIGH && counted == true) {
    counted = false;
  }
}


//aktuelle Position per MQTT senden
void sendCurrentPosition(){
  //Wert nur senden, wenn geändert und stillstand
  if(!posChange || dir != 0){
    return;
  }
  //count Wert
  char ccount[4];
  itoa( count, ccount, 10 );
  mqttSend(mqttCountTopic, ccount, true);
  if(maxCountInit){
    //Prozentwert
    // -- Prozentwerte berechen
    int percentage = count / (maxCount * 0.01);
    lastSendPercentage = percentage;
    char cpercentage[5];
    itoa( percentage, cpercentage, 10 );
    mqttSend(mqttPositionTopic, cpercentage, false);
    lastSendPercentageInit = true;
  }
  posChange = false;
}

// MQTT-Übertragung der aktuellen (geänderten) Richtung
void setCurrentDirection() {
  if (digitalRead(D4) == LOW && dir != 1) {
    mqttSend(mqttStateTopic, "Schliessen", mqttRetain);
    dir = 1;
  } else if (digitalRead(D3) == LOW && dir != -1) {
    mqttSend(mqttStateTopic, "Öffnen", mqttRetain);
    dir = -1;
  } else if (digitalRead(D4) == HIGH && digitalRead(D3) == HIGH && dir != 0) {
    mqttSend(mqttStateTopic, "Inaktiv", mqttRetain);
    dir = 0;
    delay(1000);
  }
}

//Logik für Positionsanfragen in %
void newPosition(int newPercentage){
    //Filterversuch für eigene publishes --> MQTT5.0 Feature
    if(lastSendPercentageInit && lastSendPercentage == newPercentage){
      return;
    }
    //Ignoriert Eingaben, wenn Kalibrierung stattfindet, es noch keinen maxCount gibt, oder gerade eine neue Postion angefahren wird
    if(calMode || !maxCountInit || remote){
      return;
    }
    //Sanity check
    if(newPercentage<0 || newPercentage>100){
      return;
    }
    //setze neuen Soll-Wert
    newCount = int ((float (newPercentage)/100*maxCount)+0.5);
    newCountInit = true;
    if(newCount==maxCount){
      down();
    }
    else if(newCount==0){
      up();
    }
    else{
      //Prüft ob ein Endanschlag seit boot angefahren wurde
      if(!posCertain){
        //kürzeste Strecke zum neuen Punkt mit Halt bei Endstellung
        //(maxCount-Count):Strecke nach unten
        //(maxCount-NewCount):Strecke von unten zur neuen Position
        //count: Strecke nach oben
        //newCount: Stecke von oben zur neuen Position
        if((2*maxCount-count-newCount) >= count+newCount){
          up();
        }
        else{
          down();
        }
      }
      remote = true;
    }
}

//Fährt neuen Positionswert an
void moveToNewPos() {
  //Wird nicht ausgeführt wenn eine Positionsfahrt im Gange ist und eine neue Positon definiert wurde
  if(!remote || posRunUp || posRunDown){
    return;
  }
  //Stellt sicher, dass ein Wert eingegangen ist
  if(!newCountInit){
    remote = false;
    return;
  }
  if(newCount > count){
    //wenn neuer Wert größer als aktueller ist und Rollladen aktuell gestoppt, dann starten
    if(dir == 0){
      down(); // --start
      DEBUGPRINTLN2("Neuer Prozentwert unterscheidet sich, fahre runter");
    }
    //wenn aktueller Wert kleiner als neuer ist und momentan nach oben gefahren wird, dann stopp
    else if(dir == -1){
      down(); // -- stope jetzt
      DEBUGPRINTLN2("Neuer Prozentwert ist gleich, stoppe jetzt");
      remote = false;
    }
  }
  else if(newCount < count){
    // -- wenn neuer Wert kleiner als aktueller ist und Rollladen aktuell gestoppt, dann starten
    if(dir == 0){
      up(); // -- start
      DEBUGPRINTLN2("Neuer Prozentwert unterscheidet sich, fahre hoch");
    }
    // -- wenn aktueller Wert größer als neuer ist und momentan nach unten gefahren wird, dann stopp
    else if(dir == 1){
      up(); // -- stoppe jetzt
      DEBUGPRINTLN2("Neuer Prozentwert ist gleich, stoppe jetzt");
      remote = false;
    }
  }
  else{
    remote = false;
    if(dir ==1){
      up();  //stop
      DEBUGPRINTLN2("Stop newCount = count");
    }
    else if(dir ==-1){
      down();  //stop
      DEBUGPRINTLN2("Stop newCount = count");
    }
  }
}

//Initialisierungsfahrt
void calibrationRun(){
  calMode = true;
  calUp = true;
  mqttSend(mqttInfoTopic, "Kalibrierungsfaht...", mqttRetain);
  up();
}


/*
*Kalibrierung im Loop
*1. calUp -fährt ganz nach oben und nullt Zähler
*2. calDown -fährt ganz nach unten und speichert neues Maximum
*Muss nach setDir ausgeführt werden und von calibrationRun gestartet werden.
*/
void handleCalibration(){
  if(calMode){
    if(dir==0 && calUp){
      delay(1000);
      count = 0;
      calUp = false;
      calDown = true;
      down();
      delay(1000);
    }
    else if(dir==0 &&calDown){
      calDown = false;
      calMode = false;
      posRunUp = false;
      posRunDown = false;
      setMaxCount();
    }
  }
}

void handlePosCertainty(){
  if(calMode||!maxCountInit||dir!=0){
    return;
  }
  if(posRunUp){
    count = 0;
    posRunUp = false;
    posCertain = true;
    posChange = true;
    DEBUGPRINTLN1("posCertain");
    mqttSend(mqttInfoTopic, "Position eindeutig", mqttRetain);
  }
  if(posRunDown){
    count = maxCount;
    posRunDown = false;
    posCertain = true;
    posChange = true;
    DEBUGPRINTLN1("posCertain");
    mqttSend(mqttInfoTopic, "Position eindeutig", mqttRetain);
  } 
}

void setupPins(){
  // -- Definiere Pins
  pinMode(D5, INPUT);         // Impulszähler
  pinMode(D3, INPUT_PULLUP);  // Motor fährt hoch
  pinMode(D4, INPUT_PULLUP);  // Motor fährt runter
  pinMode(D1, OUTPUT);        // Taste für runter
  digitalWrite(D1, HIGH);     // Setze Taste auf HIGH
  pinMode(D2, OUTPUT);        // Taste für hoch
  digitalWrite(D2, HIGH);     // Setze Taste auf HIGH
}
/*
*
*Web-Server
*
*/

// -- Handle Anfragen zum "/"-Pfad.
void handleRoot(){
  // -- Lass IotWebConf das Captive Portal testen und verwalten.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive Portal Anfragen werden bedient.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>MQTT Parameter</title></head><body>MQTT Parameter";
  s += "<ul>";
  s += "<li>MQTT Server: ";
  s += mqttServerValue;
  s += "<li>MQTT Port: ";
  s += mqttPortValue;
  s += "<li>MQTT Topic-Prefix: ";
  s += mqttTopicValue;
  s += "<li>MQTT Info Topic: ";
  s += mqttInfoTopic;
  s += "<li>MQTT Up Topic: ";
  s += mqttUpTopic;
  s += "<li>MQTT Down Topic: ";
  s += mqttDownTopic;
  s += "<li>MQTT Pause Topic: ";
  s += mqttPauseTopic;
  s += "<li>MQTT Position Topic: ";
  s += mqttPositionTopic;
  s += "<li>MQTT Calibration Topic: ";
  s += mqttCalibrationTopic;
  s += "<li>MQTT count Topic: ";
  s += mqttStateTopic;
  s += "<li>MQTT state Topic: ";
  s += mqttCountTopic;
  s += "<li>MQTT Online Status Topic: ";
  s += mqttOnlineStatusTopic;
  s += "</ul>";
  s += "Gehe zur <a href='config'>Konfigurations-Seite</a> f&uuml;r &Auml;nderungen.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}


void configSaved(){
  DEBUGPRINTLN1("Konfiguration wurde aktuallisiert.");
  needReset = true;
}

boolean formValidator(){
  DEBUGPRINTLN1("Eingaben werden überprüft.");
  boolean valid = true;
  int s = server.arg(mqttServer.getId()).length();
  if (s < 3)
  {
    mqttServer.errorMessage = "Gib bitte einen g&uuml;ltigen Server ein.";
    valid = false;
  }
  /*int p = server.arg(mqttPort.getId()).length();
  if (p < 3)
  {
    mqttPort.errorMessage = "Gib bitte einen g&uuml;ltigen Port ein.";
    valid = false;
  }*/
  int t = server.arg(mqttTopic.getId()).length();
  if (t < 3)
  {
    mqttTopic.errorMessage = "Gib bitte ein g&uuml;ltiges Topic ein.";
    valid = false;
  }
  return valid;
}

void setupWebserver(){
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addParameter(&separator2);
  iotWebConf.addParameter(&mqttServer);
  iotWebConf.addParameter(&mqttPort);
  iotWebConf.addParameter(&mqttUser);
  iotWebConf.addParameter(&mqttPassword);
  iotWebConf.addParameter(&mqttTopic);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setupUpdateServer(&httpUpdater);
  iotWebConf.getApTimeoutParameter()->visible = true;

  // -- IotWebConf-Konfiguration initialisieren.
  iotWebConf.init();

  // -- URL-Handler auf dem Webserver aktivieren
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  //Pure Magic -> someone should look into it
  #ifdef ESP8266
    WiFi.hostname(iotWebConf.getThingName());
  #elif defined(ESP32)
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);  // This is a MUST!
    WiFi.setHostname(iotWebConf.getThingName());
  #endif
}
/*
*
*Sonstiges
*
*/

char * addIntToChar(char* text, int i){
  char * temp = new char [200];
  strcpy (temp,text);
  itoa(i, temp+strlen(temp), 10);
  return temp;
}
//Lade Werte aus den EEPROM
void loadMaxCount(){
    //Lade untere Position (maxCount) aus dem EEPROM
  maxCount = EEPROM.read (10);
  //Sanity check
  if(maxCount>0){
    DEBUGPRINTLN1("Geladene Konfig 'maxcount'= "+String(maxCount));
    //Lade checks aus dem EEPROM
    int check1 = EEPROM.read(11);
    int check2 = EEPROM.read(12);
    int check3 = EEPROM.read(13);
    if(check1 == 19 && check2 == 92 && check3 == 42){
      maxCountInit=true;
      DEBUGPRINTLN1("Geladene Konfig 'maxcount initFlag'= "+String(maxCountInit));
      DEBUGPRINTLN3("Geladene Konfig 'maxcount check1'= "+String(check1));
      DEBUGPRINTLN3("Geladene Konfig 'maxcount check2'= "+String(check2));
      DEBUGPRINTLN3("Geladene Konfig 'maxcount check3'= "+String(check3));
    }
  }
}


/*
*
*Setup & Loop
*
*/
void setup() {
  Serial.begin(115200);
  // while the serial stream is not open, do nothing:
  //Nur zum debuggen benötigt
  if(DEBUGLEVEL >0){
    while (!Serial) ;
    delay(1000);
  }
  DEBUGPRINTLN1("Start");

  setupWebserver();

  setupMqtt();

  setupPins();

  //lese gespeicherten maxCount Wert aus dem Speicher
  loadMaxCount();

  // -- So, alles erledigt... kann los gehen
  DEBUGPRINTLN1("Initialisierung abgeschlossen.");
}

void loop() {
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();

  //wenn gerade eine Konfigurations-Änderung stattgefunden hat -> ESP neu starten
  if (needReset) {
    DEBUGPRINTLN1("Reboot nach einer Sekunde.");
    iotWebConf.delay(1000);
    ESP.restart();
  }
  
  // check if MQTT is connected, if not: connect
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    if(reconnect()){
      //Set online satus
      mqttSend(mqttOnlineStatusTopic, "true", mqttRetain);
    }
    else{
      needReset = true;
      //evtl. continue? idk
      return;
    }
  }
  //MQTT loop -Befehle werden hier empfangen
  client.loop();

  //Zähle aktuelle Position
  countPosition();
  //Bestimme Aktuelle Richtung
  setCurrentDirection();

  //Stelle fest ob aktuelle Position eindeutig ist
  handlePosCertainty();

  //Aktuelle Position per Mqtt senden
  sendCurrentPosition();

  //Fahre neue Position an
  moveToNewPos();

  //Kalibrierungsfahrt ausführen
  handleCalibration();

}
