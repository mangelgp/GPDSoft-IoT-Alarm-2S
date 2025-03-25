#include <Arduino.h>    
#include <WiFi.h>         //for wifi connection
#include <Preferences.h>  //to save settings on rom
#include <HTTPClient.h>   //to connect to a http server
#include <Update.h>       //to update from .bin
#include "PubSubClient.h" //for mqtt connectio
#include "time.h"         //to get date from server

#include "defs.h"         //variables de entorno

TaskHandle_t Task1;
TaskHandle_t Task2;
//TaskHandle_t Task3;

WiFiClient espClient;
PubSubClient client(espClient);
Preferences prefs;

//* Monitoreo del estado del wifi/mqtt
volatile bool firstConnection = false;
volatile bool wifiConnected = false;

//* Control de la bocina de la alarma
volatile bool alarmIsSet = false;

//* Para control del boton debounce
bool triggered = false;

//* Control de entradas de sensores tipo switch
int sensorQuantity = 1;  // Ajusta entre 1 y 6
bool sensorState[SENSOR_MAX] = {false};
bool lastSensorState[SENSOR_MAX] = {false};

//* Para control de debounce en lectura de sensores
unsigned long lastDebounceTime[SENSOR_MAX] = {0}; 

//* Para control de reconexion WiFi FREERTOS
String ssid = "SSID";
String pass = "PASS";

void setupWiFi();
void callback(char* topic, byte* payload, unsigned int length);
void publishTransmitter(const char* topicType, const char* txt);
void reconnectedMQTT();
void publishOnConnetion();
void updateFirmware(const char* firmware_url);
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
  
  //* Para control de verificacion de conexion WiFi
  static int checkCount = 0;

  for(;;) {
    
    if (firstConnection == true) {
      
      if (WiFi.status() == WL_CONNECTED) {
        
        // Print every 10 verifications
        if (checkCount % 10 == 0) {
          Serial.println("[WiFi] OK!");
        }

        wifiConnected = true;

      } else {

        wifiConnected = false;
        
        Serial.println("[WIFI] Connecting...");
        
        WiFi.begin(ssid.c_str(), pass.c_str());

        unsigned long startAttemptTime = millis();

        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
          Serial.print(".");
          vTaskDelay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("[WIFI] Connected: " + WiFi.localIP());
          wifiConnected = true;
        } else {
          Serial.println("[WIFI] FAILED");
          vTaskDelay(WIFI_RECOVER_TIME_MS);  // Espera antes de intentar otra conexión
        }
      }

      // Counter increment
      checkCount++;

      // Clear counter after 10 verifications
      if (checkCount >= 10) {
        checkCount = 0; 
      }

      vTaskDelay(1000); // Check WiFi connection every second
    }
  }
}

void setup() {
  // para entrada en modo config
  String incomingByte = "";

  // para control del modo configuracion.
  unsigned long timeOut = 15000; //15 segundos
  unsigned long timer = 0;

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
    Serial.println("[All settings]");
    Serial.println("SSID: " + ssid);
    Serial.println("PASS: " + pass);
    Serial.printf("NSEN: %d\n", sensorQuantity);
    Serial.println("\nPress 'C' to show WIFI settings");
    Serial.println("\nSend 'SSID:My_SSID' to modify actual SSID");
    Serial.println("\nSend 'PASS:My_PASS' to modify actual PASS");
    Serial.println("\nSend 'NSEN:1-6' to modify actual number of sensors");
    Serial.println("\nPress 'R' to close Config mode and star-up the main aplication");
    Serial.println("\nConfig Mode will automaticly close after 15 seconds of inactivity");
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
            // Handle error if NSEN is not an int number
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
  xTaskCreatePinnedToCore(checkWiFiConnection, "setWiFi", 10000, NULL, 3, &Task2, 0);
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
      //* Flag para control de cambio de estado
      triggered = true;
    }

    // Confirma el cambio después del tiempo de debounce
    if (((millis() - lastDebounceTime[i]) > debounceDelay) && triggered == true)  { 
      if (reading != sensorState[i]) { 
        sensorState[i] = reading;
        lastSensorState[i] = reading;
        
        // Save actual sensor value on non-volatile memory only if WiFi Connected
        if (WiFi.status() == WL_CONNECTED){
          prefs.begin(PREFS_BD, false);
          prefs.putBool(sensorPrefs[i], lastSensorState[i]);
          prefs.end();

          char msg[100]; //Buffer to save message before publishing
          snprintf(msg, sizeof(msg), "Sensor %d,%s,%s", 
            i + 1, 
            reading ? "abierto" : "cerrado", 
            stringLocalTime().c_str());
          // msg: Sensor 1,abierto,2025-03-25 12:52:34
          publishTransmitter("/event", msg);
        }

        //TODO Desarrollar funcion para accionado de la bocina.

        //TODO

        //* Flag para control de cambio de estado
        triggered = false;
      }
    }
  }
}

void setupWiFi() {

  Serial.print("\n[WiFi] Conectando a: ");
  Serial.print(ssid);

  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long startAttemptTime = millis();
  unsigned long lastDotTime = 0;
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    if (millis() - lastDotTime > 500) {
      Serial.print(".");
      firstConnection = false;
      lastDotTime = millis();  // Actualiza el tiempo para evitar "flood" de puntos
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Conectado a la red WiFi");
    Serial.print("[WiFi] Direccion IP: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    firstConnection = true;
  } else {
    ESP.restart();
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void reconnectedMQTT() {
  
  //* Intenta reconexion a MQTT solo con WiFi Connected
  if(wifiConnected == true){
    
    // tomamos el topico de defs.h
    String baseTopic = xbaseTopic;

    // cargamos ultimo estado enviado a MQTT
    prefs.begin(PREFS_BD, false);
    
    for (int i = 0; i < sensorQuantity; i++) {
      lastSensorState[i] = prefs.getBool(sensorPrefs[i]);
    }

    prefs.end();
    //

    while (!client.connected()) {

      Serial.println("[MQTT] Conectando...");
      
      String clientId = SERIAL_ID;
      clientId += String(random(0xffff), HEX);
      
      if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
        Serial.println("[MQTT] Conexion Exitosa");
        
        //Subscribe topics
        baseTopic += "/update";
        client.subscribe(baseTopic.c_str());
        baseTopic = xbaseTopic;
        baseTopic += "/query";
        client.subscribe(baseTopic.c_str());
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

  if (strstr(topic, "update") != NULL) {
    // comandos disponibles si topico contiene "update"
    if (incoming.startsWith("http://") ) {
      Serial.println("Descargando desde: " + incoming);
      updateFirmware(incoming.c_str());
    } else {
      Serial.println("No se recibio una URL");
    }

  } else if(strstr(topic, "query") != NULL) {
    // comandos disponibles si topico contiene "query"

    if (incoming == "alarm_on") {
      // Forzamos encendido de la bocina
      digitalWrite(ALARM_PIN, 1);
  
    } else if (incoming == "alarm_off"){
      // Forzamos apagado de la bocina
      digitalWrite(ALARM_PIN, 0);
  
    } else if (incoming == "alarm_en") {
      // Modo seguro activado
      
      //TODO desarrollar verificacion de estado de sensores
      //TODO Solo se debe de activar si todos los sensores estam cerrados
      //TODO Caso contrario, devuelve mensaje indicando que hay sensores abiertos
      
      alarmIsSet = true;
  
    } else if (incoming == "alarm_dis") {
      // Modo seguro desactivado
      //TODO Responder "Modo seguro desactivado"
      alarmIsSet = false;
  
    } else if (incoming == "sensor_state") {
  
      char msg[50]; //Buffer to save message before publishing
      for (int i = 0; i < sensorQuantity; i++) {
        snprintf(msg, sizeof(msg), "Sensor %d,%s,%s", 
          i + 1, 
          sensorState[i] ? "abierto" : "cerrado", 
          stringLocalTime().c_str());
        publishTransmitter("/response", msg);
      }

    } else if (incoming == "open") {
      //TODO desarrollar funcion para salida a actuador

    } else if (incoming == "close") {
      //TODO desarrollar funcion para salida a actuador

    } else if (incoming == "restart") {
      ESP.restart();
    } else if (incoming == "version") {
      publishTransmitter("/response", firmware_version);
    }
  }
}

void publishTransmitter(const char* topicType, const char* txt){
  char topic[50], msg[50]; 

  snprintf(topic, sizeof(topic), "%s%s", xbaseTopic, topicType); 
  snprintf(msg, sizeof(msg), "%s", txt); 

  Serial.println(topic);
  Serial.println(msg); 
  client.publish(topic, msg);
  delay(200);
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
      
      char msg[50]; //Buffer to save message before publishing
      snprintf(msg, sizeof(msg), "Sensor %d,%s,%s", 
        i + 1, 
        sensorState[i] ? "abierto" : "cerrado", 
        stringLocalTime().c_str());
      publishTransmitter("/event", msg);

    } else {
      Serial.println("Sin cambio de estado durante apagado");
    }
  }
  prefs.end();
  return;
}

void updateFirmware(const char* firmware_url) {

  vTaskSuspend(Task1);
  vTaskSuspend(Task2);

  HTTPClient http;
  http.begin(firmware_url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    if (Update.begin(contentLength)) {
      size_t written = Update.writeStream(*stream);
      if (written == contentLength && Update.end()) {
        Serial.println("Actualización exitosa, reiniciando...");
        ESP.restart();
      } else {
        Serial.println("Error en la actualización");
        Update.printError(Serial);
      }
    }
  } 

  vTaskResume(Task1);
  vTaskResume(Task2);

  http.end();
}