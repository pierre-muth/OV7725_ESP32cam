#include "OV7725aiThinker.h"
#include <img_converters.h>


#define TAG "OV7725aiThinker"

camera_config_t esp32cam_aithinker_config {

    .pin_pwdn = 32,
    .pin_reset = -1,

    .pin_xclk = 0,

    .pin_sscb_sda = 26,
    .pin_sscb_scl = 27,

    // Note: LED GPIO is apparently 4 not sure where that goes
    // per https://github.com/donny681/ESP32_CAMERA_QR/blob/e4ef44549876457cd841f33a0892c82a71f35358/main/led.c
    .pin_d7 = 35,
    .pin_d6 = 34,
    .pin_d5 = 39,
    .pin_d4 = 36,
    .pin_d3 = 21,
    .pin_d2 = 19,
    .pin_d1 = 18,
    .pin_d0 = 5,
    .pin_vsync = 25,
    .pin_href = 23,
    .pin_pclk = 22,
    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_VGA,
    .jpeg_quality = 80,              
    .fb_count = 2 
};


void OV7725aiThinker::run(void)
{
    if(fb) {
        esp_camera_fb_return(fb);//return the frame buffer back to the driver for reuse
    }

    if (_jpg_buf) {
        free(_jpg_buf);
        _jpg_buf = NULL;
    }

    fb = esp_camera_fb_get();
    bool jpeg_converted = frame2jpg(fb, _cam_config.jpeg_quality, &_jpg_buf, &_jpg_buf_len);
    
    if(!jpeg_converted) Serial.println("JPEG compression failed");
}

void OV7725aiThinker::runIfNeeded(void)
{
    if(!fb)
        run();
}

int OV7725aiThinker::getWidth(void)
{
    runIfNeeded();
    return fb->width;
}

int OV7725aiThinker::getHeight(void)
{
    runIfNeeded();
    return fb->height;
}

size_t OV7725aiThinker::getSize(void)
{
    runIfNeeded();
    if (_jpg_buf) return _jpg_buf_len;
    else return fb->len;
}

uint8_t *OV7725aiThinker::getfb(void)
{
    runIfNeeded();

    if (_jpg_buf) return _jpg_buf;
    else return fb->buf;
}

framesize_t OV7725aiThinker::getFrameSize(void)
{
    return _cam_config.frame_size;
}

void OV7725aiThinker::setFrameSize(framesize_t size)
{
    _cam_config.frame_size = size;
}

pixformat_t OV7725aiThinker::getPixelFormat(void)
{
    if (_jpg_buf) return PIXFORMAT_JPEG;
    else return _cam_config.pixel_format;
}

void OV7725aiThinker::setPixelFormat(pixformat_t format)
{
    switch (format)
    {
    case PIXFORMAT_RGB565:
    case PIXFORMAT_YUV422:
        _cam_config.pixel_format = format;
            break;
    case PIXFORMAT_GRAYSCALE:
    case PIXFORMAT_JPEG:
        _cam_config.pixel_format = format;
        break;
    default:
        _cam_config.pixel_format = PIXFORMAT_YUV422;
        break;
    }
}


esp_err_t OV7725aiThinker::init(camera_config_t config)
{
    printf("OV7725aiThinker::init");
    memset(&_cam_config, 0, sizeof(_cam_config));
    memcpy(&_cam_config, &config, sizeof(config));

    esp_err_t err = esp_camera_init(&_cam_config);
    if (err != ESP_OK) {
        printf("Camera probe failed with error 0x%x", err);
        return err;
    }
    // ESP_ERROR_CHECK(gpio_install_isr_service(0));

    

    return ESP_OK;
}
