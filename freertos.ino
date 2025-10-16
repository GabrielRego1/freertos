#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"

// -------------------- Global Variables --------------------
QueueHandle_t dataQueue = NULL;

volatile bool sendTaskActive = false;
volatile bool receiveTaskActive = false;

// -------------------- Data Sending Task --------------------
void SendDataTask(void *pvParameter)
{
    esp_task_wdt_add(NULL); // register task in WDT
    int counter = 0;

    while (1)
    {
        if (xQueueSend(dataQueue, &counter, 0) != pdTRUE)
        {
            Serial.printf("[RM88420] Gabriel Antonio do Rego - [QUEUE ERROR] Failed to send value %d\n", counter);
        }
        else
        {
            Serial.printf("[RM88420] Gabriel Antonio do Rego - [QUEUE OK] Value %d sent to the queue\n", counter);
        }

        counter++;
        sendTaskActive = true;

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// -------------------- Data Receiving Task --------------------
void ReceiveDataTask(void *pvParameter)
{
    esp_task_wdt_add(NULL); // register task in WDT
    int receivedValue = 0;
    int timeoutCounter = 0;

    while (1)
    {
        if (xQueueReceive(dataQueue, &receivedValue, 0) == pdTRUE)
        {
            timeoutCounter = 0;
            Serial.printf("[RM88420] Gabriel Antonio do Rego - [QUEUE OK] Received value: %d\n", receivedValue);
        }
        else
        {
            timeoutCounter++;

            if (timeoutCounter == 10)
                Serial.println("[RM88420] Gabriel Antonio do Rego - [WARNING] No data received in the last 5 seconds");
            else if (timeoutCounter == 20)
            {
                Serial.println("[RM88420] Gabriel Antonio do Rego - [RECOVERY] Resetting data queue");
                xQueueReset(dataQueue);
            }
            else if (timeoutCounter == 30)
            {
                Serial.println("[RM88420] Gabriel Antonio do Rego - [RECOVERY] Restarting system now");
                esp_restart();
            }
        }

        receiveTaskActive = true;
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// -------------------- System Monitoring Task --------------------
void SystemMonitorTask(void *pvParameter)
{
    esp_task_wdt_add(NULL); // register task in WDT

    while (1)
    {
        Serial.println("\n===== [SYSTEM MONITORING] =====");
        Serial.printf("[Send Task]: %s\n", sendTaskActive ? "[RM88420] Gabriel Antonio do Rego - Running normally" : "[RM88420] Gabriel Antonio do Rego - NOT RESPONDING!");
        Serial.printf("[Receive Task]: %s\n", receiveTaskActive ? "[RM88420] Gabriel Antonio do Rego - Running normally" : "[RM88420] Gabriel Antonio do Rego - NOT RESPONDING!");
        Serial.println("===================================");

        // reset task flags
        sendTaskActive = false;
        receiveTaskActive = false;

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// -------------------- Setup --------------------
void setup()
{
    Serial.begin(115200);
    while (!Serial) { delay(10); } // wait for serial to initialize
    Serial.println("[RM88420] Gabriel Antonio do Rego - Initializing system...");

    // Watchdog configuration
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdtConfig);

    // Create the queue
    dataQueue = xQueueCreate(1, sizeof(int));
    if (dataQueue == NULL)
    {
        Serial.println("[RM88420] Gabriel Antonio do Rego - Failed to create data queue");
        esp_restart();
    }

    // Create tasks
    xTaskCreate(SendDataTask, "SendDataTask", 8192, NULL, 5, NULL);
    xTaskCreate(ReceiveDataTask, "ReceiveDataTask", 8192, NULL, 5, NULL);
    xTaskCreate(SystemMonitorTask, "SystemMonitorTask", 8192, NULL, 6, NULL); // higher priority for monitoring
}

void loop(){}