#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "PubSubClient.h"
#include "time.h"

#include "defs.h"

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

WiFiClient espClient;
PubSubClient client(espClient);
Preferences prefs;

volatile bool firstConnection = false;
volatile bool wifiConnected = false;
volatile bool alarmIsSet = false;

int sensorQuantity = 1;  // Ajusta entre 1 y 6
bool sensorState[SENSOR_MAX] = {false};
bool lastSensorState[SENSOR_MAX] = {false};
bool triggered = false;

long lastMsg = 0;
char msg[50];

unsigned int conteo = 0;
unsigned long lastDebounceTime[SENSOR_MAX] = {0}; // tiempo del ultimo cambio de estado del sensor

unsigned long timeOut = 10000;   // Tiempo máximo de espera en modo configuración (10s)
unsigned long timer = 0;

String queryTopic = xqueryTopic;
String topicToPublish = xtopicToPublish;

String incomingByte = "";
String ssid = "SSID";
String pass = "PASS";

void setupWiFi();
void callback(char* topic, byte* payload, unsigned int length);
void publishTransmitter(String topicType,String txt);
void reconnectedMQTT();
void publishSensorState();
void publishOnConnetion();
String stringLocalTime();

void visualIndicator(void *parameter){
  for(;;){

    if (wifiConnected == true){
      
      if(client.connected()){
        digitalWrite(LED_BUILTIN, 1);
        delay(100/portTICK_PERIOD_MS);
        digitalWrite(LED_BUILTIN, 0);
        vTaskDelay(5000/portTICK_PERIOD_MS);
      } else {
        digitalWrite(LED_BUILTIN, 1);
        vTaskDelay(100/portTICK_PERIOD_MS);
        digitalWrite(LED_BUILTIN, 0);
        vTaskDelay(300/portTICK_PERIOD_MS);
        digitalWrite(LED_BUILTIN, 1);
        vTaskDelay(100/portTICK_PERIOD_MS);
        digitalWrite(LED_BUILTIN, 0);
        vTaskDelay(3000/portTICK_PERIOD_MS);
      }
    } else{
      digitalWrite(LED_BUILTIN, 1);
      vTaskDelay(50/portTICK_PERIOD_MS);
      digitalWrite(LED_BUILTIN, 0);
      vTaskDelay(200/portTICK_PERIOD_MS);
    }
  }
}

void checkWiFiConnection(void *parameter){
  for(;;){

    if(firstConnection == true){

      if(WiFi.status() == WL_CONNECTED){
        Serial.println("[WiFi] OK!");
        wifiConnected = true;
        vTaskDelay(10000);
        continue;
      }

      wifiConnected = false;
      Serial.println("[WIFI] Connecting...");
      WiFi.begin(ssid.c_str(), pass.c_str());

      vTaskDelay(WIFI_TIMEOUT_MS);

      if(WiFi.status() != WL_CONNECTED){
        Serial.println("[WIFI] FAILED");
        vTaskDelay(WIFI_RECOVER_TIME_MS);
        continue;
      }

      Serial.println("[WIFI] Connected: " + WiFi.localIP());
      wifiConnected = true;
      vTaskDelay(1000);
    }
  }  
}

void setup() {

  pinMode(BUILTIN_LED, OUTPUT);       //INDICADOR DE CONEXION
  pinMode(ALARM_PIN, OUTPUT);          //SALIDA DE ALARMA

  Serial.begin(115200);

  prefs.begin(PREFS_BD, false);
  ssid = prefs.getString(PREFS_SSID, String(0));
  pass = prefs.getString(PREFS_PASS, String(0));
  sensorQuantity = prefs.getInt(PREFS_NSEN, 1);

  if (Serial) {
    timer = millis();
    Serial.println("NOTICE: You are in Config mode");
    Serial.println("[WIFI settings]");
    Serial.println("SSID: " + ssid);
    Serial.println("PASS: " + pass);
    Serial.println("\nPress 'C' to show WIFI settings");
    Serial.println("\nSend 'SSID:My_SSID' to modify actual SSID");
    Serial.println("\nSend 'PASS:My_PASS' to modify actual PASS");
    Serial.println("\nSend 'NSEN:1-6' to modify actual number of sensors");
    Serial.println("\nPress 'R' to close Config mode and star-up the main aplication");
    Serial.println("\nConfig Mode will automaticly close after 10 seconds of inactivity");
  }

  while(Serial){

    //
    if ((millis() - timer) > timeOut){
      prefs.end();
      Serial.println("Config Mode closed");
      break;
    }

    if (Serial.available() > 0){
      incomingByte = Serial.readString();
      Serial.print("Data Received: ");
      Serial.println(incomingByte);

      if (incomingByte.startsWith("SSID:")){
        timer = millis();
        String _ssid = incomingByte.substring(5);
        Serial.println("SSID: " + _ssid + " OK!");
        prefs.putString(PREFS_SSID, _ssid);
        ssid = _ssid;

      } else if (incomingByte.startsWith("PASS:")){
        timer = millis();
        String _pass = incomingByte.substring(5);
        Serial.println("PASS: " + _pass + " OK!");
        prefs.putString(PREFS_PASS, _pass);
        pass = _pass;

      } else if (incomingByte == "C"){
        timer = millis();
        Serial.println("" + ssid + "");
        Serial.println("" + pass + "");

      } else if (incomingByte.startsWith("NSEN:")) {
        timer = millis();
        String subStr = incomingByte.substring(5);
        if (subStr.length() > 0 && subStr.toInt() != 0) {
          sensorQuantity = subStr.toInt();
          prefs.putInt(PREFS_NSEN, sensorQuantity);
          Serial.println("NSEN: " + subStr + " OK!");
        } else {
            // Manejar error, subStr no es un número válido
            Serial.println("Error: Ingrese un numero del 1 al 6");
        }
      } else if (incomingByte == "R"){
        prefs.end();
        Serial.println("Config Mode closed");
        break;
      }
    }

    // config all sensorPins as input
    for (int i = 0; i < sensorQuantity; i++) {
      pinMode(sensorPins[i], INPUT_PULLUP);
    }

  }

  setupWiFi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  xTaskCreate(visualIndicator, "Task1", 1000, NULL, 1, &Task1);
  xTaskCreatePinnedToCore(checkWiFiConnection, "setWiFi", 10000, NULL, 3, &Task3, 0);
}

void loop() {

  if(!client.connected()){
    reconnectedMQTT();
  }

  if(WiFi.status() != WL_CONNECTED) {
    client.disconnect();
    wifiConnected = false;
  }

  client.loop();

  for (int i = 0; i < sensorQuantity; i++) {
    bool reading = digitalRead(sensorPins[i]);

    if ((reading != lastSensorState[i]) && triggered == false) {
      lastDebounceTime[i] = millis(); // Reinicia el temporizador
      triggered = true;
    }

    if (((millis() - lastDebounceTime[i]) > debounceDelay) && triggered == true)  { // Confirma el cambio después del tiempo de debounce
      if (reading != sensorState[i]) { 
        sensorState[i] = reading;
        lastSensorState[i] = reading;
        

        // Save actual value on non-volatile memory
        if (WiFi.status() == WL_CONNECTED){
          prefs.begin(PREFS_BD, false);
          prefs.putBool(sensorPrefs[i], lastSensorState[i]);
          prefs.end();

          publishTransmitter("event", "Sensor " + String(i + 1) + (reading ? ",abierto," : ",cerrado,") + stringLocalTime());
        }

        triggered = false;
      }
    }
  }
/*
  // LECTURA DE ESTADO DE SENSORES ............
  // bool readingS01 = digitalRead(SENSOR_01_PIN);
  // bool readingS02 = digitalRead(SENSOR_02_PIN);
  //bool readingS03 = digitalRead(SENSOR_03_PIN);

  if ( ( (readingS01 != lastSensor01State) || (readingS02 != lastSensor02State) ) && (triggered == false) ) {
    lastDebounceTime = millis();
    triggered = true;
    // Serial.println("readings != lastSensorState");
  }

  if ( ( (millis() - lastDebounceTime) > debounceDelay ) && triggered == true ){
    
    // Serial.println("(millis - lastDebounceTime) > debounceDelay");
    // triggered = false;

    if (readingS01 != lastSensor01State)
    {

      triggered = false;
      sensor01State = readingS01;
      lastSensor01State = readingS01;

      conteo++;
      Serial.println(conteo);   
      
      if (WiFi.status() == WL_CONNECTED){
        prefs.begin(PREFS_BD, false);
        prefs.putBool(PREFS_S01, lastSensor01State);
        prefs.end();
      }

      if (readingS01 == 1) {
        publishTransmitter("event","Sensor01,abierto," + stringLocalTime());

        // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida
        if (alarmIsSet == true && readingS01 == 1) {
          // digitalWrite(alarmPin, 1);
        }

      } else {
        publishTransmitter("event","Sensor01,cerrado," + stringLocalTime());
      }

    } else if(readingS02 != lastSensor02State)
    {

      triggered = false;
      sensor02State = readingS02;
      lastSensor02State = readingS02;

      conteo++;
      Serial.println(conteo);     
      
      if (WiFi.status() == WL_CONNECTED) {
        prefs.begin(PREFS_BD, false);
        prefs.putBool(PREFS_S02, lastSensor02State);
        prefs.end();
      }

      if (readingS02 == 1) {
        publishTransmitter("event","Sensor02,abierto," + stringLocalTime());

        // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida
        if (alarmIsSet == true && readingS02 == 1) {
          // digitalWrite(alarmPin, 1);
        }

      } else {
        publishTransmitter("event","Sensor02,cerrado," + stringLocalTime());
      }

    } 
  } 
  */
}

void setupWiFi() {

  delay(10);
  Serial.println();
  Serial.print("[WiFi] Conectando a: ");
  Serial.print(ssid);

  WiFi.begin(ssid.c_str(), pass.c_str());

  int timeout = 0;

  while (WiFi.status() != WL_CONNECTED) {
    firstConnection = false;
    delay(500);
    Serial.print(".");
    timeout++;
    if(timeout >= 10){
      ESP.restart();
    }
  }

  firstConnection = true;
  Serial.println();
  Serial.println("[WiFi] Conectado a la red WiFi");
  Serial.print("[WiFi] Direccion IP: ");
  Serial.println(WiFi.localIP());
  wifiConnected = true;

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  delay(1000);

}

void reconnectedMQTT() {

  if(wifiConnected == true){

    prefs.begin(PREFS_BD, false);

    for (int i = 0; i < sensorQuantity; i++) {
      lastSensorState[i] = prefs.getBool(sensorPrefs[i]);
    }
    prefs.end();

    while (!client.connected()) {

      char topic[30];
      Serial.println("[MQTT] Conectando...");
      String clientId = SERIAL_ID;
      clientId += String(random(0xffff), HEX);
      
      if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
        Serial.println("[MQTT] Conexion Exitosa");
        //Subscribe topics
        queryTopic.toCharArray(topic,30);
        client.subscribe("device/cmd");
        client.subscribe(topic);
        publishOnConnetion();

      } else {
        Serial.print("[MQTT] error: ");
        Serial.println(client.state());
        delay(5000);
        return;

      }
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  String incoming = "";
  Serial.print("Mensaje de: ");
  Serial.println(topic);
  
  for (int i = 0; i < length; i++) {
    incoming += (char)payload[i];
  }

  incoming.trim();
  Serial.println("Mensaje -> " + incoming);

  // aqui colocamos las acciones para mensajes especiales
  if (incoming == "alarm_on") {
    digitalWrite(ALARM_PIN, 1);

  } else if (incoming == "alarm_off"){
    digitalWrite(ALARM_PIN, 0);

  } else if (incoming == "alarm_en") {
    alarmIsSet = true;

  } else if (incoming == "alarm_dis") {
    alarmIsSet = false;

  } else if (incoming == "sensor_state") {
    for (int i = 0; i < sensorQuantity; i++) {
      publishTransmitter("event", "Sensor " + String(i + 1) + (sensorState[i] ? ",abierto," : ",cerrado,") + stringLocalTime());
    }
  } else if (incoming == "open") {
    // bool dato = 1;
    // prefs.begin(PREFS_BD, false);
    // prefs.putBool(PREFS_S01, dato);
    // prefs.putBool(PREFS_S02, dato);
    // //prefs.putBool(PREFS_S03, dato);
    // Serial.println("Sensores abiertos");
    // prefs.end();

  } else if (incoming == "close") {
    // bool dato = 0;
    // prefs.begin(PREFS_BD, false);
    // prefs.putBool(PREFS_S01, dato);
    // prefs.putBool(PREFS_S02, dato);
    // //prefs.putBool(PREFS_S03, dato);
    // Serial.println("Sensores cerrados");
    // prefs.end();

  } else if (incoming == "restart") {
    ESP.restart();
  }
}

void publishTransmitter(String topicType, String txt){
  char topic[30];
  String topicString;
  topicString = topicToPublish + topicType;

  Serial.print(String(txt));
  txt.toCharArray(msg, 50);
  
  Serial.println(" => "  + String(topicString));
  
  topicString.toCharArray(topic,30);

  client.publish(topic, msg);
}

String stringLocalTime(){
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "NO_TIME";
  }
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  String asString(timeStringBuff);
  return asString;
}

/*
void publishSensorState(){

  if (sensor01State == 1) {
    publishTransmitter("event","Sensor01,abierto," + stringLocalTime());

    // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida
    if (alarmIsSet == true && sensor01State == 1) {
      // digitalWrite(ALARM_PIN, 1);
    }

  } else {
    publishTransmitter("event","Sensor01,cerrado," + stringLocalTime());
  }

  if (sensor02State == 1) {
    publishTransmitter("event","Sensor02,abierto," + stringLocalTime());

    // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida
    if (alarmIsSet == true && sensor02State == 1) {
      // digitalWrite(ALARM_PIN, 1);
    }

  } else {
    publishTransmitter("event","Sensor02,cerrado," + stringLocalTime());
  }

  // if (sensor03State == 1) {
  //   publishTransmitter("event","Sensor03,abierto," + stringLocalTime());

  //   // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida
  //   if (alarmIsSet == true && sensor03State == 1) {
  //     // digitalWrite(ALARM_PIN, 1);
  //   }

  // } else {
  //   publishTransmitter("event","Sensor03,cerrado," + stringLocalTime());
  // }
} */

void publishOnConnetion(){

  for (int i = 0; i < sensorQuantity; i++) {  // read al sensors and save
    sensorState[i] = digitalRead(sensorPins[i]);
  }

  prefs.begin(PREFS_BD, false);

  for (int i = 0; i < sensorQuantity; i++) {  // check changes on sensors during shutdown
    if( (sensorState[i] != lastSensorState[i]))
    {
      prefs.putBool(sensorPrefs[i], sensorState[i]);
      Serial.println("Cambio de estado durante apagado");
      publishTransmitter("event", "Sensor " + String(i + 1) + (sensorState[i] ? ",abierto," : ",cerrado,") + stringLocalTime()); // Publish latest sensorState

    } else {
      Serial.println("Sin cambio de estado durante apagado");
    }
  }
  prefs.end();
  return;
}