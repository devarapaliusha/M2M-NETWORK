#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include "llcc68_driver.h"
#include "radio_driver.h"

#include <string.h>
#include <stdio.h>


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);


/* -------------------------------------------------------------------------- */
/* CONFIG                                                                      */
/* -------------------------------------------------------------------------- */

#define UID 0x4C7F
#define DEV_ID 0x01


/* -------------------------------------------------------------------------- */
/* USB INIT                                                                    */
/* -------------------------------------------------------------------------- */

static void usb_init(void)
{
    int ret;

    ret = usb_enable(NULL);

    if (ret != 0) {

        printk("USB INIT FAILED %d\r\n", ret);
        return;
    }


    k_sleep(K_SECONDS(3));


    printk("\r\n");
    printk("====================================\r\n");
    printk(" USB CDC READY\r\n");
    printk("====================================\r\n");
}



/* -------------------------------------------------------------------------- */
/* LORA INIT                                                                   */
/* -------------------------------------------------------------------------- */

static void lora_init(void)
{

    ConfigureLora();


    llcc68_request(LLCC68_RX_CONTINUOUS);


    LOG_INF("LoRa continuous RX started");

}



/* -------------------------------------------------------------------------- */
/* MAIN                                                                        */
/* -------------------------------------------------------------------------- */

int main(void)
{

    usb_init();


    lora_init();


  lora_packet_t pkt; //Creates packet buffer.

while (1)
{
   if (pkt.len >= 2)
{

    uint8_t header1;

    uint8_t qos;

    uint8_t hop_count;

    uint8_t flags;



    /*
       Header1 format

       Bit 7-6 : QoS
       Bit 5-3 : Hop Count
       Bit 2-0 : Flags

    */


    header1 = (uint8_t)pkt.payload[0];


    qos = (header1 >> 6) & 0x03;


    hop_count = (header1 >> 3) & 0x07;


    flags = header1 & 0x07;




    LOG_INF("Header1       : 0x%02X", header1);


    LOG_INF("QoS           : %d", qos);


    LOG_INF("Hop Count     : %d", hop_count);


    LOG_INF("Flags         : %d", flags);



    LOG_INF("DEV_ID        : 0x%02X",
            (uint8_t)pkt.payload[1]);



    LOG_INF("Tunnel ID     : 0x%02X",
            (uint8_t)(pkt.payload[1] & 0xC0));

}
}
}