#include <SPI.h>
#include <LoRa.h>

#define LORA_FREQUENCY 865E6

String inputLine = "";

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

LoRa.setTxPower(22,PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(7);
  LoRa.setPreambleLength(16);
  LoRa.setSyncWord(0x34);
  LoRa.enableCrc();


  
  Serial.println("LoRa Ready");
  Serial.println();
  Serial.println("Enter:");
  Serial.println("HH HH Payload");
  Serial.println("Example:");
  Serial.println("10 07 Hello World");

  LoRa.receive();
}

void loop()
{
  // Read complete line from Serial
  while (Serial.available())
  {
    char c = Serial.read();

    if (c == '\n' || c == '\r')
    {
      if (inputLine.length() > 0)
      {
        sendPacket(inputLine);
        inputLine = "";
      }
    }
    else
    {
      inputLine += c;
    }
  }

  // Receive packet
  int packetSize = LoRa.parsePacket();

  if (packetSize)
  {
    uint8_t header1 = 0;
    uint8_t header2 = 0;

    if (LoRa.available())
      header1 = LoRa.read();

    if (LoRa.available())
      header2 = LoRa.read();

    String payload = "";

    while (LoRa.available())
      payload += (char)LoRa.read();

    Serial.println();
    Serial.println("========== Packet Received ==========");
    Serial.print("command : 0x");
    if (header1 < 0x10) Serial.print("0");
    Serial.println(header1&0xF8, HEX);

     Serial.print("payload_cnt : 0x");
    if (header1 < 0x10) Serial.print("0");
    Serial.println(header1&0x07, HEX);

    Serial.print("DEV_ID : 0x");
    if (header2 < 0x10) Serial.print("0");
    Serial.println(header2&0X07, HEX);

    Serial.print("Payload  : ");
    Serial.println(payload);

    Serial.print("RSSI     : ");
    Serial.println(LoRa.packetRssi());

    Serial.print("SNR      : ");
    Serial.println(LoRa.packetSnr());

    Serial.println("=====================================");

    LoRa.receive();
  }
}

void sendPacket(String line)
{
  int firstSpace = line.indexOf(' ');
  int secondSpace = line.indexOf(' ', firstSpace + 1);

  if (firstSpace == -1 || secondSpace == -1)
  {
    Serial.println("Invalid format.");
    Serial.println("Use: HH HH Payload");
    return;
  }

  String h1Str = line.substring(0, firstSpace);
  String h2Str = line.substring(firstSpace + 1, secondSpace);
  String payload = line.substring(secondSpace + 1);

  uint8_t header1 = (uint8_t)strtoul(h1Str.c_str(), NULL, 16);
  uint8_t header2 = (uint8_t)strtoul(h2Str.c_str(), NULL, 16);

  Serial.println();
  Serial.println("========== Sending ==========");
  Serial.print("Header 1 : 0x");
  if (header1 < 0x10) Serial.print("0");
  Serial.println(header1, HEX);

  Serial.print("Header 2 : 0x");
  if (header2 < 0x10) Serial.print("0");
  Serial.println(header2, HEX);

  Serial.print("Payload  : ");
  Serial.println(payload);

  LoRa.idle();

  LoRa.beginPacket();
  LoRa.write(header1);
  LoRa.write(header2);

  for (int i = 0; i < payload.length(); i++)
    LoRa.write((uint8_t)payload[i]);

  LoRa.endPacket();

  Serial.println("Packet Sent.");
  Serial.println("=============================");

  LoRa.receive();
}