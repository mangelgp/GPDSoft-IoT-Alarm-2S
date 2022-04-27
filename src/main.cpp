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

volatile bool sensor01State = false;
volatile bool lastSensor01State = false;

// // PARA SENSOR DOBLE ...............................................................
bool sensor02State = false;                                                       //
bool lastSensor02State = false;                                                   //
// // PARA SENSOR DOBLE ...............................................................

long lastMsg = 0;
char msg[50];

unsigned int conteo = 0;
unsigned long lastDebounceTime = 0; // tiempo del ultimo cambio de estado del sensor

unsigned long timeOut = 10000;   // Tiempo máximo de espera en modo configuración (10s)
unsigned long timer = 0;

String queryTopic = xqueryTopic;
String topicToPublish = xtopicToPublish;

String incomingByte = "";
String ssid = "NETWORK_SSID";
String pass = "NETWORK_PASS";

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
  pinMode(SENSOR_01_PIN, INPUT_PULLUP);   //ENTRADA DE SENSOR 01
  // // PARA SENSOR DOBLE ..................................................
  pinMode(SENSOR_02_PIN, INPUT_PULLUP);   //ENTRADA DE SENSOR 02       //
  // // PARA SENSOR DOBLE ..................................................
  
  Serial.begin(115200);

    prefs.begin("data", false);
  ssid = prefs.getString("SSID", String(0));
  pass = prefs.getString("PASS", String(0));

  if (Serial) {
    timer = millis();
    Serial.println("Config mode");
    Serial.println("SSID: " + ssid);
    Serial.println("PASS: " + pass);
  }

  while(Serial){

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
        // Serial.print("SSID: ");
        Serial.println("SSID: " + _ssid + " OK!");
        prefs.putString("SSID",_ssid);
        ssid = _ssid;

      } else if (incomingByte.startsWith("PASS:")){
        timer = millis();
        String _pass = incomingByte.substring(5);
        // Serial.print("PASS: ");
        Serial.println("PASS: " + _pass + " OK!");
        prefs.putString("PASS",_pass);
        pass = _pass;

      } else if (incomingByte == "CONFIG?"){
        timer = millis();
        Serial.println("" + ssid + "");
        Serial.println("" + pass + "");

      } else if (incomingByte == "RESUME"){
        prefs.end();
        Serial.println("Config Mode closed");
        break;
      }
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

  // // PARA SENSOR UNICO .............................................
  // bool readingS01 = digitalRead(SENSOR_01_PIN);                   //
                                                                
  // if (readingS01 != lastSensor01State) {                          //
  //   lastDebounceTime = millis();                                  //
  // }                                                               //

  // if ( (millis() - lastDebounceTime) > debounceDelay ){           //

  //   if (readingS01 != sensor01State) {                            //

  //     sensor01State = readingS01;                                 //

  //     conteo++;                                                   //
  //     Serial.println(conteo);                                     //
  //     publishSensorState();                                       //
  //   }                                                             //
  // }                                                               //
  // lastSensor01State = readingS01;                                 //
  // // PARA SENSOR UNICO .............................................


  // // PARA SENSOR DOBLE ............................................................................
  bool readingS01 = digitalRead(SENSOR_01_PIN);                                                    //
  bool readingS02 = digitalRead(SENSOR_02_PIN);                                                    //

  if (readingS01 != lastSensor01State || readingS02 != lastSensor02State) {                      //
    lastDebounceTime = millis();                                                                 //
  }                                                                                              //

  if ( (millis() - lastDebounceTime) > debounceDelay ){                                          //

    if (readingS01 != sensor01State) {                                                           //

      sensor01State = readingS01;                                                                //

      conteo++;                                                                                  //
      Serial.println(conteo);                                                                    //

      if (sensor01State == 1) {                                                                  //
        publishTransmitter("event","Sensor01,abierto," + stringLocalTime());                     //

        // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida            //
        if (alarmIsSet == true && sensor01State == 1) {                                          //
          // digitalWrite(alarmPin, 1);                                                             //
        }                                                                                        //

      } else {                                                                                   //
        publishTransmitter("event","Sensor01,cerrado," + stringLocalTime());                     //
      }                                                                                          //
    } else if(readingS02 != sensor02State){                                                      //

      sensor02State = readingS02;                                                                //

      conteo++;                                                                                  //
      Serial.println(conteo);                                                                    //

      if (sensor02State == 1) {                                                                  //
        publishTransmitter("event","Sensor02,abierto," + stringLocalTime());                     //

        // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida            //
        if (alarmIsSet == true && sensor02State == 1) {                                          //
          // digitalWrite(alarmPin, 1);                                                             //
        }                                                                                        //

      } else {                                                                                   //
        publishTransmitter("event","Sensor02,cerrado," + stringLocalTime());                     //
      }                                                                                          //
    }                                                                                            //
  }                                                                                              //

  lastSensor01State = readingS01;                                                                //
  lastSensor02State = readingS02;                                                                //
  // // PARA SENSOR DOBLE ............................................................................
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
    
    if (sensor01State == 1) publishTransmitter("response","Sensor01,abierto");
    else publishTransmitter("response","Sensor01,cerrado");

    // // PARA SENSOR DOBLE ............................................................
    if (sensor02State == 1) publishTransmitter("response","Sensor02,abierto");     //
    else publishTransmitter("response","Sensor02,cerrado");                        //
    // // PARA SENSOR DOBLE ............................................................
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

void publishSensorState(){
  if (sensor01State == 1) {
    publishTransmitter("event","Sensor01,abierto," + stringLocalTime());

    // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida
    if (alarmIsSet == true && sensor01State == 1) {
      digitalWrite(ALARM_PIN, 1);
    }

  } else {
    publishTransmitter("event","Sensor01,cerrado," + stringLocalTime());
  }

  // // PAR SENSOR DOBLE ................................................................
  if (sensor02State == 1) {                                                         //
    publishTransmitter("event","Sensor02,abierto," + stringLocalTime());            //

    // Disparamos la bocina cuando se activa el sensor y la alarma esta encendida   //
    if (alarmIsSet == true && sensor02State == 1) {                                 //
      digitalWrite(ALARM_PIN, 1);                                                   //
    }                                                                               //

  } else {                                                                          //
    publishTransmitter("event","Sensor02,cerrado," + stringLocalTime());            //
  }                                                                                 //
  // // PARA SENSOR DOBLE ...............................................................
}

void publishOnConnetion(){
  // PARA SENSOR UNICO..................................
  // if(sensor01State) {                                 //
  //   publishSensorState();                             //
  // }                                                   //
  // PARA SENSOR UNICO..................................

  // // PARA SENSOR DOBLE..................................
  if(sensor01State || sensor02State) {                //
    publishSensorState();                             //
  }                                                   //
  // // PARA SENSOR DOBLE..................................

  return;
}