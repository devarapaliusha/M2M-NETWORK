#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include <string.h>

#include "llcc68_driver.h"
#include "radio_driver.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);



/*=========================================================
 * USB INIT
 *=========================================================*/

static void usb_init(void)
{
    int ret;

    ret = usb_enable(NULL);

    if (ret != 0)
    {
        LOG_ERR("USB INIT FAILED: %d", ret);
        return;
    }

    k_sleep(K_SECONDS(3));

    LOG_INF("====================================");
    LOG_INF("        USB CDC READY");
    LOG_INF("====================================");
}


/*=========================================================
 * LORA INIT
 *=========================================================*/

static void lora_init(void)
{
    ConfigureLora();

    llcc68_request(LLCC68_RX_CONTINUOUS);

    LOG_INF("LoRa continuous RX started");
}


/*=========================================================
 * PROCESS RECEIVED PACKET
 *
 * Packet format:
 *
 * Byte 0 = Command
 * Byte 1 = Device ID
 * Byte 2 = QoS + Hop + Flags
 * Byte 3 = Payload
 *=========================================================*/

static void process_received(lora_packet_t *pkt)
{
    uint8_t command;
    uint8_t dev_id;
    uint8_t header;

    uint8_t qos;
    uint8_t hop;
    uint8_t flags;


    /*-----------------------------------------------------
     * Make sure packet has minimum 3 header bytes
     *-----------------------------------------------------*/

    if (pkt->len < 3)
    {
        LOG_WRN("Packet too short: %d bytes",
                pkt->len);

        return;
    }


    /*-----------------------------------------------------
     * Read packet header
     *-----------------------------------------------------*/

    command = (uint8_t)pkt->payload[0];

    dev_id = (uint8_t)pkt->payload[1];

    header = (uint8_t)pkt->payload[2];


    /*-----------------------------------------------------
     * Extract QoS
     *
     * Bits 7-6
     *-----------------------------------------------------*/

    qos = (header >> 6) & 0x03;


    /*-----------------------------------------------------
     * Extract Hop
     *
     * Bits 5-3
     *-----------------------------------------------------*/

    hop = (header >> 3) & 0x07;


    /*-----------------------------------------------------
     * Extract Flags
     *
     * Bits 2-0
     *-----------------------------------------------------*/

    flags = header & 0x07;


    /*-----------------------------------------------------
     * Print received packet
     *-----------------------------------------------------*/

    LOG_INF("");
    LOG_INF("========== RECEIVED PACKET ==========");

    LOG_INF("Length    : %d", pkt->len);

    LOG_INF("Command   : 0x%02X", command & 0xF8);

    LOG_INF("DEV_ID    : 0x%02X", dev_id);

    LOG_INF("QHF       : 0x%02X", header);

    LOG_INF("QoS       : %d", qos);

    LOG_INF("Hop Count : %d", hop);

    LOG_INF("Flags     : %d", flags);


    /*-----------------------------------------------------
     * Payload starts from Byte 3
     *-----------------------------------------------------*/

    LOG_INF("DATA      : %s",
            &pkt->payload[3]);
    LOG_INF("payload : %s",pkt);

    /*-----------------------------------------------------
     * Radio information
     *-----------------------------------------------------*/

    LOG_INF("RSSI      : %d",
            pkt->rssi);

    LOG_INF("SNR       : %d",
            pkt->snr);

    LOG_INF("=====================================");
}


/*=========================================================
 * MAIN
 *=========================================================*/

int main(void)
{
    usb_init();

    lora_init();


    while (1)
    {
        lora_packet_t pkt;


        /*
         * Clear packet structure
         */
        memset(&pkt, 0, sizeof(pkt));


        /*
         * RECEIVE
         *
         * receiveDataContinuous() expects:
         *
         * lora_packet_t *
         */
        if (receiveDataContinuous(&pkt))
        {
            /*
             * Packet received successfully
             */
            process_received(&pkt);
        }


        k_msleep(5000);
    }


    return 0;
}
