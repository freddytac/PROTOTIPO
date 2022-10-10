/* UNIVERSIDAD DE CUENCA
 *  NOMBRES: MIGUEL BELTRAN Y FREDDY TACURI
*/
////////////////////////////////////////////////////////////////////////////
#define DEBUG 1
//LIBRERIAS
#include "ThingsBoard.h"                                          // 
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "FS.h"                                                   // LIBRERIAS MICROSD
#include "SD.h"
#include "SPI.h"

String data0 = "";
//TaskHandle_t Tarea;

/////////////////////////////////////////////////////////////////////////////
//VARIABLES

#define TOKEN               "YQpHgNCFvaO5ocZPAJNO"                // TOKEN 1 


#define THINGSBOARD_SERVER  "thingsboard.cloud"                   // DIRECCION IP O LINK                                                                                       
WiFiClient espClient;                                             // DEFINICION DEL CLIENTE WIFI
//ThingsBoard tb(espClient);                                      // DEFINICION DEL CLIENTE MQTT 
ThingsBoardSized<128> tb(espClient);
int status = WL_IDLE_STATUS;                                      // ESTADO DEL RADIO WIFI
Adafruit_MPU6050 mpu;                                             // DEFINICION DEL SENSOR MPU6050
bool subscribed=false; 
uint8_t broadcastAddress[] = {0x78, 0xE3, 0x6D, 0x10, 0x30, 0xB8};// MAC NODO PRINCIPAL
//bool letsgo=false;                                              // INICIO DE CARRERA
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
bool skate=false;                                                 // Bandera: true=patinando, false=inactivo
String dataMessage;                                               // Texto a almacenar
int Time=0;                                                       // Marca de tiempo
int ID=1;                                                         // Numero de NODO              
int t0=0;                                                         // Tiempo de inicio de cada bucle
int t1=0;                                                         // Tiempo de ejecucion de cada bucle
int tiempop=0;
int counter=1;
float vart, varx, vary, varz;
const int data_items = 4;  
int valorx = 0;
bool bandera=false;
const char* ssid = "Mabs";
const char* password = "123probando";

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//MENSAJES ESPNOW
typedef struct struct_message {                                   // Estructura del mensaje
    int id;                                                       // Identificador de la tarjeta
    bool nstate;                                                  // Estado del nodo
} struct_message;

struct_message espnowMessage;                                     // Creacion del mensaje espnow
struct_message espnowMessage0;                                    // Creacion del mensaje espnow0

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//FUNCIONES
// RECIBE ESPNOW
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&espnowMessage0, incomingData, sizeof(espnowMessage0));
}

// ENVIA ESPNOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#ifdef DEBUG
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
#endif
}


///--------------------------------------------------------------------------------///
//MICROSD
void initSDCard(){
  if (!SD.begin()) {
    #ifdef DEBUG
    Serial.println("Card Mount Failed");
    #endif
    return;
  }
  uint8_t cardType = SD.cardType();
  #ifdef DEBUG
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  #endif
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  #ifdef DEBUG
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  #endif
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  #ifdef DEBUG
  Serial.printf("Listing directory: %s\n", dirname);
  #endif
  
  File root = fs.open(dirname);
  if(!root){
    #ifdef DEBUG
    Serial.println("Failed to open directory");
    #endif
    return;
  }
  
  if(!root.isDirectory()){
    #ifdef DEBUG
    Serial.println("Not a directory");
    #endif
    return;
  }
  
  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      #ifdef DEBUG
      Serial.print("  DIR : ");
      Serial.println(file.name());
      #endif
      
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    }
    #ifdef DEBUG 
    else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    #endif
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path){
  #ifdef DEBUG
  Serial.printf("Creating Dir: %s\n", path);
  #endif
  if(fs.mkdir(path)){
    #ifdef DEBUG
    Serial.println("Dir created");
    #endif
  } 
  #ifdef DEBUG
  else {
    Serial.println("mkdir failed");
  }
  #endif
}

void removeDir(fs::FS &fs, const char * path){
  #ifdef DEBUG
  Serial.printf("Removing Dir: %s\n", path);
  #endif
  if(fs.rmdir(path)){
    #ifdef DEBUG
    Serial.println("Dir removed");
    #endif
  } 
  #ifdef DEBUG
  else {
    Serial.println("rmdir failed");
  }
  #endif
}

void readFile(fs::FS &fs, const char * path){
  #ifdef DEBUG
  Serial.printf("Reading file: %s\n", path);
  #endif
  File file = fs.open(path);
  if(!file){
    #ifdef DEBUG
    Serial.println("Failed to open file for reading");
    #endif
    return;
  }

  while(file.available()){
    char character = file.read();
    if (character != ',' && character != '\n'){
      data0.concat(character);
    }
    else{
      switch(counter){
        case 1:
          vart=data0.toInt();
          Serial.print("Time: ");
          Serial.print(data0);  
          break;
        case 2:
          varx=data0.toFloat();
          Serial.print(" ax: ");
          Serial.print(data0);
          break;
        case 3:
          vary=data0.toFloat();
          Serial.print(" ay: ");
          Serial.print(data0);
          break;
        case 4:
          varz=data0.toFloat();
          Serial.print(" az: ");
          Serial.println(data0);
          break;
        }       
      counter++;
      data0 = "";
    }   
    if(character == '\n'){
      Telemetry datos[data_items] = {
        { "Time", vart},
        { "ax", varx},
        { "ay", vary},
        { "az", varz},
      };
      tb.sendTelemetry(datos, data_items);
      delay(213);
      counter=1;
    }    
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  #ifdef DEBUG
  Serial.printf("Writing file: %s\n", path);
  #endif
  File file = fs.open(path, FILE_WRITE);
  if(!file){
    #ifdef DEBUG
    Serial.println("Failed to open file for writing");
    #endif
    return;
  }
  if(file.print(message)){
    #ifdef DEBUG
    Serial.println("File written");
    #endif
  } 
  #ifdef DEBUG
  else {
    Serial.println("Write failed");
  }
  #endif
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  #ifdef DEBUG
  Serial.printf("Appending to file: %s\n", path);
  #endif
  File file = fs.open(path, FILE_APPEND);
  if(!file){
    #ifdef DEBUG
    Serial.println("Failed to open file for appending");
    #endif
    return;
  }
  if(file.print(message)){
    #ifdef DEBUG
    Serial.println("Message appended");
    #endif
  } 
  #ifdef DEBUG
  else {
    Serial.println("Append failed");
  }
  #endif
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  #ifdef DEBUG
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  #endif
  if (fs.rename(path1, path2)) {
    #ifdef DEBUG
    Serial.println("File renamed");
    #endif
  } 
  #ifdef DEBUG
  else {
    Serial.println("Rename failed");
  }
  #endif
}

void deleteFile(fs::FS &fs, const char * path){
  #ifdef DEBUG
  Serial.printf("Deleting file: %s\n", path);
  #endif
  if(fs.remove(path)){
    #ifdef DEBUG
    Serial.println("File deleted");
    #endif
  } 
  #ifdef DEBUG
  else {
    Serial.println("Delete failed");
  }
  #endif
}

void accelerometer(){
  if (!mpu.begin()) {                                             // INICIALIZACION MPU6050
    #ifdef DEBUG
    Serial.println("Failed to find MPU6050 chip");
    #endif
    while (1) {
      delay(10);
    }
  }
  #ifdef DEBUG
  Serial.println("MPU6050 Found!");
  #endif
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  #ifdef DEBUG
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
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
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  #ifdef DEBUG
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange()) {
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

  mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);
  #ifdef DEBUG
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth()) {
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

esp_err_t create_tasksN(void)
{
  static uint8_t ucParameterToPass;
  TaskHandle_t xHandle=NULL;

  xTaskCreatePinnedToCore(vTaskTbN,
             "vTaskTbN",
             1024*5,
             &ucParameterToPass,
             1,
             &xHandle,
             0);
  xTaskCreatePinnedToCore(vTaskESP_N,
             "vTaskESP_N",
             1024*10,
             &ucParameterToPass,
             1,
             &xHandle,
             1);     
}

void vTaskTbN(void *pvParameters)
{
  while (1)
  {

  if (!tb.connected()) {                                          // VERIFICA CONEXION THINGBOARD
    subscribed = false;
    #ifdef DEBUG
    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    #endif
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {                 // VERIFICA CONEXION CON EL TOKEN
      #ifdef DEBUG
      Serial.println("Failed to connect");
      #endif
      return;
    }
  }
  
  if(skate==true && espnowMessage0.id==0){
    tb.sendTelemetryBool("t_start", true);
    readFile(SD, "/data.txt");
    valorx=1;
    skate=false;
    }
      
  if(valorx==1){ 
    ESP.restart();
    }  
    
  vTaskDelay(pdMS_TO_TICKS(5)); 
  }
}

void vTaskESP_N(void *pvParameters)
{
  while (1)
  {
  t0=millis();
  if(espnowMessage0.nstate==true && espnowMessage0.id==1){
    if(bandera==false){
      tiempop=millis();
      bandera=true;
      skate=true;
      }
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    dataMessage = (millis()-tiempop) + a.acceleration.z + a.acceleration.y + a.acceleration.x;
    appendFile(SD, "/data.txt", dataMessage.c_str());
    vTaskDelay(pdMS_TO_TICKS(100+t0-millis())); 
  }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);                                           // PUERTO SERIAL          

  WiFi.begin(ssid, password);                                     // AUTENTIFICACION RED WIFI
 
  while (WiFi.status() != WL_CONNECTED) {                         // VERIFICACION DE ESTADO CONECTADO
    delay(100);
    #ifdef DEBUG
    Serial.println("Setting as a Wi-Fi Station..");
    #endif
  }
  
  WiFi.mode(WIFI_AP_STA);                                         // CONFIGURACION COMO AP Y ESTACION WIFI

  if (esp_now_init() != ESP_OK) {                                 // VALIDACION DE INICIACION ESPNOW
    #ifdef DEBUG
    Serial.println("Error initializing ESP-NOW");
    #endif
    return;
  }

  esp_now_register_send_cb(OnDataSent);                           // ESTADO DEL PAQUETE TRANSMITIDO
  esp_now_register_recv_cb(OnDataRecv);                           // ESTADO DEL PAQUETE RECIBIDO

  esp_now_peer_info_t peerInfo;                                   // EMPAREJAMIENTO ESPNOW
  
  // REGISTROS DE EMPAREJAMIENTO CON EL NODO PRINCIPAL
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.encrypt = false;
  
  // AGREGA EL EMPAREJAMIENTO      
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    #ifdef DEBUG
    Serial.println("Failed to add peer");
    #endif
    return;
  }

  initSDCard();                                                   // INICIALIZACION MICROSD
  deleteFile(SD, "/data.txt");                                    // ELIMINA ARCHIVO EN LA MICROSD

  espnowMessage.id = ID;                                          // ID
  espnowMessage.nstate = true;                                    // ESTADO DEL NODO
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &espnowMessage, sizeof(espnowMessage)); // RESULTADO DEL ENVIO DEL PAQUETE ESPNOW
  
  if (result == ESP_OK) {
    #ifdef DEBUG
    Serial.println("Sent with success");
    #endif
  }
  #ifdef DEBUG
  else {
    Serial.println("Error sending the data");
  }
  #endif
  accelerometer();                                                // INICIA EL ACELEROMETRO
  create_tasksN();
}

void loop() {}
