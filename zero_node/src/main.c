#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <string.h>

#include "llcc68_driver.h"
#include "radio_driver.h"


LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);



#define LED_PIN 17

#define DEV_ID 0x81

#define SEND_INTERVAL 5000



uint8_t commandID = 0x00;

uint8_t rx_last[64];



const struct device *gpio0;



/*
 * Arduino sendAutoPacket()
 * Equivalent Zephyr function
 */

void send_auto_packet(void)
{

    uint8_t packet[64];

    uint8_t header1 = commandID;

    uint8_t header2 = DEV_ID;



    memset(packet,0,sizeof(packet));



    // Header 1
    packet[0] = header1;


    // Header 2
    packet[1] = header2;



    // Payload
    strcpy((char *)&packet[2],"Hello");



    LOG_INF("========== TX PACKET ==========");


    LOG_INF("Command : 0x%02X",header1);


    LOG_INF("DEV_ID  : 0x%02X",header2);


    LOG_INF("DATA    : Hello");



    gpio_pin_set(gpio0,LED_PIN,1);



    sendData(packet);



    gpio_pin_set(gpio0,LED_PIN,0);



    LOG_INF("Packet Sent");


    LOG_INF("===============================");



    /*
       Command ring
       00-07
    */


    commandID++;


    if(commandID > 0x07)
    {
        commandID = 0x00;
    }


}





void process_received(uint8_t *data)
{

    uint8_t header1;

    uint8_t header2;


    header1 = data[0];

    header2 = data[1];



    LOG_INF("========== RX PACKET ==========");



    LOG_INF("Command : 0x%02X",header1);


    LOG_INF("DEV_ID  : 0x%02X",header2);



    LOG_INF("DATA    : %s",&data[2]);



    LOG_INF("===============================");





    /*
       Check Device ID
    */


    if(header2 != DEV_ID)
    {
        LOG_INF("Invalid Device ID");

        return;
    }



    /*
       Retransmit if required
    */


    if((header1 & 0xF8)==0x00)
    {

        LOG_INF("Data packet received");


        sendData(data);

    }


}





int main(void)
{


    usb_enable(NULL);



    gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));


    if(!device_is_ready(gpio0))
    {

        LOG_ERR("GPIO not ready");

        return 0;
    }



    gpio_pin_configure(
        gpio0,
        LED_PIN,
        GPIO_OUTPUT
    );




    ConfigureLora();



    llcc68_request(LLCC68_RX_CONTINUOUS);



    LOG_INF("LoRa RX Started");




    uint32_t last_send = 0;




    while(1)
    {


        /*
          Auto TX every 5 seconds
        */

        if((k_uptime_get_32()-last_send) >= SEND_INTERVAL)
        {

            last_send = k_uptime_get_32();


            send_auto_packet();

        }





        /*
          Continuous RX
        */


        uint8_t *data;


        data = receiveDataContinuous();



        if(data != NULL)
        {

            process_received(data);

        }



        k_msleep(50);

    }



    return 0;

}