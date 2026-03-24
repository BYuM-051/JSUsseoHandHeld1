#include <Arduino.h>
#include <RTOS.h>
#include <Wire.h>
#include "gui.h"
#include "ui.h"
#include "screens/ui_Screen1.h"

#pragma Once

#define __DEBUG__

void uiHandle(void* param);
#ifdef __DEBUG__
void deviceInfoLabelHandle(void* param);
#endif

#define HHUWB Serial1

static volatile bool isConnected = true;
static volatile bool labelUpdateExists = false;

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
    0
  );

  #ifdef __DEBUG__
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    Serial.println("Serial initialized");
  #endif

  HHUWB.begin(115200, SERIAL_8N1, GPIO_NUM_17, GPIO_NUM_18);
  HHUWB.setTimeout(100);
  while(!HHUWB) { delay(10); }
}

void loop()
{

}

void uiHandle(void* param)
{
  uint32_t lastTick = millis();
  while(true)
  {
    const uint32_t now = millis();
    lv_tick_inc(now - lastTick);
    lastTick = now;
    const uint32_t wait = lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(wait > 0 ? min<uint32_t>(wait, 5) : 1));
  }
}