// UNIVERSIDAD DE CUENCA
//  NOMBRES: MIGUEL BELTRAN Y FREDDY TACURI
//
////////////////////////////////////////////////////////////////////////////
//LIBRERIAS
#include "ThingsBoard.h"
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
//#define WIFI_AP_NAME        "CELERITY_AVILES PARRA"               // SSID
//#define WIFI_PASSWORD       "Shodan_user1"                        // CONTRASEÑA
#define WIFI_AP_NAME        "FAMTACURIM"               // SSID
#define WIFI_PASSWORD       "20etelvina22"                        // CONTRASEÑA
#define TOKEN               "lMScR4Sz50sKRVZm02xv"                // TOKEN
#define THINGSBOARD_SERVER  "thingsboard.cloud"                   // DIRECCION IP O LINK
WiFiClient espClient;                                             // DEFINICION DEL CLIENTE WIFI
//ThingsBoard tb(espClient);                                      // DEFINICION DEL CLIENTE MQTT 
ThingsBoardSized<128> tb(espClient);
int status = WL_IDLE_STATUS;                                      // ESTADO DEL RADIO WIFI
Adafruit_MPU6050 mpu;                                             // DEFINICION DEL SENSOR MPU6050
bool subscribed=false;                                          
bool state=false;                                                 // ESTADO DE TODOS LOS NODOS(TRUE=SISTEMA LISTO, FALSE=FALSE)
bool letsgo=false;                                                // INICIO DE CARRERA
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
String dataMessage;                                               // Texto a almacenar
float Time=0;                                                     // Marca de tiempo
bool skate=false;                                                 // Bandera: true=patinando, false=inactivo
//bool complete=false;                                              // Bandera: true=carrera realizada, false = carrera sin realizar
int t0=0;
int t1=0;
bool pcapture=false;
bool npsent=false;
int counter=1;
float vart, varx, vary, varz;
int contador=1;
const int data_items = 4;                                       // Cantidad de items a enviar al servidor thinboard time,ax,ay,az
bool recibe=false;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct struct_message {                                   // Estructura del mensaje
    int id;                                                       // Identificador de la tarjeta
    bool nstate;                                                    // Estado del nodo
} struct_message;

struct_message espnowMessage;                                     // Creacion del mensaje espnow recibir
struct_message espnowMessage1;                                    // Creacion del mensaje de la tarjeta 1
struct_message espnowMessage2;                                    // Creacion del mensaje de la tarjeta 2
struct_message espnowMessage3;                                    // Creacion del mensaje de la tarjeta 3
struct_message espnowMessage4;                                    // Creacion del mensaje de la tarjeta 4
struct_message espnowMessage0;                                     // Creacion del mensaje espnow enviar
// Create an array with all the structures
struct_message boardsStruct[4] = {espnowMessage1, espnowMessage2, espnowMessage3, espnowMessage4};



// REPLACE WITH YOUR ESP RECEIVER'S MAC ADDRESS
uint8_t broadcastAddress1[] = {0x78, 0xE3, 0x6D, 0x10, 0x12, 0x20};
uint8_t broadcastAddress2[] = {0x78, 0xE3, 0x6D, 0x11, 0x17, 0x64};
uint8_t broadcastAddress3[] = {0x34, 0x86, 0x5D, 0x3B, 0x4E, 0x14};
uint8_t broadcastAddress4[] = {0x34, 0x86, 0x5D, 0x3B, 0x48, 0x0C};


esp_now_peer_info_t peerInfo;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//FUNCIONES

// ESPNOW RECIBIR
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  memcpy(&espnowMessage, incomingData, sizeof(espnowMessage));
  boardsStruct[espnowMessage.id-1].id = espnowMessage.id;         // Actualiza los valores entrantes
  boardsStruct[espnowMessage.id-1].nstate = espnowMessage.nstate;
}


// ESPNOW ENVIAR
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  //Serial.print("Packet to: ");
  // Copies the sender mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  //Serial.print(macStr);
  //Serial.print(" send status:\t");
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}



///////////////////////////////////////////////////////////////////////////////////////////
void InitWiFi()
{
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network
  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
  
}

void reconnect() {
  // Loop until we're reconnected
  status = WiFi.status();
  if ( status != WL_CONNECTED) {
    WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");
  }
}


///////////////////////////////////////////////////////////////////////////////////////////
// RPC

RPC_Response GpioState(const RPC_Data &data){                     //  Funcion RPC
  //Serial.println("Received the set GPIO RPC method");
  letsgo = data["start"];
  return RPC_Response(NULL, (bool)data["start"]);
}

/*RPC_Response algo(const RPC_Data &data){                     //  Funcion RPC
  //Serial.println("Received the set GPIO RPC method");
  recibe = data["Recibido"];
  return RPC_Response(NULL, (bool)data["Recibido"]);
}*/


RPC_Callback callbacks[] = {                                      // LLAMADA RPC
  { "setOrder",    GpioState },
  //{ "ack", algo },
};



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
          vart=data0.toFloat();
          Serial.print("Time: ");
          Serial.print(contador);  
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
      tb.loop();
      Telemetry datos[data_items] = {
        { "Time", contador},
        { "ax", varx},
        { "ay", vary},
        { "az", varz},
      };
      tb.sendTelemetry(datos, data_items);
      delay(200);
      //Serial.println(recibe);
      //recibe=false;
      counter=1;
      contador++;
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




//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
//
void setup() {
  Serial.begin(115200);                                           // PUERTO SERIAL
      //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(Sincro,"Tarea",10000,NULL,0,&Tarea,0); // CREACION DE LA TAREA Almacenamiento             
  delay(500); 
  WiFi.mode( WIFI_STA );
  //WiFi.disconnect();
  //int chan=11;
  //ESP_ERROR_CHECK(esp_wifi_set_channel(chan,WIFI_SECOND_CHAN_NONE));
  
  if (esp_now_init() != ESP_OK) {                                 // VALIDACION DE INICIACION ESPNOW
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);                           // REGISTRO PARA OBTENER EL MENSAJE ESPNOW
  esp_now_register_send_cb(OnDataSent);                           // REGISTRO PARA ENVIAR EL MENSAJE ESPNOW

   // register peer
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  // register first peer  
  memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  // register second peer  
  memcpy(peerInfo.peer_addr, broadcastAddress2, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  /// register third peer
  memcpy(peerInfo.peer_addr, broadcastAddress3, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

    /// register four peer
  memcpy(peerInfo.peer_addr, broadcastAddress4, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
   
  initSDCard();                                                   // INICIALIZACION MICROSD
  deleteFile(SD, "/data.txt");
  /*File file = SD.open("/data.txt");
  if(!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    //writeFile(SD, "/data.txt", "Time, ax, ay, az \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();*/


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

  mpu.setFilterBandwidth(MPU6050_BAND_10_HZ);
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



//////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  // Serial.print("ESTADO 1>>>");
        // Reconnect to WiFi, if needed
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }
    // Reconnect to ThingsBoard, if needed
  if (!tb.connected()) {
    subscribed = false;
    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      return;
    }
  }

  // Subscribe for RPC, if needed
  if (!subscribed) {
    Serial.println("Subscribing for RPC...");
    // Perform a subscription. All consequent data processing will happen in
    // callbacks as denoted by callbacks[] array.
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }
    Serial.println("Subscribe done");
    subscribed = true;
    tb.sendTelemetryBool("state", state);
    tb.loop();
  }
  
  //if(state==false){

    //tb.loop();  
  //}
  //if(skate==true && letsgo==false){
    //Serial.println(recibe);
    //recibe=false;
  //}
  tb.loop();    
}


/////////////////////////////////////////////////////////////////////////////
// NUCLEO0 ========> SINCRONIZACION
void Sincro( void * pvParameters ){
  for(;;){
    t0=millis();
    if(boardsStruct[0].nstate==true && boardsStruct[1].nstate==true && boardsStruct[2].nstate==true && boardsStruct[3].nstate==true && state==false){
      state=true;
    //skate=true;
      tb.sendTelemetryBool("state", state);
      Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>>>ENTRA<<<<<<<<<<<<<<<<<<<<<<<<<<");
    } 


    
    if(letsgo==true){
      skate=true;
      espnowMessage0.id=1;
      espnowMessage0.nstate=true;
      esp_err_t result = esp_now_send(0, (uint8_t *) &espnowMessage0, sizeof(struct_message));
      //if (result == ESP_OK) {
        sensors_event_t a, g, t;
        mpu.getEvent(&a, &g, &t);
        dataMessage = String(Time) + "," + String(a.acceleration.x) + "," + String(a.acceleration.y) + "," + String(a.acceleration.z)+"\n";
        appendFile(SD, "/data.txt", dataMessage.c_str());
      //}
      //else {
        //Serial.println("Error sending the data");
      //}
    }
    else if(skate==true && letsgo==false){
      espnowMessage0.id=0;
      espnowMessage0.nstate=false;
      esp_err_t result = esp_now_send(0, (uint8_t *) &espnowMessage0, sizeof(struct_message));
      //delay(50);
      readFile(SD, "/data.txt");
      deleteFile(SD, "/data.txt");
      t0=millis();
      skate=false;
      state=false;
      contador=1;
      }
    //Serial.println("NUCLEO 0");
    ///////////////////////////////////////////////////////////////////////////
    t1=millis();
    delay(100-t1+t0);
  }
}


/////////////////////////////////////////////////////////////////////////////
