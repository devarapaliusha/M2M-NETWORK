#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <string.h>

#include "llcc68_driver.h"
#include "radio_driver.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


/*================ Configuration ================*/

#define LED_PIN         17
#define DEV_ID          0xBF 
#define SEND_INTERVAL   5000


/*================ Header1 =====================*/
/*
 * Bit 7-6 : QoS
 * Bit 5-3 : Hop Count
 * Bit 2-0 : Flags
 */

#define QOS_VALUE       0x01
#define HOP_COUNT_VALUE 0x02
#define FLAGS_VALUE     0x00


/*================ Command =====================*/

uint8_t commandID = 0x01;


/*================ GPIO ========================*/

const struct device *gpio0;


/*================================================
 * Create Header1
 *================================================*/

uint8_t create_header1(void)
{
    uint8_t header1;

    header1 =
        ((QOS_VALUE & 0x03) << 6) |
        ((HOP_COUNT_VALUE & 0x07) << 3) |
        (FLAGS_VALUE & 0x07);

    return header1;
}


/*================================================
 * Send Automatic Packet
 *
 * Packet format:
 *
 * Byte 0 = Command ID
 * Byte 1 = Header1
 * Byte 2 = Device ID
 * Byte 3 onwards = Data
 *================================================*/

void send_auto_packet(void)
{
    uint8_t packet[64];

    uint8_t header1;

    header1 = create_header1();

    memset(packet, 0, sizeof(packet));


    /* Command ID */
    packet[0] = commandID;

     /* Device ID */
    packet[1] = DEV_ID;

    /* QoS + Hop + Flags */
    packet[2] = header1;

   

    /* Payload */
    strcpy((char *)&packet[3], "Hello");


    /*================ TX Log ================*/

    LOG_INF("========== TX PACKET ==========");

    LOG_INF("Command : 0x%02X", commandID);

    LOG_INF("Header1 : 0x%02X", header1);

    LOG_INF("QoS     : %d", QOS_VALUE);

    LOG_INF("Hop     : %d", HOP_COUNT_VALUE);

    LOG_INF("Flags   : %d", FLAGS_VALUE);

    LOG_INF("DEV_ID  : 0x%02X", DEV_ID);

    LOG_INF("DATA    : Hello");


    /* LED ON */

    gpio_pin_set(gpio0, LED_PIN, 1);


    /* Send LoRa packet */

    sendData(packet);
    k_msleep(100);


    /* LED OFF */

    gpio_pin_set(gpio0, LED_PIN, 0);


    LOG_INF("Packet Sent");

    LOG_INF("===============================");


    /*================ Command Counter ================*/

    commandID++;

    if (commandID > 0x07)
    {
        commandID = 0x01;
    }
}


/*================================================
 * Process Received Packet
 *
 * Packet format:
 *
 * Byte 0 = Command ID
 * Byte 1 = Header1
 * Byte 2 = Device ID
 * Byte 3 onwards = Data
 *================================================*/

void process_received(uint8_t *data)
{
    uint8_t command;
    uint8_t header1;
    uint8_t header2;

    uint8_t qos;
    uint8_t hop;
    uint8_t flags;


    /* Read packet */

    command = data[0];

    header1 = data[1];

    header2 = data[2];


    /* Extract Header1 */

    qos =
        (header1 >> 6) & 0x03;

    hop =
        (header1 >> 3) & 0x07;

    flags =
        header1 & 0x07;


    /*================ RX Log ================*/

    LOG_INF("========== RX PACKET ==========");

    LOG_INF("Command : 0x%02X", command);

    LOG_INF("Header1 : 0x%02X", header1);

    LOG_INF("QoS     : %d", qos);

    LOG_INF("Hop     : %d", hop);

    LOG_INF("Flags   : %d", flags);

    LOG_INF("DEV_ID  : 0x%02X", header2);

    LOG_INF("DATA    : %s", &data[3]);

    LOG_INF("===============================");


    /*================ Device ID Check ================*/

    if (header2 != DEV_ID)
    {
        LOG_INF("Invalid Device ID");

        return;
    }


    /*================ Retransmit ====================*/

    if ((flags & 0x01) == 0x00)
    {
        LOG_INF("Data packet received");

        sendData(data);
    }
}


/*================================================
 * Main
 *================================================*/

int main(void)
{
    uint32_t last_send = 0;


    /*================ USB =================*/

    usb_enable(NULL);


    /*================ GPIO =================*/

    gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (!device_is_ready(gpio0))
    {
        LOG_ERR("GPIO not ready");

        return 0;
    }


    gpio_pin_configure(
        gpio0,
        LED_PIN,
        GPIO_OUTPUT
    );


    /*================ LoRa =================*/

    ConfigureLora();

    llcc68_request(LLCC68_RX_CONTINUOUS);

    LOG_INF("LoRa RX Started");


    /*================ Main Loop =============*/

    while (1)
    {
        uint8_t *data;


        /*------------------------------------
         * Send packet every 5 seconds
         *-----------------------------------*/

        if ((k_uptime_get_32() - last_send) >= SEND_INTERVAL)
        {
            last_send = k_uptime_get_32();

            send_auto_packet();
        }


        /*------------------------------------
         * Continuous RX
         *-----------------------------------

        data = receiveDataContinuous();

        if (data != NULL)
        {
            process_received(data);
        }

        */
        k_msleep(50);
    }


    return 0;
}
