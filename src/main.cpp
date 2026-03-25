#include <Arduino.h>
#include <RTOS.h>
#include <Wire.h>
#include "gui.h"
#include "ui.h"
#include "screens/ui_Screen1.h"

#define __DEBUG__

#define HHUWB Serial1

bool isSerialInUse = false;

volatile bool isConnected = true;
volatile bool isLabelUpdateExists = false;
#define UICore 0
void uiHandle(void* param);
void setup();
void loop();

#ifdef __DEBUG__
#define DEVICE_CHECK_CORE 1
void deviceInfoCheck(void *param)
{
    #define UART_TIMEOUT_MS 200

    while(true)
    {
        if(!isSerialInUse)
        {
            bool uwbConnected = false;

            HHUWB.println("PING");
            uint32_t startTime = millis();
            while(millis() - startTime < UART_TIMEOUT_MS)
            {
                if(HHUWB.available())
                {
                    String response = HHUWB.readStringUntil('\n');
                    if(response == "PONG")
                    {
                        uwbConnected = true;
                        break;
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if(uwbConnected && !isConnected)
            {
                isConnected= true;
                Serial.println("BH0 is connected");
                isLabelUpdateExists = true;
            }
            else if(!uwbConnected && isConnected)
            {
                isConnected = false;
                Serial.println("BH0 is Timeout");
                isLabelUpdateExists = true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

void setup() 
{
    gui_start();

    xTaskCreatePinnedToCore
    (
        uiHandle,
        "GUI",
        4096,
        NULL,
        1,
        NULL,
        UICore
    );

    #ifdef __DEBUG__
        lv_label_set_text(ui_DebugLabel, "DEBUG : ENABLED");
        Serial.begin(115200);
        while (!Serial) { delay(10); }
        Serial.println("Serial initialized");
    #endif

    HHUWB.begin(115200, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_18);
    HHUWB.setTimeout(100);
    while(!HHUWB) { delay(10); }
    #ifdef __DEBUG__
        lv_label_set_text(ui_SerialLabel, "BH0 : Finding...");
        Serial.println("UART to BH0 initialized");

        xTaskCreatePinnedToCore
        (
            deviceInfoCheck,
            "DeviceInfoCheck",
            1024,
            NULL,
            0,
            NULL,
            DEVICE_CHECK_CORE
        );
    #endif

    #ifdef __DEBUG__
        if(psramInit())
        {
            Serial.printf("PSRAM init. Total : %d bytes, Free : %d bytes \n", ESP.getPsramSize(), ESP.getFreePsram());
        }
        else
        {
            Serial.println("PSRAM init failed!");
        }
    #endif
}

void loop()
{

}


void uiHandle(void* param)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    constexpr TickType_t frequency = pdMS_TO_TICKS(10);
    while(true)
    {
        if(isLabelUpdateExists)
        {
            lv_label_set_text(ui_SerialLabel, isConnected ? "BH0 : Connected" : "BH0 : Disconnected");
            lv_obj_invalidate(ui_SerialLabel);
            isLabelUpdateExists = false;
        }
        lv_tick_inc(10);
        lv_timer_handler();
        xTaskDelayUntil(&lastWakeTime, frequency);
    }
}


