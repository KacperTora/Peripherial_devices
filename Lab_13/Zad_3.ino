#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEAdvertisedDevice* myDevice = nullptr;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
        Serial.println("Smartphone found.");
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
      }
    }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting BLE Client.");

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
  if (myDevice != nullptr) {
    BLEClient* pClient = BLEDevice::createClient();
    Serial.println("Connecting to smartphone.");

    if (pClient->connect(myDevice)) {
      Serial.println("Connected to smartphone.");

      BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
      if (pRemoteService != nullptr) {
        BLERemoteCharacteristic* pRemoteChar = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
        if (pRemoteChar != nullptr) {
          pRemoteChar->writeValue("Message from ESP32");
          Serial.println("Data send.");
        }
      }

      delay(2000);
      pClient->disconnect();
      Serial.println("Disconnected.");
    }
    myDevice = nullptr;
  }
  delay(2000);
}
