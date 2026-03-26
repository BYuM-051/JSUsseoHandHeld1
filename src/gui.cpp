#include <Arduino.h>
#include <esp_heap_caps.h>
#include <Wire.h>
#include <lv_conf.h>
#include <lvgl.h>
#include "gui.h"
#include "ui/generated/ui.h"

static const char* TAG = "gui";

static const uint16_t screenWidth  = 800;
static const uint16_t screenHeight = 480;
static const uint32_t bufferSize = screenWidth * screenHeight;

static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;

static lv_disp_draw_buf_t draw_buf;
LGFX gfx;

void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
    if (gfx.getStartCount() > 0) gfx.waitDMA();

    gfx.startWrite();
    gfx.pushImageDMA( area->x1
                    , area->y1
                    , area->x2 - area->x1 + 1
                    , area->y2 - area->y1 + 1
                    , ( lgfx::rgb565_t* )color_p); 

    gfx.endWrite();
    lv_disp_flush_ready( disp );
}

void my_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
    uint16_t touchX, touchY;
    if( gfx.getTouch( &touchX, &touchY ) ) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void gui_start()
{
  gfx.begin();
  gfx.setBrightness(127);

  buf1 = (lv_color_t *)heap_caps_malloc(bufferSize * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  buf2 = (lv_color_t *)heap_caps_malloc(bufferSize * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (buf1 == NULL) {
      if (buf2 != NULL) {
          heap_caps_free(buf2);
          buf2 = NULL;
      }
      return;
  }

  if (buf2 == NULL) {
      // Keep a single full-screen PSRAM buffer if a second one doesn't fit.
      Serial.println("LVGL: second full buffer allocation failed, continuing with single buffering");
  }

  lv_init();
  lv_disp_draw_buf_init( &draw_buf, buf1, buf2, bufferSize );

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;

  disp_drv.full_refresh = 1; 
  
  lv_disp_drv_register( &disp_drv );

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );

  ui_init(); 
}
