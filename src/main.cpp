#include <Arduino.h>
#include <RTOS.h>
#include <Wire.h>
#include "gui.h"
#include "ui.h"
#include "screens/ui_Screen1.h"

#define __DEBUG__

void uiHandle(void* param);
#ifdef __DEBUG__
void deviceInfoLabelHandle(void* param);
#endif

#define HHUWB Serial1

static constexpr uint32_t kUiTaskStackSize = 4096;
static constexpr uint32_t kDeviceInfoTaskStackSize = 4096;
static constexpr uint32_t kUartTimeoutMs = 200;
static constexpr uint32_t kPingRetryDelayMs = 10;
static constexpr uint32_t kDevicePollIntervalMs = 1000;

static volatile bool isConnected = true;
static volatile bool labelUpdateExists = false;
static TaskHandle_t deviceInfoTaskHandle = nullptr;

extern "C" void bibarababira(lv_event_t * e)
{
  LV_UNUSED(e);
}

extern "C" void registerLabelUpdater(lv_event_t * e)
{
  LV_UNUSED(e);

  if (deviceInfoTaskHandle != nullptr) {
    return;
  }

#ifdef __DEBUG__
  Serial.println("UART to BH0 initialized");
#endif

  xTaskCreatePinnedToCore(
    deviceInfoLabelHandle,
    "DeviceInfoLabelHandle",
    kDeviceInfoTaskStackSize,
    NULL,
    0,
    &deviceInfoTaskHandle,
    0
  );
}

extern "C" void screen1KillLabelUpdater(lv_event_t * e)
{
  LV_UNUSED(e);

  if (deviceInfoTaskHandle == nullptr) {
    return;
  }

  TaskHandle_t taskToDelete = deviceInfoTaskHandle;
  deviceInfoTaskHandle = nullptr;
  vTaskDelete(taskToDelete);
}

void setup() 
{
  gui_start();

  xTaskCreatePinnedToCore(
    uiHandle,
    "GUI",
    kUiTaskStackSize,
    NULL,
    1,
    NULL,
    0
  );

#ifdef __DEBUG__
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("Serial initialized");
#endif

  HHUWB.begin(115200, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_18);
  HHUWB.setTimeout(100);
}

void loop()
{

}

void uiHandle(void* param)
{
  uint32_t lastTick = millis();
  while (true) {
    if (labelUpdateExists && ui_DeviceInfoLabel != nullptr) {
      lv_label_set_text(ui_DeviceInfoLabel, isConnected ? "BH0 : Connected" : "BH0 : Disconnected");
      labelUpdateExists = false;
    }

    const uint32_t now = millis();
    lv_tick_inc(now - lastTick);
    lastTick = now;
    const uint32_t wait = lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(wait > 0 ? min<uint32_t>(wait, 5) : 1));
  }
}

#ifdef __DEBUG__
void deviceInfoLabelHandle(void *param)
{
  LV_UNUSED(param);

  while (true) {
    bool uwbConnected = false;

    Serial.println("Ping");
    HHUWB.println("PING");

    const uint32_t startTime = millis();
    while (millis() - startTime < kUartTimeoutMs) {
      if (HHUWB.available()) {
        String response = HHUWB.readStringUntil('\n');
        response.trim();
        if (response == "PONG") {
          uwbConnected = true;
          break;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(kPingRetryDelayMs));
    }

    if (uwbConnected != isConnected) {
      isConnected = uwbConnected;
      labelUpdateExists = true;
      Serial.println(isConnected ? "BH0 is connected" : "BH0 is Timeout");
    }

    vTaskDelay(pdMS_TO_TICKS(kDevicePollIntervalMs));
  }
}
#endif
