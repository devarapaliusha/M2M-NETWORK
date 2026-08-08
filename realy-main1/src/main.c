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


LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);



#define LED_PIN 17


#define DEV_ID 0x88



/* Existing tunnel bits */

#define TUNNEL_MASK 0xC0



/* New QoS Header */

#define DEFAULT_QOS      1     // 2 bits
#define DEFAULT_HOP      0     // 3 bits
#define DEFAULT_FLAGS    0     // 3 bits




/* INA219 */

#define INA219_ADDR 0x40

#define REG_CFG     0x00
#define REG_SHUNT   0x01
#define REG_BUS     0x02
#define REG_CAL     0x05





static const struct device *i2c;

static const struct device *gpio0;




/*
 Packet Format

 Byte0 : Command
 Byte1 : Device ID + Tunnel ID  (existing)
 Byte2 : QoS + Hop Count + Flags
 Byte3 : Payload

*/


uint8_t p_hdr[3];



uint16_t p_id = 0x03;


float bat_v = 0.0f;



char *data;


char us[256]={0};



volatile bool timeout=false;






/* ================= DEVICE ID ================= */


void print_device_id(void)
{

uint32_t id0 = NRF_FICR->DEVICEID[0];

uint32_t id1 = NRF_FICR->DEVICEID[1];


printf("Device ID: %08X%08X\n",
       id1,id0);

}






/* ================= TIMER ================= */


void ch_tmr_cb(struct k_timer *timer)
{

LOG_INF("Channel health check timeout, sending...");

timeout=true;

}


K_TIMER_DEFINE(timeout_timer,
               ch_tmr_cb,
               NULL);







/* ================= INA219 ================= */


static int wr(uint8_t reg,uint16_t val)
{

uint8_t b[3]={reg,val>>8,val};


return i2c_write(i2c,
                 b,
                 3,
                 INA219_ADDR);

}




static int rd(uint8_t reg,int16_t *val)
{

uint8_t b[2];


int ret=i2c_write_read(i2c,
                       INA219_ADDR,
                       &reg,
                       1,
                       b,
                       2);



*val=(b[0]<<8)|b[1];


return ret;

}






float battery_voltage(void)
{

static bool init;

int16_t raw;



if(!init)
{

i2c=DEVICE_DT_GET(DT_NODELABEL(i2c0));



if(!device_is_ready(i2c))

return -1.0f;



wr(REG_CAL,4096);

wr(REG_CFG,0x399F);


k_msleep(15);


init=true;

}




rd(REG_SHUNT,&raw);


float shunt=raw*0.01f;



rd(REG_BUS,&raw);


float bus=((uint16_t)raw>>3)*0.004f;



return bus+(shunt/1000.0f);


}







/* ================= HEADER BUILDER ================= */



void header_builder(uint8_t cmd)
{


/*

Byte0 = Command

Byte1 = Device ID + Tunnel ID

Byte2 = QoS/Hop/Flags

*/



p_hdr[0]=cmd;



p_hdr[1]=DEV_ID;



p_hdr[2]=
((DEFAULT_QOS & 0x03)<<6) |
((DEFAULT_HOP & 0x07)<<3) |
(DEFAULT_FLAGS & 0x07);





LOG_INF("Header Built");

LOG_INF("Command : 0x%02X",
        p_hdr[0]);


LOG_INF("Dev ID  : 0x%02X",
        p_hdr[1]);


LOG_INF("QHF     : 0x%02X",
        p_hdr[2]);


}
/* ================= RETRANSMISSION ================= */


void retx()
{

/*
Duplicate check

Byte0 = Command
Byte1 = Device ID/Tunnel
Byte2 = QoS/Hop/Flags

*/


if((data[0] == us[0]) &&
   (data[1] == us[1]) &&
   (data[2] == us[2]))
{

LOG_INF("Duplicate data received, skipping transmission");

return;

}


memcpy(us,data,256);



LOG_INF("New data received, transmitting...");



gpio_pin_set(gpio0,
             LED_PIN,
             1);



sendData(data);



gpio_pin_set(gpio0,
             LED_PIN,
             0);



LOG_INF("Data transmitted: %s",
        data);


}







/* ================= DEVICE HEALTH ================= */



void dev_hlth(void)
{


char dev_rpt[128];

char battery[5];



bat_v=battery_voltage();



LOG_INF("Preparing device health report...");



/*
 Build Header

Command = 0x20
*/

header_builder(0x20);




snprintf(battery,
         sizeof(battery),
         "%s",
         bat_v < 4.00f ? "BL":"OK");





/*
 Copy Header

Byte0 Command
Byte1 Device ID
Byte2 QoS/Hop/Flags

*/


memcpy(dev_rpt,
       p_hdr,
       sizeof(p_hdr));




/*
 Payload after header

Byte3 onwards

*/


memcpy(dev_rpt + sizeof(p_hdr),
       battery,
       strlen(battery)+1);






LOG_INF("========== DEVICE HEALTH REPORT ==========");



LOG_INF("Command : 0x%02X",
        (uint8_t)dev_rpt[0]);



LOG_INF("Dev ID  : 0x%02X",
        (uint8_t)dev_rpt[1]);



LOG_INF("Tunnel ID : 0b%02x",
        dev_rpt[1] & TUNNEL_MASK);





uint8_t qos;

uint8_t hop;

uint8_t flags;



qos =
(dev_rpt[2] >> 6) & 0x03;


hop =
(dev_rpt[2] >> 3) & 0x07;


flags =
dev_rpt[2] & 0x07;





LOG_INF("QHF Byte : 0x%02X",
        (uint8_t)dev_rpt[2]);



LOG_INF("QoS       : %d",
        qos);



LOG_INF("Hop Count : %d",
        hop);



LOG_INF("Flags     : %d",
        flags);



LOG_INF("Battery   : %s",
        &dev_rpt[3]);





gpio_pin_set(gpio0,
             LED_PIN,
             1);



sendData(dev_rpt);



gpio_pin_set(gpio0,
             LED_PIN,
             0);



}
//id="main_part"
int main(void)
{

const struct device *dev;



usb_enable(NULL);



/* ================= GPIO ================= */


gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));


if(!device_is_ready(gpio0))
{

LOG_ERR("GPIO device not ready");

return 0;

}



gpio_pin_configure(gpio0,
                   LED_PIN,
                   GPIO_OUTPUT);





/* ================= Console ================= */


dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));



if(!device_is_ready(dev))
{

return 0;

}



k_sleep(K_SECONDS(2));



print_device_id();





/* ================= LORA ================= */


ConfigureLora();



llcc68_request(LLCC68_RX_CONTINUOUS);



LOG_INF("Continuous RX started");




/* Timer 5 minutes */


k_timer_start(&timeout_timer,
              K_MINUTES(5),
              K_MINUTES(5));





while(true)
{


data = receiveDataContinuous();




if(data == NULL)
{


if(timeout)
{

timeout=false;

dev_hlth();

}



k_msleep(50);

continue;


}






gpio_pin_toggle(gpio0,
                LED_PIN);




LOG_INF("===========RELAY RECEIVED===========");





/* Existing command */


LOG_INF("command received: 0x%02X",
        data[0]);





/* Existing device ID */


LOG_INF("dev_id: 0x%02X",
        data[1]);





/* Existing tunnel ID */


LOG_INF("tunnel_id: 0b%02x",
        data[1] & TUNNEL_MASK);








/* ================= NEW QOS ================= */


uint8_t qos;

uint8_t hop;

uint8_t flags;



qos =
(data[2] >> 6) & 0x03;



hop =
(data[2] >> 3) & 0x07;



flags =
data[2] & 0x07;




LOG_INF("QHF byte : 0x%02X",
        data[2]);



LOG_INF("QoS       : %d",
        qos);



LOG_INF("Hop Count : %d",
        hop);



LOG_INF("Flags     : %d",
        flags);






/* Tunnel validation */


if((data[1] & TUNNEL_MASK) !=
   (DEV_ID & TUNNEL_MASK))
{

LOG_INF("Invalid Tunnel ID received");

k_msleep(50);

continue;

}







/* Device validation */


if(data[1] != DEV_ID)
{

LOG_INF("Invalid Device ID received");

k_msleep(50);

continue;

}







/* Command Handling */


switch(data[0] & 0xF8)
{


case 0x00:


retx();


break;





case 0x18:


dev_hlth();


break;





case 0x20:


LOG_INF("Device health report received, retx");


retx();


break;





default:


LOG_INF("Unknown command received: 0x%02X",
        data[0]);


break;



}




k_msleep(50);



gpio_pin_toggle(gpio0,
                LED_PIN);



}



return 0;

}