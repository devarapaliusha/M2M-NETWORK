#include <SPI.h>
#include <LoRa.h>

#define LORA_FREQUENCY 865E6

// Command Ring (00-07)
uint8_t commandID = 0x00;

// Device ID Toggle
bool devToggle = false;

unsigned long lastSendTime = 0;

#define SEND_INTERVAL 5000

/*================ QoS / Hop / Flags ================*/

#define DEFAULT_QOS    1     // 2 bits
#define DEFAULT_HOP    0     // 3 bits
#define DEFAULT_FLAGS  0     // 3 bits

void setup()
{
    Serial.begin(115200);

    while (!Serial);

    Serial.println("Starting LoRa...");

    if (!LoRa.begin(LORA_FREQUENCY))
    {
        Serial.println("LoRa init failed!");
        while (1);
    }

    LoRa.setTxPower(22, PA_OUTPUT_PA_BOOST_PIN);
    LoRa.setSpreadingFactor(10);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(7);
    LoRa.setPreambleLength(16);
    LoRa.setSyncWord(0x34);
    LoRa.enableCrc();

    Serial.println("LoRa Ready");
    Serial.println("Automatic Ring Test Started");

    LoRa.receive();
}

void loop()
{
    /* Automatic Transmit */

    if (millis() - lastSendTime >= SEND_INTERVAL)
    {
        lastSendTime = millis();

        sendAutoPacket();
    }
}

    /* Receive 

    int packetSize = LoRa.parsePacket();

    if (packetSize)
    {
        uint8_t header1 = 0x01;
        uint8_t header2 = 0;
        uint8_t header3 = 0;

        if (LoRa.available())
            header1 = LoRa.read();

        if (LoRa.available())
            header2 = LoRa.read();

        if (LoRa.available())
            header3 = LoRa.read();

        uint8_t qos   = (header3 >> 6) & 0x03;
        uint8_t hop   = (header3 >> 3) & 0x07;
        uint8_t flags = header3 & 0x07;

        String payload = "";

        while (LoRa.available())
        {
            payload += (char)LoRa.read();
        }

        Serial.println();
        Serial.println("========== RX PACKET ==========");

        Serial.print("Command : 0x");
        if (header1 < 0x10)
            Serial.print("0");
        Serial.println(header1, HEX);

        Serial.print("DEV_ID  : 0x");
        if (header2 < 0x10)
            Serial.print("0");
        Serial.println(header2, HEX);

        Serial.print("QHF     : 0x");
        if (header3 < 0x10)
            Serial.print("0");
        Serial.println(header3, HEX);

        Serial.print("QoS     : ");
        Serial.println(qos);

        Serial.print("Hop     : ");
        Serial.println(hop);

        Serial.print("Flags   : ");
        Serial.println(flags);

        Serial.print("DATA    : ");
        Serial.println(payload);

        Serial.print("RSSI    : ");
        Serial.println(LoRa.packetRssi());

        Serial.print("SNR     : ");
        Serial.println(LoRa.packetSnr());

        Serial.println("===============================");

        LoRa.receive();
    }*/

void sendAutoPacket()
{
    uint8_t header1 = commandID;

    // Fixed Device ID
    uint8_t header2 = 0x7F;

    // QoS / Hop / Flags
    uint8_t header3 =
        ((DEFAULT_QOS & 0x03) << 6) |
        ((DEFAULT_HOP & 0x07) << 3) |
        (DEFAULT_FLAGS & 0x07);

    String payload = "Hello";

    Serial.println();
    Serial.println("========== TX PACKET ==========");

    Serial.print("Command : 0x");
    if (header1 < 0x10)
        Serial.print("0");
    Serial.println(header1, HEX);

    Serial.print("DEV_ID  : 0x");
    if (header2 < 0x10)
        Serial.print("0");
    Serial.println(header2, HEX);

    Serial.print("QHF     : 0x");
    if (header3 < 0x10)
        Serial.print("0");
    Serial.println(header3, HEX);

    Serial.print("QoS     : ");
    Serial.println((header3 >> 6) & 0x03);

    Serial.print("Hop     : ");
    Serial.println((header3 >> 3) & 0x07);

    Serial.print("Flags   : ");
    Serial.println(header3 & 0x07);

    Serial.print("DATA    : ");
    Serial.println(payload);

    LoRa.idle();

    LoRa.beginPacket();

    // Byte0 : Command
    LoRa.write(header1);

    // Byte1 : Device ID
    LoRa.write(header2);

    // Byte2 : QoS / Hop / Flags
    LoRa.write(header3);

    // Payload
    for (int i = 0; i < payload.length(); i++)
    {
        LoRa.write((uint8_t)payload[i]);
    }

    LoRa.endPacket();

    Serial.println("Packet Sent");
    Serial.println("===============================");

    // Command Ring (00-07)
    commandID++;

    if (commandID > 0x07)
    {
        commandID = 0x01;
    }

   // LoRa.receive();
}
