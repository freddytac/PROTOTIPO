/*
                                UNIVERSIDAD DE CUENCA
              DESARROLLO DE UN SISTEMA IoT PARA LA ADQUISICION DE DATOS BIOMECANICOS
                    DE UN PATINADOR DE VELOCIDAD, UTILIZANDO DISPOSITIVOS ESP32
                          NOMBRES: MIGUEL BELTRAN Y FREDDY TACURI
 */
////////////////////////////////////////////////////////////////////////////
#define DEBUG 1
// LIBRERIAS
#include "ThingsBoard.h"                        // LIBRERIA THINGSBOARD
#include <esp_now.h>                            // LIBRERIA ESPNOW
#include <WiFi.h>                               // LIBRERIA WIFI
#include "time.h"                               // LIBRERIA TIME
#include <Adafruit_MPU6050.h>                   // LIBRERIA SENSOR MPU6050
#include <Adafruit_Sensor.h>                    // LIBRERIA SENSOR
#include <Wire.h>                               // LIBRERIA I2C, SDA Y SDL
#include "FS.h"                                 // LIBRERIA FS
#include "SD.h"                                 // LIBRERIA SD
#include "SPI.h"                                // LIBRERIA SPI
#include "base64.hpp"                           // LIBRERIA BASE 64

/////////////////////////////////////////////////////////////////////////////
// VARIABLES
Adafruit_MPU6050 mpu;                           // DEFINICION DEL SENSOR MPU6050
WiFiClient espClient;                           // DEFINICION DEL CLIENTE WIFI
ThingsBoardSized<256> tb(espClient);            // DEFINICION DEL CLIENTE MQTT
#define THINGSBOARD_SERVER "thingsboard.cloud"  // DIRECCION IP O LINK DEL SERVIDOR THINGSBOARD
#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))
#define TOKEN "s8eAQoTXImJLjkallS30"            // TOKEN 3
int ID = 3;                                     // IDENTIFICADOR DEL NODO
int status = WL_IDLE_STATUS;                    // ESTADO DEL RADIO WIFI
bool subscribed = false;                        // ESTADO DE LA SUSCRIPCION RPC
uint8_t broadcastAddress[] = {0x94, 0xB5, 0x55, 0x2C, 0x81, 0x48}; // MAC NODO PRINCIPAL
bool skate = false;                             // BANDERA: true=patinando, false=inactivo
int t0 = 0;                                     // TIEMPO DE INICIO DE CADA BUCLE
int arr=0;
int valorx = 0;                                 // CONDICION PARA EL REINICIO DEL NODO
bool bandera = false;                           // INDICADOR DEL PRIMER BUCLE REALIZADO
bool flag=false;
bool band=false;
const char *ssid = "Mabs";                      // SSID
const char *password = "123probando";           // CONTRASEÑA SSID
unsigned char data1[156];                       // BUFFER DE ALMACENAMIENTO TEMPORAL DATA1
int cont = 0;                                   // CONTADOR (PUNTERO DE ALMACENAMIENTO DE DATA1 EN DATAN)
char *datan;                                    // BUFFER DE ALMACENAMIENTO MICROSD DATAN
const char *ntpServer = "pool.ntp.org";         // SERVIDOR NTP
unsigned long Epoch_Time;                       // VARIABLE EPOCH_TIME
File file;                                      // CREACION DEL ARCHIVO MICROSD
unsigned char buf[150];                         // BUFFER DE LECTURA DESDE LA MICROSD
unsigned char base64[208];                      // BUFFER DE LOS CARRACTERES EN BASE 64
unsigned char *ptrbuff;
unsigned char *ptr;

// ESTRUCTURA DEL MENSAJE ESPNOW
typedef struct struct_message
{
  int id;                                       // Identificador de la tarjeta
  bool nstate;                                  // Estado del nodo
} struct_message;

struct_message espnowMessage;                   // CREACION DEL MENSAJE ESPNOW PARA EL ENVIO DE MENSAJES AL NODO PRINCIPAL
struct_message espnowMessage0;                  // CREACION DEL MENSAJE ESPNOW PARA RECIBIR LOS MENSAJES DEL NODO PRINCIPAL

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCIONES

// OBTENER EL TIEMPO ACTUAL
unsigned long Get_Epoch_Time()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return (0);
  }
  time(&now);
  return now;
}

// RECEPCION ESPNOW
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  memcpy(&espnowMessage0, incomingData, sizeof(espnowMessage0));
}

// ENVIO ESPNOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
#ifdef DEBUG
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
#endif
}

// INICIO DE LA MICROSD
void initSDCard()
{
  if (!SD.begin())
  {
#ifdef DEBUG
    Serial.println("Card Mount Failed");
#endif
    return;
  }
  uint8_t cardType = SD.cardType();
#ifdef DEBUG
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }
#endif
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
#ifdef DEBUG
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
#endif
}

// LECTURA DE ARCHIVO Y ENVIO DE DATOS
void readFile(fs::FS &fs, const char *path)
{
#ifdef DEBUG
  Serial.printf("Reading file: %s\n", path);
#endif
  File file = fs.open(path);
  if (!file)
  {
#ifdef DEBUG
    Serial.println("Failed to open file for reading");
#endif
    return;
  }
  if (file)
  {
    while (file.available())
    {
      memset(data1, 0, sizeof(data1));
      memset(buf, 0, sizeof(buf));
      file.read(buf, 150);
      memcpy(&data1[6], buf, sizeof(buf));
      union t_tag
      {
        struct
        {
          long time1;
          int time2;
        };
        unsigned char allt[6];
      } t;
      t.time1 = Epoch_Time;
      t.time2 = arr;
      memcpy(&data1[0], t.allt, sizeof(t.allt));
      unsigned int base64_length = encode_base64(data1, 156, base64);
      Serial.println(base64_length);
      datan = (char *)base64;
      tb.sendTelemetryString("array", datan);
      vTaskDelay(pdMS_TO_TICKS(125));
      arr=arr+1;
    }
    file.close();
  }
  
}

// ELIMINACION DE ARCHIVO
void deleteFile(fs::FS &fs, const char *path)
{
#ifdef DEBUG
  Serial.printf("Deleting file: %s\n", path);
#endif
  if (fs.remove(path))
  {
#ifdef DEBUG
    Serial.println("File deleted");
#endif
  }
#ifdef DEBUG
  else
  {
    Serial.println("Delete failed");
  }
#endif
}

// INICIALIZACION Y CONFIGURACION DEL SENSOR MPU6050
void accelerometer()
{
// INICIALIZACION MPU6050
  if (!mpu.begin())
  {
#ifdef DEBUG
    Serial.println("Failed to find MPU6050 chip");
#endif
    while (1)
    {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
#ifdef DEBUG
  Serial.println("MPU6050 Found!");
#endif

// CONFIGURACION DEL SENSOR
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G); // CONFIGURACION DE LA MAXIMA ACELRACION EN G
#ifdef DEBUG
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange())
  {
  case MPU6050_RANGE_2_G:
    Serial.println("+-2G");
    break;
  case MPU6050_RANGE_4_G:
    Serial.println("+-4G");
    break;
  case MPU6050_RANGE_8_G:
    Serial.println("+-8G");
    break;
  case MPU6050_RANGE_16_G:
    Serial.println("+-16G");
    break;
  }
#endif
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);      // CONFIGURACION DEL MAXIMO GIRO EN GRADOS
#ifdef DEBUG
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange())
  {
  case MPU6050_RANGE_250_DEG:
    Serial.println("+- 250 deg/s");
    break;
  case MPU6050_RANGE_500_DEG:
    Serial.println("+- 500 deg/s");
    break;
  case MPU6050_RANGE_1000_DEG:
    Serial.println("+- 1000 deg/s");
    break;
  case MPU6050_RANGE_2000_DEG:
    Serial.println("+- 2000 deg/s");
    break;
  }
#endif

  mpu.setFilterBandwidth(MPU6050_BAND_94_HZ);   // CONFIGURACION DE LA ACELERACION MAXIMA EN Hz
#ifdef DEBUG
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth())
  {
  case MPU6050_BAND_260_HZ:
    Serial.println("260 Hz");
    break;
  case MPU6050_BAND_184_HZ:
    Serial.println("184 Hz");
    break;
  case MPU6050_BAND_94_HZ:
    Serial.println("94 Hz");
    break;
  case MPU6050_BAND_44_HZ:
    Serial.println("44 Hz");
    break;
  case MPU6050_BAND_21_HZ:
    Serial.println("21 Hz");
    break;
  case MPU6050_BAND_10_HZ:
    Serial.println("10 Hz");
    break;
  case MPU6050_BAND_5_HZ:
    Serial.println("5 Hz");
    break;
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TAREAS
esp_err_t create_tasksN(void)
{
  static uint8_t ucParameterToPass;
  TaskHandle_t xHandle = NULL;
  // TAREA1: THINGSBOARD, LECTURA Y ENVIO DE DATOS.
  xTaskCreatePinnedToCore(vTaskTbN,
                          "vTaskTbN",
                          1024 * 20,
                          NULL,
                          1,
                          NULL,
                          0);
  // TAREA: ESPNOW, LECTURA DEL SENSOR MPU6050.
  xTaskCreatePinnedToCore(vTaskESP_N,
                          "vTaskESP_N",
                          1024 * 40,
                          NULL,
                          2,
                          NULL,
                          1);
}

// TAREA NUCLEO 0
void vTaskTbN(void *pvParameters)
{
  while (1)
  {
    if (!tb.connected())
    { // VERIFICA CONEXION THINGBOARD
      subscribed = false;
#ifdef DEBUG
      // Connect to the ThingsBoard
      Serial.print("Connecting to: ");
      Serial.print(THINGSBOARD_SERVER);
      Serial.print(" with token ");
      Serial.println(TOKEN);
#endif
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN))
      { // VERIFICA CONEXION CON EL TOKEN
#ifdef DEBUG
        Serial.println("Failed to connect");
#endif
        return;
      }
    }
    if (skate == true && espnowMessage0.id == 0)
    {
      file.close();
      vTaskDelay(pdMS_TO_TICKS(5));
      readFile(SD, "/data.txt");
      valorx = 1;
    }
    if (valorx == 1)
    {
      //vTaskDelay(pdMS_TO_TICKS(10));
      //file.close();
      //deleteFile(SD, "/data.txt");
      vTaskDelay(pdMS_TO_TICKS(10));
      ESP.restart();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// TAREA NUCLEO 1
void vTaskESP_N(void *pvParameters)
{
  while (1)
  {
    t0 = millis();
    if (espnowMessage0.nstate == true && espnowMessage0.id == 1)
    {
      if (bandera == false)
      {
        tb.sendTelemetryBool("t_start", true);
        file = SD.open("/data.txt", FILE_APPEND);
        bandera = true;
        skate = true;
        Epoch_Time = Get_Epoch_Time();
      }
      sensors_event_t a, g, t;
      mpu.getEvent(&a, &g, &t);
      union u_tag
      {
        struct
        {
          int16_t ax;
          int16_t ay;
          int16_t az;
          /*int16_t gx;
          int16_t gy;
          int16_t gz;*/
        };
        unsigned char all[6];
      } u;
      u.ax = int16_t((a.acceleration.x)*1000);
#ifdef DEBUG
      Serial.print(a.acceleration.x, 2);
      Serial.print(" ");
#endif
      u.ay = int16_t((a.acceleration.z)*-1000);
#ifdef DEBUG
      Serial.print(a.acceleration.z*-1, 2);
      Serial.println(" ");
#endif
      u.az = int16_t((a.acceleration.y)*-1000);
/*      u.gx = int16_t((g.gyro.x) * 1000);
#ifdef DEBUG
      Serial.print(g.gyro.x, 2);
      Serial.print(" ");
#endif
      u.gy = int16_t((g.gyro.y) * 1000);
#ifdef DEBUG
      Serial.print(g.gyro.y, 2);
      Serial.println(" ");
#endif
      u.gz = int16_t((g.gyro.z) * 1000);
      
*/ 
      
      if (file)
      { 
        file.write(u.all, sizeof(u.all));
      }
      #ifdef DEBUG
      else {
        Serial.print(F("SD Card: error on opening file"));
      }
      #endif
     
    }
    //vTaskDelay(pdMS_TO_TICKS(5));
    if (espnowMessage0.id == 0)
    {
      goto a;
    }
    vTaskDelay(pdMS_TO_TICKS(25 + t0 - millis()));
    a:;
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
#ifdef DEBUG
  Serial.begin(115200);                         // PUERTO SERIAL
#endif
  WiFi.begin(ssid, password);                   // AUTENTIFICACION RED WIFI
  while (WiFi.status() != WL_CONNECTED)
  { // VERIFICACION DE ESTADO CONECTADO
    vTaskDelay(pdMS_TO_TICKS(1000));
#ifdef DEBUG
    Serial.println("Setting as a Wi-Fi Station..");
#endif
  }
  WiFi.mode(WIFI_AP_STA);                       // CONFIGURACION COMO AP Y ESTACION WIFI
  configTime(0, 0, ntpServer);
  if (esp_now_init() != ESP_OK)
  { // VALIDACION DE INICIACION ESPNOW
#ifdef DEBUG
    Serial.println("Error initializing ESP-NOW");
#endif
    return;
  }
  esp_now_register_send_cb(OnDataSent);         // ESTADO DEL PAQUETE TRANSMITIDO
  esp_now_register_recv_cb(OnDataRecv);         // ESTADO DEL PAQUETE RECIBIDO
  esp_now_peer_info_t peerInfo;                 // EMPAREJAMIENTO ESPNOW

  // REGISTROS DE EMPAREJAMIENTO CON EL NODO PRINCIPAL
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.encrypt = false;

  // AGREGA EL EMPAREJAMIENTO
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
#ifdef DEBUG
    Serial.println("Failed to add peer");
#endif
    return;
  }
  initSDCard();                                 // INICIALIZACION MICROSD
  deleteFile(SD, "/data.txt");                  // ELIMINA ARCHIVO EN LA MICROSD
  espnowMessage.id = ID;                        // ID
  espnowMessage.nstate = true;                  // ESTADO DEL NODO
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&espnowMessage, sizeof(espnowMessage)); // RESULTADO DEL ENVIO DEL PAQUETE ESPNOW
  if (result == ESP_OK)
  {
#ifdef DEBUG
    Serial.println("Sent with success");
#endif
  }
#ifdef DEBUG
  else
  {
    Serial.println("Error sending the data");
  }
#endif
  accelerometer();                              // INICIA EL ACELEROMETRO
  create_tasksN();                              // INICIA LAS TAREAS
}

void loop() {}
