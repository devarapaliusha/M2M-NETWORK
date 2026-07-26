#include "radio_driver.h"
volatile bool critical_process = false;

LOG_MODULE_REGISTER(isc_radio, LOG_LEVEL_DBG);

static const struct gpio_dt_spec sx1262_reset = GPIO_DT_SPEC_INST_GET(0, reset_gpios);
static const struct gpio_dt_spec sx1262_dio1 = GPIO_DT_SPEC_INST_GET(0, dio1_gpios);

void DIO1_ISR(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("Interrupt received!");
	uint8_t status = llcc68_status();
	switch (status)
	{
	case LLCC68_STATUS_TX_DONE:
		LOG_INF("TX Done");
		break;
	case LLCC68_STATUS_RX_DONE:
		LOG_INF("RX Done");
		break;
	}
}

static struct gpio_callback DIO1_cb_data;

static int configure_dio1_interrupt(const struct gpio_dt_spec *spec)
{
	int err;
	if (!gpio_is_ready_dt(spec))
	{
		LOG_ERR("DIO1 pin not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(spec, GPIO_INPUT);
	if (err)
	{
		LOG_ERR("Failed to configure DIO1 pin: %d", err);
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(spec, GPIO_INT_EDGE_TO_ACTIVE);
	if (err)
	{
		LOG_ERR("Failed to configure DIO1 interrupt: %d", err);
		return err;
	}

	gpio_init_callback(&DIO1_cb_data, DIO1_ISR, BIT(spec->pin));
	gpio_add_callback(spec->port, &DIO1_cb_data);
	LOG_DBG("Interrupt Configured Successfully");
	return 0;
}

int ConfigureLora(void)
{
	int err;
	peripherals_ready();
	configure_dio1_interrupt(&sx1262_dio1);
	LOG_DBG("Config Done");
	llcc68_reset(&sx1262_reset);
	LOG_DBG("Reset Done");
	k_msleep(100);
	llcc68_setStandby(LLCC68_STDBY_RC);
	llcc68_setRegulatorMode(1);
	uint8_t mode = llcc68_getMode();
	if (mode != LLCC68_STATUS_MODE_STDBY_RC)
	{
		LOG_WRN("Wrong status received! (mode: 0x%X)", mode);
		return -1;
	}
	LOG_DBG("Status confirmed");
	llcc68_setPacketType(LLCC68_PACKETTYPE_LORA);
	llcc68_fixResistanceAntenna();

	uint8_t xtalA = 0x12;
	uint8_t xtalB = 0x12;
	LOG_INF("Set RF module to use XTAL as clock reference");
	llcc68_setXtalCap(xtalA, xtalB);

	LOG_INF("Setting LORA Frequency!");
	llcc68_setFrequency(865000000);

	LOG_INF("Setting TX power");
	llcc68_setTxPower(22, SX126X_TX_POWER_SX1262);

	LOG_INF("Setting modulation parameters");
	uint8_t sf = 10;
	uint32_t bw = 125000;
	uint8_t cr = 7;
	llcc68_LoraModulation(sf, bw, cr, false);

	LOG_INF("Setting Packet Parameters");
	uint8_t headerType = LLCC68_HEADER_EXPLICIT;
	uint16_t preambleLength = 16;
	uint8_t payloadLength = 13;
	bool crcType = true;
	llcc68_setLoraPacket(headerType, preambleLength, payloadLength, crcType, false);

	LOG_INF("Setting sync word");
	llcc68_setSyncWord(0x3444);
	llcc68_setDio2AsRfSwitchCtrl(1);

	LOG_INF("Lora TX configuration complete!");
	return 0;
}



void sendData(char *data)
{

	uint8_t len = strlen(data);
	uint8_t headerType = LLCC68_HEADER_EXPLICIT;
	uint16_t preambleLength = 16;
	uint8_t payloadLength = len;
	bool crcType = true;

	llcc68_setLoraPacket(headerType, preambleLength, payloadLength, crcType, false);

	llcc68_beginPacket();
	llcc68_write_char(data, len);
	llcc68_endPacket(0U);

	llcc68_wait(0U);
	uint32_t time = llcc68_transmitTime();
	LOG_INF("Transmit Time: %d ms", time);
	llcc68_request(LLCC68_RX_CONTINUOUS);


	return;

}
int ConfigLoraTx(const struct LoraConfig *config)
{
	if (config == NULL)
	{
		LOG_ERR("Unsupported configuration");
		return -EINVAL;
	}

	llcc68_setFrequency(config->LoraFrequency);
	llcc68_setTxPower(config->txPower, SX126X_TX_POWER_SX1262);
	llcc68_LoraModulation(config->sf, config->bw, config->cr, config->ldro);
	llcc68_setLoraPacket(config->headerType, config->preambleLength, config->payloadLength, config->crcType, config->invertIq);
	llcc68_setSyncWord(0x3444); // Default sync word

	LOG_INF("LoRa TX configuration complete with SF: %d, BW: %d, CR: %d", config->sf, config->bw, config->cr);
	return 0;
}

const char* receiveData()
{
		LOG_INF("Waiting to receive data...");
		llcc68_request(LLCC68_RX_SINGLE);
		LOG_INF("Receive request sent, waiting for data...");
		llcc68_wait(0U);
		LOG_INF("Data received, reading...");
		uint8_t msgLen = llcc68_available();
		static char message[256]; // +1 for null terminator
		// uint8_t counter;
		llcc68_read_char(message, msgLen);
		message[msgLen] = '\0'; // Null terminate the string
		// counter = llcc68_read();
		// llcc68_read_data(&counter, 1);
		LOG_INF("Message: %s", message);
		LOG_INF("Packet Status: RSSI = %i dBm | SNR: %.2i", llcc68_packetRssi(), (int8_t)llcc68_snr());
		// LOG_INF("Packet Status: RSSI = %i dBm | SNR: %.2i", llcc68_rssiInst(), (int8_t)llcc68_snr());

		uint8_t status = llcc68_status();
		if (status == LLCC68_STATUS_CRC_ERR)
		{
			LOG_WRN("CRC Error");
		}
		else if (status == LLCC68_STATUS_HEADER_ERR)
		{
			LOG_WRN("Packet header error");
		}
		return message;
		// k_msleep(500);
	
}


const char *receiveDataContinuous(void)
{
    static char message[256];

    /* Wait only a short time for an RX interrupt */
    if (!llcc68_wait(10))
    {
        return NULL;
    }

    if (llcc68_status() != LLCC68_STATUS_RX_DONE)
    {
        return NULL;
    }

    uint8_t msgLen = llcc68_available();

    if (msgLen == 0)
    {
        return NULL;
    }

    llcc68_read_char(message, msgLen);
    message[msgLen] = '\0';

    LOG_INF("Message: %s", message);
    LOG_INF("RSSI=%d SNR=%d",
            llcc68_packetRssi(),
            (int8_t)llcc68_snr());

    return message;
}