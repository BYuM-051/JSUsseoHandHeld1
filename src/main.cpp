#include <Arduino.h>
#include <RTOS.h>
#include <Wire.h>
#include "gui.h"
#include "ui.h"
#include "screens/ui_Screen1.h"
#include "screens/ui_Screen2.h"
#include "ui_events.h"

#define __DEBUG__

#define HHUWB Serial1

bool isSerialInUse = false;

volatile bool isConnected = false;
volatile bool isLabelUpdateExists = true;
#define UICore 1
void uiHandle(void* param);
void setup();
void loop();

#ifdef __DEBUG__
#define DEVICE_CHECK_CORE 0
#define UART_TIMEOUT_MS 200
#define BH0_UART_RX_PIN GPIO_NUM_17
#define BH0_UART_TX_PIN GPIO_NUM_18
void deviceInfoCheck(void *param)
{

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
            
            if((uwbConnected && !isConnected) || (!uwbConnected && isConnected))
            {
                isLabelUpdateExists = true;
            }
            isConnected = uwbConnected;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

void setup() 
{
    #ifdef __DEBUG__
    
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    Serial.println("Serial initialized");
    #endif

    gui_start();
    delay(1000);
    #ifdef __DEBUG__
        lv_label_set_text(ui_DebugLabel, "DEBUG : ENABLED");
        lv_label_set_text(ui_SerialLabel, "BH0 : Searching...");
    #endif

    xTaskCreatePinnedToCore
    (
        uiHandle,
        "GUI",
        4096 * 3,
        NULL,
        1,
        NULL,
        UICore
    );



    HHUWB.begin(115200, SERIAL_8N1, BH0_UART_RX_PIN, BH0_UART_TX_PIN);
    HHUWB.setTimeout(100);
    delay(500); // delay to ensure Serial is ready before sending data
    #ifdef __DEBUG__
        Serial.println("UART to BH0 initialized");

        xTaskCreatePinnedToCore
        (
            deviceInfoCheck,
            "DeviceInfoCheck",
            4096,
            NULL,
            0,
            NULL,
            DEVICE_CHECK_CORE
        );
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
            Serial.print("UI : ");
            Serial.println(isConnected ? "BH0 : Connected" : "BH0 : Disconnected");
            lv_label_set_text(ui_SerialLabel, isConnected ? "BH0 : Connected" : "BH0 : Disconnected");
            lv_obj_invalidate(ui_SerialLabel);
            isLabelUpdateExists = false;
        }
        lv_tick_inc(10);
        lv_timer_handler();
        xTaskDelayUntil(&lastWakeTime, frequency);
    }
}

extern "C" 
{
    void unloadScreen1(lv_event_t * e)
    {
        #ifdef __DEBUG__
            Serial.println("Screen1 Unloaded");
        #endif
    }

    void loadedScreen1(lv_event_t * e)
    {
        #ifdef __DEBUG__
            Serial.println("Screen1 Loaded");
        #endif
    }

    void unloadScreen2(lv_event_t * e)
    {
        #ifdef __DEBUG__
            Serial.println("Screen2 Unloaded");
        #endif
    }

    void loadedScreen2(lv_event_t * e)
    {
        #ifdef __DEBUG__
            Serial.println("Screen2 Loaded");
        #endif
    }

    void unloadScreen3(lv_event_t * e)
    {
        #ifdef __DEBUG__
            Serial.println("Screen3 Unloaded");
        #endif
    }

    void loadedScreen3(lv_event_t * e)
    {
        #ifdef __DEBUG__
            Serial.println("Screen3 Loaded");
        #endif
    }

    void unloadScreen4(lv_event_t * e)
    {
        #ifdef __DEBUG__
            Serial.println("Screen4 Unloaded");
        #endif
    }

    void loadedScreen4(lv_event_t * e)
    {
        #ifdef __DEBUG__
            Serial.println("Screen4 Loaded");
        #endif
    }
}