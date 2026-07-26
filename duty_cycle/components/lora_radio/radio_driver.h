#ifndef __ISC_RADIO_DRIVER__
#define __ISC_RADIO_DRIVER__

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include "llcc68_driver.h"
#include <zephyr/kernel.h>
#include <stdio.h>

struct LoraConfig {
    uint8_t sf;
    uint32_t bw;
    uint8_t cr;
    bool ldro;
    uint8_t headerType;
    uint16_t preambleLength;
    uint8_t payloadLength;
    bool crcType;
    bool invertIq;
    uint32_t LoraFrequency; // Frequency in Hz
    uint8_t txPower; // TX power in dBm
};

/**
 * @brief Initializes the LoRa radio with default settings.
 * 
 * @return 0 on success, negative error code on failure.
 */
int ConfigureLora(void);
/**
 * @brief Transmits data over LoRa Radio.
 * 
 * @param data Pointer to the data to be transmitted.
 * @param length Length of the data to be transmitted.
 * @param count Number of times to transmit the data.
 * @param delay Delay in milliseconds between each transmission.
 */
void TransmitData(char *data, uint8_t length, uint8_t count, uint16_t delay);
/**
 * @brief Configures the LoRa radio with the provided settings.
 * 
 * @param config Pointer to the LoraConfig structure containing the settings.
 * @return 0 on success, negative error code on failure.
 */
int ConfigLoraTx(const struct LoraConfig *config);


void sendData(char *data);

const char* receiveData();

#endif // __ISC_RADIO_DRIVER__

