#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <LoRa.h>

// UUID dla BLE
#define SERVICE_UUID        "12345678-1234-5678-1234-567890abcdef"
#define CHARACTERISTIC_UUID "12345678-1234-5678-1234-567890ABCDE0" 

// Piny LoRa
#define LORA_SCK  4
#define LORA_MISO 10
#define LORA_MOSI 11
#define LORA_SS   12
#define LORA_RST  14
#define LORA_DIO0 13
#define LORA_FREQUENCY 433E6


const String DEVICE_ID = "Marco"; 

const int SHIFT_KEY = 3; 

const String AUTHORIZATION_KEY = "tajny_klucz_a";

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
BLEServer *pServer;
bool advertisingStarted = false;
BLEAdvertising *pAdvertising;

QueueHandle_t messageQueueFromBLE;
QueueHandle_t messageQueueFromLoRa;

String encryptData(const String &input) {
  String encrypted = "";
  for (char c : input) {
    if (isalpha(c)) {
      char base = islower(c) ? 'a' : 'A';
      encrypted += (char)((c - base + SHIFT_KEY) % 26 + base);
    } else if (isdigit(c)) {
      encrypted += (char)((c - '0' + SHIFT_KEY) % 10 + '0');
    } else {
      encrypted += c;
    }
  }
  Serial.println("Zaszyfrowana wiadomość: " + encrypted);
  return encrypted;
}

String decryptData(const String &input) {
  String decrypted = "";
  for (char c : input) {
    if (isalpha(c)) {
      char base = islower(c) ? 'a' : 'A';
      decrypted += (char)((c - base - SHIFT_KEY + 26) % 26 + base);
    } else if (isdigit(c)) {
      decrypted += (char)((c - '0' - SHIFT_KEY + 10) % 10 + '0');
    } else {
      decrypted += c;
    }
  }
  Serial.println("Odszyfrowana wiadomość: " + decrypted);
  return decrypted;
}

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    if (value.length() > 0) {
      Serial.print("Odebrano przez BLE: ");
      Serial.println(value);

      xQueueSend(messageQueueFromBLE, value.c_str(), portMAX_DELAY);
    }
  }
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Urządzenie połączone.");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Urządzenie rozłączone.");
    pServer->getAdvertising()->stop(); 
    advertisingStarted = false;
    delay(500); 
    pServer->getAdvertising()->start(); 
    advertisingStarted = true;
    Serial.println("Reklamowanie wznowione.");
  }
};

void setupLoRa() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setSPI(SPI);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("Inicjalizacja LoRa nieudana!");
    while (1);
  }
  Serial.println("LoRa zainicjalizowane.");
}

void setupBLE() {
    BLEDevice::init(DEVICE_ID);
    if (BLEDevice::getInitialized()) {
        Serial.println("Bluetooth jest włączony.");
    } else {
        Serial.println("Bluetooth jest wyłączony.");
    }

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->setCallbacks(new MyCallbacks());
    pCharacteristic->setValue("Witaj BLE!");
    pService->start();

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLEUUID(SERVICE_UUID));
    pAdvertising->start();
    advertisingStarted = true;

    // Ustawienia interwałów reklamowania
    pServer->getAdvertising()->setMinPreferred(0x06); 
    pServer->getAdvertising()->setMaxPreferred(0x12); 

    // Ustawienie lokalnego MTU
    BLEDevice::setMTU(512); 

    delay(1000);
    Serial.println("BLE zainicjalizowane.");
    Serial.println("Service UUID: " + String(SERVICE_UUID));
    Serial.println("Characteristic UUID: " + String(CHARACTERISTIC_UUID));
    Serial.println("Advertising started.");
}


void loraTask(void *parameter) {
  while (1) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String encryptedMessage = "";
      while (LoRa.available()) {
        encryptedMessage += (char)LoRa.read();
      }
      Serial.println("Odebrano zaszyfrowaną wiadomość: " + encryptedMessage);

      String decryptedMessage = decryptData(encryptedMessage);

      int sep1 = decryptedMessage.indexOf(':');
      int sep2 = decryptedMessage.indexOf(':', sep1 + 1);
      String authKey;
      String senderID;
      String message;
      String formattedMessage;

      if (sep1 != -1 && sep2 != -1) {
        authKey = decryptedMessage.substring(0, sep1);
        senderID = decryptedMessage.substring(sep1 + 1, sep2);
        message = decryptedMessage.substring(sep2 + 1);
      } else {
        authKey = "";
        senderID = "Unknown";
        message = decryptedMessage;
      }

      if (authKey == AUTHORIZATION_KEY) {
        Serial.println("Poprawny klucz autoryzacji.");
        formattedMessage = senderID + ":" + message;
        xQueueSend(messageQueueFromLoRa, formattedMessage.c_str(), portMAX_DELAY);
      } else {
        Serial.println("Niepoprawny klucz autoryzacji.");
      }
    }

    char message[256];
    if (xQueueReceive(messageQueueFromBLE, &message, 0)) {
      String formattedMessage = AUTHORIZATION_KEY + ":" + DEVICE_ID + ":" + String(message);
      String encryptedMessage = encryptData(formattedMessage);
      Serial.println("Wysyłam zaszyfrowaną wiadomość: " + encryptedMessage);
      LoRa.beginPacket();
      LoRa.print(encryptedMessage);
      LoRa.endPacket();
    }

    delay(10);
  }
}
bool isDeviceConnected() {
    return deviceConnected;
}
void bleTask(void *parameter) {
  setupBLE();
  while (1) {
    if (isDeviceConnected()) {
      char message[256];
      if (xQueueReceive(messageQueueFromLoRa, &message, 0)) {
        pCharacteristic->setValue(message);
        pCharacteristic->notify();
        Serial.println("Wysłano przez BLE: " + String(message));
      }
    } else {
        if (advertisingStarted) {
            Serial.println("Urządzenie rozłączone przez klienta. Zatrzymuję reklamowanie.");
            pAdvertising->stop();
            advertisingStarted = false;
            delay(500);
            Serial.println("Uruchamiam reklamowanie ponownie.");
            pAdvertising->start();
            advertisingStarted = true;
        }
    }
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Start");
  setupLoRa();
  messageQueueFromBLE = xQueueCreate(10, sizeof(char) * 256);
  messageQueueFromLoRa = xQueueCreate(10, sizeof(char) * 256);

  xTaskCreate(loraTask, "LoRa Task", 4096, NULL, 1, NULL);
  xTaskCreate(bleTask, "BLE Task", 4096, NULL, 1, NULL);
}

void loop() {
  // Nie używamy loop, ponieważ zadania są obsługiwane przez FreeRTOS.
}
