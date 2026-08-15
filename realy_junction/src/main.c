#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>

#include "llcc68_driver.h"
#include "radio_driver.h"

#include <zephyr/usb/usb_device.h>
#include <hal/nrf_ficr.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


/* =========================================================
 * GPIO / DEVICE
 * ========================================================= */

#define LED_PIN     17
#define DEV_ID      0x41


/* =========================================================
 * COMMANDS
 * ========================================================= */

#define CMD_NORMAL_DATA       0x00
#define CMD_HEALTH_REQUEST    0x18
#define CMD_HEALTH_DATA       0x20


/* =========================================================
 * QoS / Hop / Flags
 *
 * Byte2:
 * Bit 7-6 = QoS
 * Bit 5-3 = Hop Count
 * Bit 2-0 = Flags
 * ========================================================= */

#define DEFAULT_QOS     1
#define DEFAULT_HOP     0
#define DEFAULT_FLAGS   0


/* =========================================================
 * INA219
 * ========================================================= */

#define INA219_ADDR     0x40

#define REG_CFG         0x00
#define REG_SHUNT       0x01
#define REG_BUS         0x02
#define REG_CAL         0x05


/* =========================================================
 * GLOBAL VARIABLES
 * ========================================================= */

static const struct device *i2c;
static const struct device *gpio0;


/*
 * Packet format:
 *
 * Byte 0 : Command
 * Byte 1 : Device ID
 * Byte 2 : QoS + Hop Count + Flags
 * Byte 3 : Payload
 */

uint8_t p_hdr[3] =
{
    0x01,
    DEV_ID,
    0x00
};


uint16_t p_id = 0x03;

float bat_v = 0.0f;


/*
 * Received packet
 */
char *data;


/*
 * Previous packet used for duplicate checking
 */
char us[256] = {0};


/*
 * Health timer flag
 */
volatile bool timeout = false;


/* =========================================================
 * DEVICE ID
 * ========================================================= */

void print_device_id(void)
{
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    uint32_t id1 = NRF_FICR->DEVICEID[1];

    printf("Device ID: %08X%08X\n",
           id1,
           id0);
}


/* =========================================================
 * TIMER
 * ========================================================= */

void ch_tmr_cb(struct k_timer *timer)
{
    LOG_INF("Channel health check timeout, sending...");

    timeout = true;
}


K_TIMER_DEFINE(timeout_timer,
                ch_tmr_cb,
                NULL);


/* =========================================================
 * INA219 WRITE
 * ========================================================= */

static int wr(uint8_t reg, uint16_t val)
{
    uint8_t b[3] =
    {
        reg,
        val >> 8,
        val
    };

    return i2c_write(i2c,
                     b,
                     3,
                     INA219_ADDR);
}


/* =========================================================
 * INA219 READ
 * ========================================================= */

static int rd(uint8_t reg, int16_t *val)
{
    uint8_t b[2];

    int ret = i2c_write_read(i2c,
                             INA219_ADDR,
                             &reg,
                             1,
                             b,
                             2);

    *val = (b[0] << 8) | b[1];

    return ret;
}


/* =========================================================
 * BATTERY VOLTAGE
 * ========================================================= */

float battery_voltage(void)
{
    static bool init;

    int16_t raw;

    if (!init)
    {
        i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));

        if (!device_is_ready(i2c))
        {
            LOG_ERR("INA219 I2C not ready");
            return -1.0f;
        }

        wr(REG_CAL, 4096);

        wr(REG_CFG, 0x399F);

        k_msleep(15);

        init = true;
    }


    rd(REG_SHUNT, &raw);

    float shunt = raw * 0.01f;


    rd(REG_BUS, &raw);

    float bus =
        ((uint16_t)raw >> 3) * 0.004f;


    return bus + (shunt / 1000.0f);
}


/* =========================================================
 * PAYLOAD COUNTER
 * ========================================================= */

uint8_t pld_cnt()
{
    p_id = p_id + 0x01;

    if (p_id > 0x07)
    {
        p_id = 0x01;
    }

    return p_id;
}


/* =========================================================
 * HEADER BUILDER
 * ========================================================= */

void header_builder(uint8_t cmd_t)
{
    uint8_t pld = pld_cnt();


    /*
     * Byte0 = Command + Payload ID
     * Byte1 = Device ID
     * Byte2 = QoS/Hop/Flags
     */

    p_hdr[0] = cmd_t + pld;

    p_hdr[1] = DEV_ID;

    p_hdr[2] =
        ((DEFAULT_QOS & 0x03) << 6) |
        ((DEFAULT_HOP & 0x07) << 3) |
        (DEFAULT_FLAGS & 0x07);


    LOG_INF("Header Built");

    LOG_INF("Command : 0x%02X",
            p_hdr[0]);

    LOG_INF("Dev ID  : 0x%02X",
            p_hdr[1]);

    LOG_INF("QHF Byte: 0x%02X",
            p_hdr[2]);
}


/* =========================================================
 * RETRANSMISSION
 *
 * Receive:
 *
 * 50 81 48 Hello
 *
 * QoS  = 1
 * Hop  = 1
 * Flag = 0
 *
 *
 * Junction changes:
 *
 * 50 81 50 Hello
 *
 * QoS  = 1
 * Hop  = 2
 * Flag = 0
 * ========================================================= */

void retx()
{
    /*
     * Duplicate packet check
     */

    if ((data[0] == us[0]) &&
        (data[1] == us[1]) )
    {
        LOG_INF("Duplicate data received, skipping transmission");

        return;
    }


    /*
     * Save current packet
     */

    memcpy(us, data, 256);


    LOG_INF("New data received");


    /*
     * Read current QoS / Hop / Flags
     */

    uint8_t qos =
        ((uint8_t)data[2] >> 6) & 0x03;

    uint8_t hop =
        ((uint8_t)data[2] >> 3) & 0x07;

    uint8_t flags =
        (uint8_t)data[2] & 0x07;


    LOG_INF("Before retransmission");

    LOG_INF("QoS      : %d",
            qos);

    LOG_INF("Hop Count: %d",
            hop);

    LOG_INF("Flags    : %d",
            flags);


    /*
     * Increase Hop Count
     */

    if (hop < 7)
    {
        hop++;
    }
    else
    {
        LOG_INF("Maximum hop count reached");

        return;
    }


    /*
     * Build new QHF byte
     */

    data[2] =
        ((qos & 0x03) << 6) |
        ((hop & 0x07) << 3) |
        (flags & 0x07);


    LOG_INF("After retransmission");

    LOG_INF("New QHF Byte: 0x%02X",
            (uint8_t)data[2]);

    LOG_INF("QoS      : %d",
            qos);

    LOG_INF("Hop Count: %d",
            hop);

    LOG_INF("Flags    : %d",
            flags);


    /*
     * LED ON
     */

    gpio_pin_set(gpio0,
                 LED_PIN,
                 1);


    /*
     * Send packet to next receiver
     */

    sendData(data);
    k_msleep(100);


    /*
     * LED OFF
     */

    gpio_pin_set(gpio0,
                 LED_PIN,
                 0);


    LOG_INF("Data retransmitted to receiver");
}


/* =========================================================
 * DEVICE HEALTH
 * ========================================================= */

void dev_hlth(void)
{
    char dev_rpt[128];

    char battery[5];


    bat_v = battery_voltage();


    LOG_INF("Preparing device health report...");


    /*
     * Command = 0x20
     */

    header_builder(CMD_HEALTH_DATA);


    if (bat_v < 4.00f)
    {
        strcpy(battery, "BL");
    }
    else
    {
        strcpy(battery, "OK");
    }


    /*
     * Copy header
     */

    memcpy(dev_rpt,
           p_hdr,
           sizeof(p_hdr));


    /*
     * Copy battery payload
     */

    memcpy(dev_rpt + sizeof(p_hdr),
           battery,
           strlen(battery) + 1);


    LOG_INF("========== DEVICE HEALTH REPORT ==========");


    LOG_INF("Command : 0x%02X",
            (uint8_t)dev_rpt[0]);


    LOG_INF("Dev ID  : 0x%02X",
            (uint8_t)dev_rpt[1]);


    /*
     * Decode QHF
     */

    uint8_t qos =
        ((uint8_t)dev_rpt[2] >> 6) & 0x03;

    uint8_t hop =
        ((uint8_t)dev_rpt[2] >> 3) & 0x07;

    uint8_t flags =
        (uint8_t)dev_rpt[2] & 0x07;


    LOG_INF("QHF Byte : 0x%02X",
            (uint8_t)dev_rpt[2]);


    LOG_INF("QoS      : %d",
            qos);


    LOG_INF("Hop Count: %d",
            hop);


    LOG_INF("Flags    : %d",
            flags);


    LOG_INF("Battery  : %s",
            &dev_rpt[3]);


    /*
     * Send health report
     */

    gpio_pin_set(gpio0,
                 LED_PIN,
                 1);


    sendData(dev_rpt);
    k_msleep(100);

    gpio_pin_set(gpio0,
                 LED_PIN,
                 0);
}


/* =========================================================
 * MAIN
 * ========================================================= */

int main(void)
{
    const struct device *dev;


    /* =====================================================
     * USB
     * ===================================================== */

    usb_enable(NULL);


    /* =====================================================
     * GPIO
     * ===================================================== */

    gpio0 =
        DEVICE_DT_GET(DT_NODELABEL(gpio0));


    if (!device_is_ready(gpio0))
    {
        LOG_ERR("GPIO device not ready");

        return 0;
    }


    gpio_pin_configure(gpio0,
                       LED_PIN,
                       GPIO_OUTPUT);


    /* =====================================================
     * CONSOLE
     * ===================================================== */

    dev =
        DEVICE_DT_GET(DT_CHOSEN(zephyr_console));


    if (!device_is_ready(dev))
    {
        LOG_ERR("Console device not ready");

        return 0;
    }


    k_sleep(K_SECONDS(2));


    /* =====================================================
     * PRINT MCU ID
     * ===================================================== */

    print_device_id();


    /* =====================================================
     * LORA INITIALIZATION
     * ===================================================== */

    ConfigureLora();


    llcc68_request(LLCC68_RX_CONTINUOUS);


    LOG_INF("Continuous RX started");


    /* =====================================================
     * DEVICE HEALTH TIMER
     *
     * First trigger : 5 minutes
     * Repeat        : every 5 minutes
     * ===================================================== */

    k_timer_start(&timeout_timer,
                  K_MINUTES(5),
                  K_MINUTES(5));


    /* =====================================================
     * MAIN LOOP
     * ===================================================== */

    while (true)
    {
        /*
         * Receive LoRa packet
         */

        data = receiveDataContinuous();


        /* =================================================
         * NO PACKET
         * ================================================= */

        if (data == NULL)
        {
            /*
             * Check health timer
             */

            if (timeout)
            {
                timeout = false;

                dev_hlth();
            }


            k_msleep(5000);

            continue;
        }


        /* =================================================
         * PACKET RECEIVED
         * ================================================= */

        gpio_pin_toggle(gpio0,
                        LED_PIN);


        LOG_INF("===========JUNCTION RECEIVED===========");


        LOG_INF("Command received : 0x%02X",
                (uint8_t)data[0]);


        LOG_INF("Dev ID           : 0x%02X",
                (uint8_t)data[1]);


        /* =================================================
         * QOS / HOP / FLAGS
         * ================================================= */

        uint8_t qos =
            ((uint8_t)data[2] >> 6) & 0x03;


        uint8_t hop =
            ((uint8_t)data[2] >> 3) & 0x07;


        uint8_t flags =
            (uint8_t)data[2] & 0x07;


        LOG_INF("QHF Byte : 0x%02X",
                (uint8_t)data[2]);


        LOG_INF("QoS      : %d",
                qos);


        LOG_INF("Hop Count: %d",
                hop);


        LOG_INF("Flags    : %d",
                flags);



        /* =================================================
         * COMMAND HANDLING
         * ================================================= */

        switch ((uint8_t)data[0] & 0xF8)
        {
            /*
             * Normal data packet
             *
             * 0x50
             */

            case CMD_NORMAL_DATA:

                LOG_INF("Valid normal data packet received");

                retx();

                break;


            /*
             * Optional old normal command
             *
             * 0x00
             */

            case 0x50:

                LOG_INF("Normal data packet received");

                retx();

                break;


            /*
             * Health request
             */

            case CMD_HEALTH_REQUEST:

                LOG_INF("Health request received");

                dev_hlth();

                break;


            /*
             * Health packet
             */

            case CMD_HEALTH_DATA:

                LOG_INF("Device health packet received");

                retx();

                break;


            /*
             * Unknown command
             */

            default:

                LOG_INF("Unknown command : 0x%02X",
                        (uint8_t)data[0]);

                break;
        }


        k_msleep(5000);


        gpio_pin_toggle(gpio0,
                        LED_PIN);
    }


    return 0;
}
