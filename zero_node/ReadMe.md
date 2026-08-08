## Usage of LoRa

### 1. Building the Firmware

Make sure you have nRF Connect SDK and all dependencies installed.

```sh
west build -b ISC_4012_B .
west flash
```

### 2. Connecting to the Device

- Connect the ISC-4012-B module to your PC via USB.
- Open a serial terminal (e.g., `minicom`, `screen`, or Zephyr's built-in shell) at 115200 baud.
- You should see the shell prompt:  
  ```
  ISC_4012_B:~$
  ```

### 3. LoRa Shell Commands

The firmware provides a shell interface for LoRa radio testing and configuration.

#### Configure LoRa TX

```sh
lora config <sf> <bw> <cr> <ldro> <headerType> <preambleLength> <payloadLength> <crcType> <invertIq> <frequency> <txPower>
```
- Example:
  ```
  lora config 10 125000 5 false 0 16 13 true false 865000000 22
  ```
- All parameters must be provided. See code for details on each.

#### Transmit Data

```sh
lora transmit <data> <count> <delay_ms>
```
- Example:
  ```
  lora transmit Hello 5 1000
  ```
  This will send "Hello" 5 times with 1000 ms delay between transmissions.

### 4. Notes

- The LoRa radio is initialized at boot with default parameters.
- Use the `lora config` command to change LoRa parameters before transmitting.
- The shell will print logs for each transmission and configuration step.

---

For more details, refer to the source code and comments in `src/main.c` and `components/lora_radio/radio_driver.c`.