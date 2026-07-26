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



    while(1)
    {

        char *data;


        data = receiveDataContinuous();



        if(data != NULL)
        {

            char tx_data[256];


            snprintf(tx_data,
                     sizeof(tx_data),
                     "UID:0x%04X,%s",
                     UID,
                     data);



            printk("Received: %s\n", tx_data);



            /*
             * Send through LoRa
             */
            sendData(tx_data);


        }



        k_sleep(K_MSEC(100));

    }


    return 0;
}