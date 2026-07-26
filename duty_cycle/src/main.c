/*paste your code heres*/
#include <zephyr/kernel.h>      // Zephyr kernel APIs (k_sleep, threads, timers)
#include <zephyr/device.h>      // Device driver framework
#include <zephyr/logging/log.h>  // Zephyr logging API
#include <zephyr/usb/usb_device.h> //USB device support
#include <zephyr/usb/usbd.h>       //USB stack definitions
 
#include "radio_driver.h"          //LoRa radio configuration functions
#include "llcc68_driver.h"         //LLCC68 radio driver APIs
 
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG); //Register log module with debug level
 
int main(void)
{
    const struct device *dev; //Pointer to the USB console device
 
    /* USB Console */
    dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console)); //  Get the USB console device from the Device Tree 
 
    if (!device_is_ready(dev))    // Verify that the console device is read
    {
        LOG_ERR("Console device not ready");  
        return -1;
    }
 
    if (usb_enable(NULL)) //Enable the USB CDC ACM interface
    {
        LOG_ERR("USB initialization failed");
        return -1;
    }
 
    k_sleep(K_SECONDS(2)); // Wait for the PC to enumerate the USB device
 
    LOG_INF("Initializing LoRa...");
 
    if (ConfigureLora() != 0) // Initialize and configure the LLCC68 LoRa radio
    {
        LOG_ERR("LoRa initialization failed");
        return -1;
    }
    /*
     * Start Continuous Receive Mode
     * The radio continuously listens for incoming LoRa packets.
     
 
    LOG_INF("=========================================="); 
    LOG_INF("   LLCC68 RX DUTY CYCLE MODE STARTED");
    LOG_INF("==========================================");
 
    /*
     * Start RX Duty Cycle
     *
     * RX Window : 10 ms
     * Sleep     : 40 ms
     *
     * Change these values as required.
     */
    //llcc68_listen(10, 40);
    llcc68_request(LLCC68_RX_CONTINUOUS); //Start continuous receive mode
    k_sleep(K_MSEC(100)); //Give the radio time to enter RX mode
 
    LOG_INF("Mode = 0x%02X", llcc68_getMode()); //Display the current operating mode of the radio
 
    LOG_INF("Radio is listening...");
    LOG_INF("MCU entering sleep...");
 
   while (1) //Main receive loop
{
    uint16_t irq = 0;
    llcc68_getIrqStatus(&irq); // Read the current interrupt status from the radio 
 
    if (irq & LLCC68_IRQ_RX_DONE) //Check if a packet has been successfully received 
    {
        uint8_t payload[256];
        uint8_t len, start;
 
        llcc68_getRxBufferStatus(&len, &start); //Get the received packet length and buffer start address
        llcc68_read_buffer(start, payload, len); //Read the received packet from the radio buffer
 
        LOG_INF("RX DONE: %d bytes", len); //Print the received packet length 
        LOG_HEXDUMP_INF(payload, len, "Payload"); //Print the received payload in hexadecimal format
    }
 
    if (irq & LLCC68_IRQ_CRC_ERR) // Check whether the received packet has a CRC error
        LOG_WRN("CRC error"); //Check whether a receive timeout has occurred
 
    if (irq & LLCC68_IRQ_TIMEOUT) //Clear all interrupt flags that were detected
        LOG_WRN("RX timeout");
 
    if (irq)
    {
        llcc68_clearIrqStatus(irq);   /* always clear what you read */
    }
 
    LOG_INF("IRQ=0x%04X Mode=0x%02X", irq, llcc68_getMode()); //Print the interrupt status and current radio mode
    k_sleep(K_MSEC(2000));            /* poll faster while debugging */
}
}
