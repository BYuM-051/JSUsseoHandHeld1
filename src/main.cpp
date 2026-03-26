// NOTE : I know that you wont read this sentence :/
#include <RTOS.h>
#define __DEBUG__

#define currentBoard BH1
// arduino core ============================================================
#include <Arduino.h>
void setup();
void loop();

// Project BOARD Info ======================================================
#define MAX_BOARDS 6
#define BR0 0
#define BR1 1
#define BR2 2
#define BH0 3
#define BH1 4
#define BS0 5

constexpr uint8_t MAC_ADDR [MAX_BOARDS][6] = 
{
    {0x20, 0x43, 0xA8, 0x42, 0x0C, 0xC8}, // BR0
    {0xC0, 0x5D, 0x89, 0xE9, 0x1C, 0x30}, // BR1
    {0x20, 0x43, 0xA8, 0x42, 0x0C, 0xCC}, // BR2
    {0x20, 0x43, 0xA8, 0x42, 0x10, 0xEC}, // BH0
    {0x3C, 0x84, 0x27, 0xFC, 0xD8, 0x94}, // BH1
    {0xC0, 0x5D, 0x89, 0xE9, 0x1C, 0x44}  // BS0
};

constexpr uint16_t UWB_ID [MAX_BOARDS] = 
{
    0x100,  // BR0
    0x101,  // BR1
    0x102,  // BR2
    0x200,  // BH0
    0x000,  // BH1 (N/A)
    0x300   // BS0
};

// lvgl systen =============================================================
#include "gui.h"
#include "ui.h"
#include "ui_events.h"

volatile bool isLabelUpdateExists = true;
#define UICore 1
void uiHandle(void* param);

// UART Communication ======================================================
#include "driver/uart.h"
QueueHandle_t hhuwbEventQueue;
#define MAX_SERIAL_LENGTH 64
#define HHUWB UART_NUM_1
#define BH0_UART_RX_PIN GPIO_NUM_17
#define BH0_UART_TX_PIN GPIO_NUM_18
void uartListener(void *param);
void onUARTDataReceived(uint8_t *data);
#define UART_LISTENER_CORE 0
#define UART_BLOCK_TICKS pdMS_TO_TICKS(portMAX_DELAY)
volatile bool isConnected = false;
volatile TickType_t lastBRUpdateTime = 0;
volatile TickType_t lastBSUpdateTime = 0;
volatile uint32_t distanceBetweenBHAndBR = 0;
volatile uint32_t distanceBetweenBHAndBS = 0;

#ifdef __DEBUG__
    volatile int pingCount = 0;
    #define MAX_PING_COUNT 5
    #undef UART_BLOCK_TICKS
    #define UART_BLOCK_TICKS pdMS_TO_TICKS(1000)
#endif

// ESP NOW Communication ===================================================
#include <WiFi.h>
#include <esp_now.h>

#define ESP_NOW_CHANNEL 1U
#define ESP_NOW_LISTENER_CORE 0

typedef struct 
{
    uint8_t macAddr[6];
    uint8_t data[250];
    int len;
} espNowPacket_t;

QueueHandle_t espNowRecvQueue;

void espNowInit(void);
void espNowDataRecvCallback(const uint8_t *mac_addr, const uint8_t *data, int len);
void espNowListnener(void *param);
void onESPNowDataReceived(espNowPacket_t *packet);
// UWB Communication =======================================================
// NOTE : UWB Communication is handled by BH0, BH1 only receives data from BH0 via UART, so UWB related code is not included in this file.

//==========================================================================

void setup() 
{
    // Initialize Serial ========================================================
    #ifdef __DEBUG__
    
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    Serial.println("Serial initialized");
    #endif
    //===========================================================================

    // Initialize GUI System ====================================================
    gui_start();
    delay(1000);
    #ifdef __DEBUG__
        lv_label_set_text(ui_DebugLabel, "DEBUG : ENABLED");
        lv_label_set_text(ui_SerialLabel, "BH0 : Searching...");
    #else
        lv_obj_add_flag(ui_DebugLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SerialLabel, LV_OBJ_FLAG_HIDDEN);
    #endif

    xTaskCreatePinnedToCore
    (
        uiHandle,
        "uiHandle",
        4096 * 3,
        NULL,
        1,
        NULL,
        UICore
    );
    //===========================================================================

    // Initialize UART for BH0 Communication ====================================
    constexpr uart_port_t hhuwbUART = UART_NUM_1;
    uart_config_t uartConfig = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(hhuwbUART, &uartConfig);
    uart_set_pin(hhuwbUART, BH0_UART_TX_PIN, BH0_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(hhuwbUART, 1024 * 2, 1024 * 2, 20, &hhuwbEventQueue, 0);
    
    delay(500); // delay to ensure Serial is ready before sending data

    xTaskCreatePinnedToCore
    (
        uartListener,
        "uartListener",
        4096,
        NULL,
        1,
        NULL,
        UART_LISTENER_CORE
    );
    //===========================================================================

    // Initialize ESP-NOW Communication =========================================
    espNowInit();
    delay(500);
    //===========================================================================
}

void loop()
{

}

// UI Handler Task ==============================================================
void uiHandle(void *param)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    constexpr TickType_t frequency = pdMS_TO_TICKS(10);
    while(true)
    {
        if(isLabelUpdateExists)
        {
            #ifdef __DEBUG__
                Serial.print("UI : ");
                Serial.println(isConnected ? "BH0 : Connected" : "BH0 : Disconnected");
            #endif
            lv_label_set_text(ui_SerialLabel, isConnected ? "BH0 : Connected" : "BH0 : Disconnected");
            lv_obj_invalidate(ui_SerialLabel);
            isLabelUpdateExists = false;
        }
        lv_tick_inc(10);
        lv_timer_handler();
        xTaskDelayUntil(&lastWakeTime, frequency);
    }
}

// UART Listener Task ===========================================================
void uartListener(void *param) 
{
    uart_event_t event;
    uint8_t* data = (uint8_t*) malloc(MAX_SERIAL_LENGTH * sizeof(char));
    
    while(true) 
    {
        #ifdef __DEBUG__
            Serial.println("UART Listener Task Running");
        #endif
        if(xQueueReceive(hhuwbEventQueue, (void *)&event, UART_BLOCK_TICKS)) 
        {
            #ifdef __DEBUG__
                pingCount = 0;
                Serial.println("UART From BH0 Event Received");
                isLabelUpdateExists = !isConnected;
                isConnected = true;
            #endif
            switch(event.type) 
            {
                case UART_DATA:
                    int len;
                    uart_get_buffered_data_len(UART_NUM_1, (size_t*)&len);
                    len = uart_read_bytes(UART_NUM_1, data, len, 100 / portTICK_PERIOD_MS);
                    
                    if(len > 0) 
                    {
                        data[len] = '\0';
                        onUARTDataReceived(data);
                    }
                    break;
                
                case UART_FIFO_OVF:
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(hhuwbEventQueue);
                    break;
                
                default:
                    #ifdef __DEBUG__
                        Serial.printf("Unknown UART Event Type: %d\n", event.type);
                    #endif
                    break;
            }
        }
        #ifdef __DEBUG__
        else
        {
            uart_write_bytes(UART_NUM_1, "PING\n", 5);
            if(pingCount > MAX_PING_COUNT)
            {
                pingCount = 0;
                isLabelUpdateExists = isConnected;
                isConnected = false;
            }
            else
                {pingCount++;}
        }
        #endif
    }
}

void onUARTDataReceived(uint8_t *data)
{
    char *cmdText = (char *) data;
    if(strcmp(cmdText, "PONG") == 0)
    {
        // Ping Pong Command, Do nothing
    }
    else if(strcmp(cmdText, "") == 0)
    {

    }
    else
    {
        #ifdef __DEBUG__
            Serial.printf("Unknown Command Received : %s\n", cmdText);
        #endif
    }
}

// ESP NOW Communication ========================================================
void espNowInit(void)
{
    WiFi.mode(WIFI_STA);
    if(esp_now_init() != ESP_OK)
    {
        #ifdef __DEBUG__
            Serial.println("ESP-NOW Initialization Failed");
        #endif
        return;
    }

    for(int i = 0 ; i < MAX_BOARDS ; i++)
    {
        esp_now_peer_info_t peerInfo = {};
        peerInfo.channel = ESP_NOW_CHANNEL;
        peerInfo.encrypt = false;
        memcpy(peerInfo.peer_addr, MAC_ADDR[i], 6);

        if(esp_now_add_peer(&peerInfo) != ESP_OK)
        {
            #ifdef __DEBUG__
                Serial.println("Failed to add ESP-NOW peer");
            #endif
        }
        #ifdef __DEBUG__
            else
            {
                Serial.print("Added ESP-NOW peer: ");
                Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", MAC_ADDR[i][0], MAC_ADDR[i][1], MAC_ADDR[i][2], MAC_ADDR[i][3], MAC_ADDR[i][4], MAC_ADDR[i][5]);
            }
        #endif
    }

    esp_now_register_recv_cb(espNowDataRecvCallback);
    espNowRecvQueue = xQueueCreate(10, sizeof(espNowPacket_t));
    xTaskCreatePinnedToCore
    (
        espNowListnener,
        "espNowListener",
        4096,
        NULL,
        2,
        NULL,
        ESP_NOW_LISTENER_CORE
    );
}

void espNowDataRecvCallback(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    //TODO : repay the techdept. Do this with my own hands.
}

void espNowListnener(void *param)
{
    //TODO : Implement ESP-NOW Data Handling Logic
}

void onESPNowDataReceived(espNowPacket_t *packet)
{
    //TODO : Implement ESP-NOW Data Handling Logic
}

// extenral UI events============================================================
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