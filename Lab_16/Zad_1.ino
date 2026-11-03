#include <Arduino.h>
#include <DHT.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

#define DHTPIN 13
#define DHTTYPE DHT11

struct RawMeasurement {
  float temperature;
  float humidity;
};

struct ProcessedMeasurement {
  uint32_t sampleId;
  unsigned long timestampMs;
  float temperature;
  float humidity;
  char payloadString[32];
};

QueueHandle_t rawDataQueue;
QueueHandle_t processedDataQueue;

BLEAdvertising *pAdvertising;
DHT dht(DHTPIN, DHTTYPE);

void TaskAcquisition(void *pvParameters) {
  dht.begin();
  RawMeasurement raw;

  for (;;) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("Sensor error");
    } else {
      raw.temperature = t;
      raw.humidity = h;

      if (xQueueSend(rawDataQueue, &raw, pdMS_TO_TICKS(100)) == pdPASS) {
        Serial.printf("Raw: T=%.1f C, H=%.1f %%\n", t, h);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void TaskProcessing(void *pvParameters) {
  RawMeasurement raw;
  ProcessedMeasurement processed;
  uint32_t counter = 0;

  for (;;) {
    if (xQueueReceive(rawDataQueue, &raw, portMAX_DELAY) == pdPASS) {
      counter++;
      processed.sampleId = counter;
      processed.timestampMs = millis();
      processed.temperature = raw.temperature;
      processed.humidity = raw.humidity;

      snprintf(processed.payloadString, sizeof(processed.payloadString),
               "T:%.1f H:%.0f%% #%lu",
               processed.temperature, processed.humidity, processed.sampleId);

      Serial.printf("Proc: %s (TS: %lu)\n", processed.payloadString, processed.timestampMs);

      xQueueSend(processedDataQueue, &processed, pdMS_TO_TICKS(100));
    }
  }
}

void TaskTransmission(void *pvParameters) {
  ProcessedMeasurement outData;

  for (;;) {
    if (xQueueReceive(processedDataQueue, &outData, portMAX_DELAY) == pdPASS) {

      BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
      oAdvertisementData.setFlags(0x06);
      oAdvertisementData.setName("ESP32");

      String mfgData = "";
      mfgData += (char)0xFF;
      mfgData += (char)0xFF;
      mfgData += outData.payloadString;
      oAdvertisementData.setManufacturerData(mfgData);

      BLEAdvertisementData oScanResponseData = BLEAdvertisementData();
      
      String scanRespPayload = "";
      scanRespPayload += (char)0xFF;
      scanRespPayload += (char)0xFF;
      scanRespPayload += "Ala ma kota";
      oScanResponseData.setManufacturerData(scanRespPayload);

      pAdvertising->stop();
      pAdvertising->setAdvertisementData(oAdvertisementData);
      pAdvertising->setScanResponseData(oScanResponseData);
      pAdvertising->start();

      Serial.printf("Adv: %s | ScanRsp: Ala ma kota\n", outData.payloadString);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  BLEDevice::init("ESP32");
  pAdvertising = BLEDevice::getAdvertising();
  
  pAdvertising->setAdvertisementType(ADV_TYPE_SCAN_IND);
  pAdvertising->setScanResponse(true);

  rawDataQueue = xQueueCreate(5, sizeof(RawMeasurement));
  processedDataQueue = xQueueCreate(5, sizeof(ProcessedMeasurement));

  if (rawDataQueue == NULL || processedDataQueue == NULL) {
    Serial.println("Queue allocation failed");
    while (1);
  }

  xTaskCreatePinnedToCore(TaskAcquisition,  "Task1", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskProcessing,   "Task2", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskTransmission, "Task3", 4096, NULL, 1, NULL, 0);

  Serial.println("RTOS tasks initialized");
}

void loop() {
  vTaskDelete(NULL);
}
