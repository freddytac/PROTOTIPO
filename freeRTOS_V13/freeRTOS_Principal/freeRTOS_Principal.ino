/*
                                UNIVERSIDAD DE CUENCA
              DESARROLLO DE UN SISTEMA IoT PARA LA ADQUISICION DE DATOS BIOMECANICOS
                    DE UN PATINADOR DE VELOCIDAD, UTILIZANDO DISPOSITIVOS ESP32
                          NOMBRES: MIGUEL BELTRAN Y FREDDY TACURI
 */
#include "ThingsBoard.h"
#include <esp_now.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "esp_system.h"
#include "driver/gpio.h"

#define TOKEN "lMScR4Sz50sKRVZm02xv"           // TOKEN
#define THINGSBOARD_SERVER "thingsboard.cloud" // DIRECCION IP O LINK
WiFiClient espClient;                          // DEFINICION DEL CLIENTE WIFI
ThingsBoardSized<64> tb(espClient);            // DEFINICION DEL CLIENTE MQTT
int status = WL_IDLE_STATUS;                   // ESTADO DEL RADIO WIFI
bool subscribed = false;
bool state = false; // ESTADO DE TODOS LOS NODOS(TRUE=SISTEMA LISTO, FALSE=FALSE)
int letsgo = 0;     // INICIO DE CARRERA
#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

const char *ssid = "Mabs";
const char *password = "123probando";

// REPLACE WITH YOUR ESP RECEIVER'S MAC ADDRESS

uint8_t broadcastAddress1[] = {0x30, 0xC6, 0xF7, 0x2F, 0x17, 0x40};
uint8_t broadcastAddress2[] = {0x78, 0xE3, 0x6D, 0x11, 0x17, 0x64};
uint8_t broadcastAddress3[] = {0x34, 0x86, 0x5D, 0x3A, 0xF4, 0xE4};
// uint8_t broadcastAddress3[] = {0x34, 0x86, 0x5D, 0x3B, 0x45, 0xD8};
uint8_t broadcastAddress4[] = {0x94, 0xB5, 0x55, 0x25, 0x7C, 0x24};

typedef struct struct_message
{              // Estructura del mensaje
  int id;      // Identificador de la tarjeta
  bool nstate; // Estado del nodo
} struct_message;

struct_message espnowMessage;  // Creacion del mensaje espnow recibir
struct_message espnowMessage1; // Creacion del mensaje de la tarjeta 1
struct_message espnowMessage2; // Creacion del mensaje de la tarjeta 2
struct_message espnowMessage3; // Creacion del mensaje de la tarjeta 3
struct_message espnowMessage4; // Creacion del mensaje de la tarjeta 4
struct_message espnowMessage0; // Creacion del mensaje espnow enviar
// Create an array with all the structures
struct_message boardsStruct[4] = {espnowMessage1, espnowMessage2, espnowMessage3, espnowMessage4};

// ESPNOW RECIBIR
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  char macStr[24];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  memcpy(&espnowMessage, incomingData, sizeof(espnowMessage));
  boardsStruct[espnowMessage.id - 1].id = espnowMessage.id; // Actualiza los valores entrantes
  boardsStruct[espnowMessage.id - 1].nstate = espnowMessage.nstate;
}

// ESPNOW ENVIAR
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  char macStr[24];
  // Copies the sender mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

RPC_Response GpioState(const RPC_Data &data)
{ //  Funcion RPC
  // Serial.println("Received the set GPIO RPC method");
  letsgo = data["start"];
  return RPC_Response(NULL, (int)data["start"]);
}

RPC_Callback callbacks[] = { // LLAMADA RPC
    {"setOrder", GpioState}};

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_AP_STA);

  if (esp_now_init() != ESP_OK)
  { // VALIDACION DE INICIACION ESPNOW
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.println("Setting as a Wi-Fi Station..");
  }

  esp_now_register_recv_cb(OnDataRecv); // REGISTRO PARA OBTENER EL MENSAJE ESPNOW
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo;
  // register first peer
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }
  // register second peer
  memcpy(peerInfo.peer_addr, broadcastAddress2, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }
  /// register third peer
  memcpy(peerInfo.peer_addr, broadcastAddress3, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }

  /// register four peer
  memcpy(peerInfo.peer_addr, broadcastAddress4, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }
}

// void setup() {}
void loop()
{
  if (!tb.connected())
  {
    subscribed = false;
    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN))
    {
      Serial.println("Failed to connect");
      return;
    }
  }

  // Subscribe for RPC, if needed
  if (!subscribed)
  {
    Serial.println("Subscribing for RPC...");
    // Perform a subscription. All consequent data processing will happen in
    // callbacks as denoted by callbacks[] array.
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks)))
    {
      Serial.println("Failed to subscribe for RPC");
      return;
    }
    Serial.println("Subscribe done");
    subscribed = true;
    tb.sendTelemetryBool("state", state);
  }

  if (boardsStruct[0].nstate == true && boardsStruct[1].nstate == true && boardsStruct[2].nstate == true && boardsStruct[3].nstate == true && state == false)
  {
    state = true;
    tb.sendTelemetryBool("state", state);
  }

  if (letsgo == 1)
  {
    espnowMessage0.id = 1;
    espnowMessage0.nstate = true;
    esp_err_t result = esp_now_send(0, (uint8_t *)&espnowMessage0, sizeof(struct_message));
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP.restart();
  }
  else if (letsgo == 2)
  {
    espnowMessage0.id = 0;
    espnowMessage0.nstate = false;
    esp_err_t result = esp_now_send(0, (uint8_t *)&espnowMessage0, sizeof(struct_message));
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP.restart();
  }
  tb.loop();
  // vTaskDelay(pdMS_TO_TICKS(10));
}
