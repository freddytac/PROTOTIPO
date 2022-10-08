/* UNIVERSIDAD DE CUENCA
 *  NOMBRES: MIGUEL BELTRAN Y FREDDY TACURI
*/
////////////////////////////////////////////////////////////////////////////
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

//#define DEBUG(a) Serial.println(a)
String data0 = "";
TaskHandle_t Tarea;

/////////////////////////////////////////////////////////////////////////////
//VARIABLES

#define TOKEN               "9p8WhrNL54lhJkxkqMCl"                // TOKEN 4


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
int ID=4;                                                         // Numero de NODO
int t0=0;                                                         // Tiempo de inicio de cada bucle
int t1=0;                                                         // Tiempo final de cada bucle
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

// ENVIA ESPNO
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


///--------------------------------------------------------------------------------///
//MICROSD
void initSDCard(){
   if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

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
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  //Serial.print("Read from file: ");
  while(file.available()){
    //Serial.write(file.read());
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
      //tb.loop();
      Telemetry datos[data_items] = {
        { "Time", vart},
        { "ax", varx},
        { "ay", vary},
        { "az", varz},
      };
      tb.sendTelemetry(datos, data_items);
      delay(211);
      counter=1;
    }    
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }
  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

void accelerometer(){
  if (!mpu.begin()) {                                             // INICIALIZACION MPU6050
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
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
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
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

  mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);
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
}


void setup() {
  // put your setup code here, to run once:
  //Serial.begin(115200);                                         // PUERTO SERIAL          

  WiFi.begin(ssid, password);                                     // AUTENTIFICACION RED WIFI
 
  while (WiFi.status() != WL_CONNECTED) {                         // VERIFICACION DE ESTADO CONECTADO
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());  

  WiFi.mode(WIFI_AP_STA);                                         // CONFIGURACION COMO AP Y ESTACION WIFI

  if (!tb.connected()) {                                          // VERIFICA CONEXION THINGBOARD
    subscribed = false;
    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {                 // VERIFICA CONEXION CON EL TOKEN
      Serial.println("Failed to connect");
      return;
    }
  }

  if (esp_now_init() != ESP_OK) {                                 // VALIDACION DE INICIACION ESPNOW
    Serial.println("Error initializing ESP-NOW");
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
    Serial.println("Failed to add peer");
    return;
  }


  initSDCard();                                                   // INICIALIZACION MICROSD
  deleteFile(SD, "/data.txt");                                    // ELIMINA ARCHIVO EN LA MICROSD

  espnowMessage.id = ID;                                          // ID
  espnowMessage.nstate = true;                                    // ESTADO DEL NODO
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &espnowMessage, sizeof(espnowMessage)); // RESULTADO DEL ENVIO DEL PAQUETE ESPNOW
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
  
  accelerometer();                                                // INICIA EL ACELEROMETRO

}

void loop() {
      //Serial.println("Im alive");
    t0=millis();
    if(espnowMessage0.nstate==true && espnowMessage0.id==1){
      if(bandera==false){
        tiempop=millis();
        bandera=true;
        }
      sensors_event_t a, g, t;
      mpu.getEvent(&a, &g, &t);
      dataMessage = String(millis()-tiempop) + "," + String(a.acceleration.z) + "," + String(-1*a.acceleration.y) + "," + String(a.acceleration.x)+"\n";
      appendFile(SD, "/data.txt", dataMessage.c_str());
      skate=true;
    }
    else if(skate==true && espnowMessage0.id==0){
      tb.sendTelemetryBool("t_start", true);
      readFile(SD, "/data.txt");
      valorx=1;
      }
      
    if(valorx==1){ 
      ESP.restart();
    }
    
    t1=millis();
    delay(100-t1+t0);  
}
