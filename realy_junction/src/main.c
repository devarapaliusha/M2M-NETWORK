/*the relay code which reads battery voltage from INA219, checks chain health and filter payloads according to architecture */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include "llcc68_driver.h"
#include <zephyr/shell/shell.h>
#include <zephyr/version.h>
#include <ctype.h>
#include <stdlib.h>
#include <zephyr/drivers/uart.h>
#include "radio_driver.h"
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/sensor.h>
#include <hal/nrf_ficr.h>
#include <stdio.h>
 
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
 
#define LED_PIN 17
#define DEV_ID 0x04
//#define UID 0xFFFF77C7CC
#define INA219_ADDR 0x40
#define REG_CFG     0x00
#define REG_SHUNT   0x01
#define REG_BUS     0x02
#define REG_CAL     0x05

 
//#define INA219_NODE DT_NODELABEL(ina219)
static const struct device *i2c;
 

uint8_t p_hdr[2] = {0x00, DEV_ID};
uint16_t p_id = 0x03;
float bat_v = 0.0f;
char *data;
char us[256] = {0};
volatile bool timeout = false;



void print_device_id(void)  
{
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    uint32_t id1 = NRF_FICR->DEVICEID[1];
 
    printf("Device ID: %08X%08X\n", id1, id0);
}

void ch_tmr_cb(struct k_timer *timer)
{
    LOG_INF("Channel health check timeout, resending...");
    timeout = true;
    return;
}

K_TIMER_DEFINE(timeout_timer, ch_tmr_cb, NULL); 

static int wr(uint8_t reg, uint16_t val)
{
    uint8_t b[3] = {reg, val >> 8, val};
    return i2c_write(i2c, b, 3, INA219_ADDR);
}

static int rd(uint8_t reg, int16_t *val)
{
    uint8_t b[2];
    int ret = i2c_write_read(i2c, INA219_ADDR, &reg, 1, b, 2);
    *val = (b[0] << 8) | b[1];
    return ret;
}


float battery_voltage(void)
{
    static bool init;
    int16_t raw;

    if (!init) {
        i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
        if (!device_is_ready(i2c))
            return -1.0f;

        wr(REG_CAL, 4096);
        wr(REG_CFG, 0x399F);
        k_msleep(15);
        init = true;
    }

    rd(REG_SHUNT, &raw);
    float shunt = raw * 0.01f;

    rd(REG_BUS, &raw);
    float bus = ((uint16_t)raw >> 3) * 0.004f;

    return bus + shunt / 1000.0f;
}

 
 void retx() {
        if ((data[0] & 0x07) == (us[0] & 0x07) && data[1] == us[1])
        {
          LOG_INF("duplicate data received, skipping transmission");
          return;
        }
        else
        {
            strcpy(us, data);
 
            LOG_INF("New data received, transmitting...");
 
            sendData(data);
 
            LOG_INF("Data transmitted: %s", data);
            return;
        }
    }

uint8_t pld_cnt () {
    p_id = p_id + 0x01;
    if (p_id > 0x07) {
        p_id = 0x01;
    }
    return p_id;
}

void header_builder ( uint8_t cmd_t) {

    uint8_t pld = pld_cnt();
    p_hdr[0] = cmd_t + pld;
    printf("Header built: 0x%02X\t0x%02X", p_hdr[0], p_hdr[1]);
    return;
}
void dev_hlth(void)
{
   
    char dev_rpt[128];
    char battery[5];
    bat_v = battery_voltage();
    LOG_INF("Preparing device health report...");
    
    header_builder(0x20);

    snprintf(battery, sizeof(battery), "%s", bat_v < 4.00f ? "BL" : "OK");

    memcpy(dev_rpt, &p_hdr, sizeof(p_hdr));
    memcpy(dev_rpt + sizeof(p_hdr), battery, strlen(battery) + 1);
    //LOG_INF("Device health report: %s", dev_rpt);
    LOG_INF("Header bytes: %02X %02X",
        (uint8_t)dev_rpt[0],
        (uint8_t)dev_rpt[1]);

    LOG_INF("Battery string: %s", &dev_rpt[2]);

    sendData(dev_rpt);
}

 
int main(void)
{

    const struct device *dev;
    const struct device *gpio0;

    usb_enable(NULL);
 
    /* ================= GPIO ================= */
 
    gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
 
    if (!device_is_ready(gpio0)) {
        LOG_ERR("GPIO device not ready");
        return 0;
    }
 
    gpio_pin_configure(gpio0, LED_PIN, GPIO_OUTPUT);
 
    /* ================= Console ================= */
 
    dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
 
    if (!device_is_ready(dev)) {
        return 0;
    }
 
    k_sleep(K_SECONDS(2));
 
 
    print_device_id();

    gpio_pin_toggle(gpio0, LED_PIN);
 
    ConfigureLora();
    llcc68_request(LLCC68_RX_CONTINUOUS);
    LOG_INF("Continuous RX started");

     k_timer_start(&timeout_timer,
              K_MINUTES(5),    // expires after 5 minutes
              K_MINUTES(5));   // repeats every 5 minutes
 
    while (true)
    {   
        //LOG_INF("Waiting for data...");
        data = receiveDataContinuous();

        if (data == NULL)
        {
            if (timeout)
            {   
                timeout = false;
                dev_hlth();
            }

            k_msleep(50);
            continue;
        }

        
        LOG_INF("===========RECEIVED===========");
        LOG_INF("command received: 0x%02X", data[0]);
        LOG_INF("dev_id: 0x%02X", data[1]);

 
       // if ((data[1] & 0xc0) != (DEV_ID & 0xc0)) {
       //     LOG_INF("Invalid Tunnel ID received, skipping transmission");
       //     k_msleep(50);
       //     continue;
       // }    
        /* ===================================================== */
        switch (data[0] & 0xF8) {
            case 0x00 :
                retx();
                break;
            case 0x18 :   
                dev_hlth();
                break;
            case 0x20 :
                LOG_INF("Device health report received, retx");
                retx();
                break;
            default :
                LOG_INF("Unknown command received: 0x%02X", data[0]);
                break; 
        } 
       k_msleep(50);
    }


    return 0;
}
