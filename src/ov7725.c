/*
 * This file is part of the OpenMV project.
 * Copyright (c) 2013/2014 Ibrahim Abdelkader <i.abdalkader@gmail.com>
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * OV7725 driver.
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sccb.h"
#include "ov7725.h"
#include "ov7725_regs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
static const char* TAG = "ov7725";
#endif


static const uint8_t default_regs[][2] = {
    {COM3,          COM3_SWAP_YUV},
    {COM7,          COM7_RES_QVGA | COM7_FMT_YUV},

    {COM4,          0x01 | 0x00}, /* bypass PLL (0x00:off, 0x40:4x, 0x80:6x, 0xC0:8x) */
    {CLKRC,         0x80 | 0x03}, /* Res/Bypass pre-scalar (0x40:bypass, 0x00-0x3F:prescaler PCLK=XCLK/(prescaler + 1)/2 ) */

    // QVGA Window Size
    {HSTART,        0x3F},
    {HSIZE,         0x50},
    {VSTART,        0x03},
    {VSIZE,         0x78},
    {HREF,          0x00},

    // Scale down to QVGA Resolution
    {HOUTSIZE,      0x50},
    {VOUTSIZE,      0x78},
    {EXHCH,         0x00},

    {COM12,         0x03},
    {TGT_B,         0x7F},
    {FIXGAIN,       0x09},
    {AWB_CTRL0,     0xE0},
    {DSP_CTRL1,     0xFF},

    {DSP_CTRL2,     DSP_CTRL2_VDCW_EN | DSP_CTRL2_HDCW_EN | DSP_CTRL2_HZOOM_EN | DSP_CTRL2_VZOOM_EN},

    {DSP_CTRL3,     0x00},
    {DSP_CTRL4,     0x00},
    {DSPAUTO,       0xFF},

    {COM8,          0xF0},
    {COM6,          0xC5},
    {COM9,          0x41},  //0x11
    {COM10,         COM10_VSYNC_NEG | COM10_PCLK_MASK}, //Invert VSYNC and MASK PCLK
    {BDBASE,        0x7F},
    {DBSTEP,        0x03},
    {AEW,           0x96},
    {AEB,           0x64},
    {VPT,           0xA1},
    {EXHCL,         0x00},
    {AWB_CTRL3,     0xAA},
    {COM8,          0xFF},

    //Gamma
    {GAM1,          0x0C},
    {GAM2,          0x16},
    {GAM3,          0x2A},
    {GAM4,          0x4E},
    {GAM5,          0x61},
    {GAM6,          0x6F},
    {GAM7,          0x7B},
    {GAM8,          0x86},
    {GAM9,          0x8E},
    {GAM10,         0x97},
    {GAM11,         0xA4},
    {GAM12,         0xAF},
    {GAM13,         0xC5},
    {GAM14,         0xD7},
    {GAM15,         0xE8},

    {SLOP,          0x20},
    {EDGE1,         0x05},
    {EDGE2,         0x03},
    {EDGE3,         0x00},
    {DNSOFF,        0x01},

    {MTX1,          0xB0},
    {MTX2,          0x9D},
    {MTX3,          0x13},
    {MTX4,          0x16},
    {MTX5,          0x7B},
    {MTX6,          0x91},
    {MTX_CTRL,      0x1E},

    {BRIGHTNESS,    0x00},
    {CONTRAST,      0x20},
    {UVADJ0,        0x81},
    {SDE,           (SDE_CONT_BRIGHT_EN | SDE_SATURATION_EN)},

    // For 30 fps/60Hz
    {DM_LNL,        0x00},
    {DM_LNH,        0x00},
    {BDBASE,        0x7F},
    {DBSTEP,        0x03},

    // Lens Correction, should be tuned with real camera module
    {LC_RADI,       0x10},
    {LC_COEF,       0x10},
    {LC_COEFB,      0x14},
    {LC_COEFR,      0x17},
    {LC_CTR,        0x04},  //0x05
    {COM5,          0xF5}, //0x65

    {0x00,          0x00},
};


static int reset(sensor_t *sensor)
{
    int i=0;
    const uint8_t (*regs)[2];

    // Reset all registers
    SCCB_Write(sensor->slv_addr, COM7, COM7_RESET);

    // Delay 10 ms
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // Write default regsiters
    for (i=0, regs = default_regs; regs[i][0]; i++) {
        SCCB_Write(sensor->slv_addr, regs[i][0], regs[i][1]);
    }

    // Delay
    vTaskDelay(30 / portTICK_PERIOD_MS);

    return 0;
}


static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret=0;
    sensor->pixformat = pixformat;
    // Read register COM7
    uint8_t reg = SCCB_Read(sensor->slv_addr, COM7);

    switch (pixformat) {
    case PIXFORMAT_RGB565:
        reg =  COM7_SET_RGB(reg, COM7_FMT_RGB565);
        break;
    case PIXFORMAT_YUV422:
    case PIXFORMAT_GRAYSCALE:
        reg =  COM7_SET_FMT(reg, COM7_FMT_YUV);
        break;
    default:
        return -1;
    }

    // Write back register COM7
    ret = SCCB_Write(sensor->slv_addr, COM7, reg);

    // Delay
    vTaskDelay(30 / portTICK_PERIOD_MS);

    return ret;
}

static int set_framesize(sensor_t *sensor, framesize_t framesize)
{
    int ret=0;
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;
    uint8_t reg = SCCB_Read(sensor->slv_addr, COM7);

    sensor->status.framesize = framesize;

    // Write MSBs
    ret |= SCCB_Write(sensor->slv_addr, HOUTSIZE, w>>2);
    ret |= SCCB_Write(sensor->slv_addr, VOUTSIZE, h>>1);

    ret |= SCCB_Write(sensor->slv_addr, HSIZE, w>>2);
    ret |= SCCB_Write(sensor->slv_addr, VSIZE, h>>1);

    // Write LSBs
    ret |= SCCB_Write(sensor->slv_addr, HREF, ((w&0x3) | ((h&0x1) << 2)));

    if (framesize < FRAMESIZE_VGA) {
        // Enable auto-scaling/zooming factors
        ret |= SCCB_Write(sensor->slv_addr, DSPAUTO, 0xFF);

        ret |= SCCB_Write(sensor->slv_addr, HSTART, 0x3F);
        ret |= SCCB_Write(sensor->slv_addr, VSTART, 0x03);

        ret |= SCCB_Write(sensor->slv_addr, COM7, reg | COM7_RES_QVGA);

        ret |= SCCB_Write(sensor->slv_addr, CLKRC, 0x80 | 0x01);

    } else {
        // Disable auto-scaling/zooming factors
        ret |= SCCB_Write(sensor->slv_addr, DSPAUTO, 0xF3);

        // Clear auto-scaling/zooming factors
        ret |= SCCB_Write(sensor->slv_addr, SCAL0, 0x00);
        ret |= SCCB_Write(sensor->slv_addr, SCAL1, 0x00);
        ret |= SCCB_Write(sensor->slv_addr, SCAL2, 0x00);

        ret |= SCCB_Write(sensor->slv_addr, HSTART, 0x23);
        ret |= SCCB_Write(sensor->slv_addr, VSTART, 0x07);

        ret |= SCCB_Write(sensor->slv_addr, COM7, reg & ~COM7_RES_QVGA);

        ret |= SCCB_Write(sensor->slv_addr, CLKRC, 0x80 | 0x03);
    }

    // Delay
    vTaskDelay(30 / portTICK_PERIOD_MS);

    return ret;
}

static int set_colorbar(sensor_t *sensor, int enable)
{
    int ret=0;
    uint8_t reg;
    sensor->status.colorbar = enable;

    // Read reg COM3
    reg = SCCB_Read(sensor->slv_addr, COM3);
    // Enable colorbar test pattern output
    reg = COM3_SET_CBAR(reg, enable);
    // Write back COM3
    ret |= SCCB_Write(sensor->slv_addr, COM3, reg);

    // Read reg DSP_CTRL3
    reg = SCCB_Read(sensor->slv_addr, DSP_CTRL3);
    // Enable DSP colorbar output
    reg = DSP_CTRL3_SET_CBAR(reg, enable);
    // Write back DSP_CTRL3
    ret |= SCCB_Write(sensor->slv_addr, DSP_CTRL3, reg);

    return ret;
}

static int set_whitebal(sensor_t *sensor, int enable)
{
    // Read register COM8
    uint8_t reg = SCCB_Read(sensor->slv_addr, COM8);

    sensor->status.awb = enable;
    // Set white bal on/off
    reg = COM8_SET_AWB(reg, enable);

    // Write back register COM8
    return SCCB_Write(sensor->slv_addr, COM8, reg);
}

static int set_gain_ctrl(sensor_t *sensor, int enable)
{
    sensor->status.agc = enable;
    // Read register COM8
    uint8_t reg = SCCB_Read(sensor->slv_addr, COM8);

    // Set white bal on/off
    reg = COM8_SET_AGC(reg, enable);

    // Write back register COM8
    return SCCB_Write(sensor->slv_addr, COM8, reg);
}

static int set_exposure_ctrl(sensor_t *sensor, int enable)
{
    sensor->status.aec = enable;
    // Read register COM8
    uint8_t reg = SCCB_Read(sensor->slv_addr, COM8);

    // Set white bal on/off
    reg = COM8_SET_AEC(reg, enable);

    // Write back register COM8
    return SCCB_Write(sensor->slv_addr, COM8, reg);
}

static int set_hmirror(sensor_t *sensor, int enable)
{
    sensor->status.hmirror = enable;
    // Read register COM3
    uint8_t reg = SCCB_Read(sensor->slv_addr, COM3);

    // Set mirror on/off
    reg = COM3_SET_MIRROR(reg, enable);

    // Write back register COM3
    return SCCB_Write(sensor->slv_addr, COM3, reg);
}

static int set_vflip(sensor_t *sensor, int enable)
{
    sensor->status.vflip = enable;
    // Read register COM3
    uint8_t reg = SCCB_Read(sensor->slv_addr, COM3);

    // Set mirror on/off
    reg = COM3_SET_FLIP(reg, enable);

    // Write back register COM3
    return SCCB_Write(sensor->slv_addr, COM3, reg);
}

static int init_status(sensor_t *sensor)
{
    sensor->status.awb = 0;//get_reg_bits(sensor, BANK_DSP, CTRL1, 3, 1);
    sensor->status.aec = 0;
    sensor->status.agc = 0;
    sensor->status.hmirror = 0;
    sensor->status.vflip = 0;
    sensor->status.colorbar = 0;
    return 0;
}

static int set_dummy(sensor_t *sensor, int val){ return -1; }
static int set_gainceiling_dummy(sensor_t *sensor, gainceiling_t val){ return -1; }

int ov7725_init(sensor_t *sensor)
{
    // Set function pointers
    sensor->reset = reset;
    sensor->init_status = init_status;
    sensor->set_pixformat = set_pixformat;
    sensor->set_framesize = set_framesize;
    sensor->set_colorbar = set_colorbar;
    sensor->set_whitebal = set_whitebal;
    sensor->set_gain_ctrl = set_gain_ctrl;
    sensor->set_exposure_ctrl = set_exposure_ctrl;
    sensor->set_hmirror = set_hmirror;
    sensor->set_vflip = set_vflip;

    //not supported
    sensor->set_brightness= set_dummy;
    sensor->set_saturation= set_dummy;
    sensor->set_quality = set_dummy;
    sensor->set_gainceiling = set_gainceiling_dummy;
    sensor->set_gain_ctrl = set_dummy;
    sensor->set_exposure_ctrl = set_dummy;
    sensor->set_hmirror = set_dummy;
    sensor->set_vflip = set_dummy;
    sensor->set_whitebal = set_dummy;
    sensor->set_aec2 = set_dummy;
    sensor->set_aec_value = set_dummy;
    sensor->set_special_effect = set_dummy;
    sensor->set_wb_mode = set_dummy;
    sensor->set_ae_level = set_dummy;
    sensor->set_dcw = set_dummy;
    sensor->set_bpc = set_dummy;
    sensor->set_wpc = set_dummy;
    sensor->set_awb_gain = set_dummy;
    sensor->set_agc_gain = set_dummy;
    sensor->set_raw_gma = set_dummy;
    sensor->set_lenc = set_dummy;
    sensor->set_sharpness = set_dummy;
    sensor->set_denoise = set_dummy;




    // Retrieve sensor's signature
    sensor->id.MIDH = SCCB_Read(sensor->slv_addr, REG_MIDH);
    sensor->id.MIDL = SCCB_Read(sensor->slv_addr, REG_MIDL);
    sensor->id.PID = SCCB_Read(sensor->slv_addr, REG_PID);
    sensor->id.VER = SCCB_Read(sensor->slv_addr, REG_VER);
    
    ESP_LOGD(TAG, "OV7725 Attached !");

    return 0;
}
