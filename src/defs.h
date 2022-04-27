/*
definition file for 2 sensor IoT Alarm @GPDSoft IoT Alarm 2S
last modified: 25 april 2022.
*/

#ifndef defs_h
#define defs_h

//* DEV
// #define WIFI_NETWORK "red_test"
// #define WIFI_PASSWORD "abcd1234"

//*ZARAGOZA
// #define WIFI_NETWORK "ALFHA68DB_2.4"
// #define WIFI_PASSWORD "CYGFCGeC43"

// //*8 MAYO
// #define WIFI_NETWORK "IZZI-BC00"
// #define WIFI_PASSWORD "C863FC2EBC00"

// //*HOME
// #define WIFI_NETWORK "IZZI-B654b"
// #define WIFI_PASSWORD "2C9569A4B654"

// //*PGOSA
// #define WIFI_NETWORK "IZZI-986C"
// #define WIFI_PASSWORD "F8F5329C986C"

#define WIFI_TIMEOUT_MS 10000       // 10 seconds WiFi connection timeout
#define WIFI_RECOVER_TIME_MS 20000  // Wait 10 seconds after a failed connection attempt

#define MQTT_SERVER "vpsgpdsoft.ddns.net"
#define MQTT_PORT 1883 
#define MQTT_USER "emqx"
#define MQTT_PASS "public"

// ID del dispositivo
// #define SERIAL_ID "esp32_Zaragoza"
// #define SERIAL_ID "esp32_8mayo"
#define SERIAL_ID "esp32_8Mayo"

// MODIDIFICAR DEPEDIENDO LA SUCURSAL ................................................
#define xtopicToPublish "TEST/alarm/"                                               //
#define xqueryTopic "TEST/alarm/query"                                             //
// ...................................................................................


/*
DOUBLE SENSOR CONFIG
P25 NA
P26 -> S02
P27 -> S01

SINGLE SENSOR CONFIG
P27 -> NA
P26 -> S01
P25 -> NA
*/

// pin de conexion del sensor 01 digital
#define SENSOR_01_PIN 27

// pin de conexion del sensor 02 digital
#define SENSOR_02_PIN 26

// pin de conexion del sensor 03 digital
// #define SENSOR_03_PIN 25

// pin de disparo de la alarma auditiva
#define ALARM_PIN 15

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600;
const int daylightOffset_sec = 3600;

const unsigned long debounceDelay = 200;   // tiempo de filtrado, incrementar si hay oscilaciones (ms)

#endif