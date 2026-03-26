// NOTE : I know that you wont read this sentence :/
#include <RTOS.h>
#define __DEBUG__

#define CurrentBoard BH1
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
#define MAX_SERIAL_LENGTH 64
#define HHUWB UART_NUM_1
#define BH0_UART_RX_PIN GPIO_NUM_17
#define BH0_UART_TX_PIN GPIO_NUM_18
#define UART_LISTENER_CORE 0
#define UART_BLOCK_TICKS pdMS_TO_TICKS(portMAX_DELAY)

QueueHandle_t hhuwbEventQueue;

volatile bool isConnected = false;
volatile TickType_t lastBRUpdateTime = 0;
volatile TickType_t lastBSUpdateTime = 0;
volatile uint32_t distanceBetweenBHAndBR = 0;
volatile uint32_t distanceBetweenBHAndBS = 0;

void uart_init(void);
void uartListener(void *param);
void onUARTDataReceived(char *cmdText);
void uartPrintf(uart_port_t uart_num, const char *format, ...);

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
void espNowDataSendCallback(const uint8_t *mac_addr, esp_now_send_status_t status);
void espNowListnener(void *param);
void onESPNowDataReceived(char *cmdText);
void espNowPrintf(const uint8_t *mac_addr, const char *format, ...);
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
    uart_init();
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

// UART Communication ===========================================================
void uart_init(void)
{
    uart_config_t uartConfig = 
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(HHUWB, &uartConfig);
    uart_set_pin(HHUWB, BH0_UART_TX_PIN, BH0_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(HHUWB, 1024 * 2, 1024 * 2, 20, &hhuwbEventQueue, 0);
    
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
}

void uartListener(void *param) 
{
    uart_event_t event;
    uint8_t data[MAX_SERIAL_LENGTH];
    
    while(true) 
    {
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
                        onUARTDataReceived((char *)data);
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
        else
        {
            #ifdef __DEBUG__
                uart_write_bytes(UART_NUM_1, "PING\n", 5);
                if(pingCount > MAX_PING_COUNT)
                {
                    pingCount = 0;
                    isLabelUpdateExists = isConnected;
                    isConnected = false;
                }
                else
                    {pingCount++;}
            #endif
        }
        
    }
}

void onUARTDataReceived(char *cmdText)
{
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

void uartPrintf(uart_port_t uart_num, const char *format, ...)
{
    char buffer[250];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    uart_write_bytes(uart_num, buffer, strlen(buffer));
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
        if(i == CurrentBoard)
        {
            peerInfo.peer_addr[0] = 0x02; // Locally Administered dummy Address
        }
        else
        {
            memcpy(peerInfo.peer_addr, MAC_ADDR[i], 6);
        }
        if(esp_now_add_peer(&peerInfo) != ESP_OK)
        {
            #ifdef __DEBUG__
                Serial.println("Failed to add ESP-NOW peer");
            #endif
        }
        else
        {
            #ifdef __DEBUG__
            Serial.print("Added ESP-NOW peer: ");
            Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", peerInfo.peer_addr[0], peerInfo.peer_addr[1], peerInfo.peer_addr[2], peerInfo.peer_addr[3], peerInfo.peer_addr[4], peerInfo.peer_addr[5]);
            #endif
        }
}

    esp_now_register_recv_cb(espNowDataRecvCallback);
    esp_now_register_send_cb(espNowDataSendCallback);
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
    espNowPacket_t buffer;
    memcpy(buffer.macAddr, mac_addr, 6);
    memcpy(buffer.data, data, len);
    buffer.len = len;
    xQueueSendFromISR(espNowRecvQueue, &buffer, NULL);
}

void espNowListnener(void *param)
{
    espNowPacket_t packet;
    while(true)
    {
        if(xQueueReceive(espNowRecvQueue, &packet, portMAX_DELAY))
        {
            #ifdef __DEBUG__
                Serial.println("ESP-NOW Data Received");
            #endif
            boolean isKnownSender = false;
            for(int i = 0 ; i < MAX_BOARDS ; i++)
            {
                if(memcmp(packet.macAddr, MAC_ADDR[i], 6) == 0)
                {
                    isKnownSender = true;
                    break;
                }
            }

            if(!isKnownSender)
            {
                #ifdef __DEBUG__
                    Serial.println("Unknown ESP-NOW Sender, Ignoring Packet");
                #endif
                continue;
            }
            else
            {
                if(packet.len > 0) 
                {
                    packet.data[packet.len] = '\0';
                    onESPNowDataReceived((char *)packet.data);
                }
                #ifdef __DEBUG__
                    else
                    {
                        Serial.println("Received Empty ESP-NOW Packet");
                    }
                #endif
            }
        }
    }
}

void onESPNowDataReceived(char *cmdText)
{
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
            Serial.printf("Unknown ESP-NOW Command Received : %s\n", cmdText);
        #endif
    }
}

//esp_now_send(targetMac, (uint8_t *)msg, len);
void espNowDataSendCallback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    #ifdef __DEBUG__
        Serial.print("ESP-NOW Data Send Callback, Status: ");
        Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
    #endif
}

void espNowPrintf(const uint8_t *mac_addr, const char *format, ...)
{
    char buffer[250];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    esp_now_send(mac_addr, (uint8_t *)buffer, strlen(buffer));
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
