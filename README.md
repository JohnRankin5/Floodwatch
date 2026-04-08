# LoRa Receiver for Raspberry Pi 5

This repository contains the configuration and Python script to act as a LoRa receiver using a Raspberry Pi 5 and the Adafruit RFM9x LoRa Bonnet with OLED.

## Hardware Setup
- **Board:** Raspberry Pi 5
- **Hat:** Adafruit LoRa 900MHz Radio Bonnet with OLED
- **Transmitter:** Adafruit Feather M0 with LoRa (to be documented)

## OS & System Requirements
- **OS:** Ubuntu Server
- **Dependencies:** Python 3.12, SPI enabled, I2C enabled

### Raspberry Pi 5 Specific Configuration
To allow the Adafruit CircuitPython library to manually control the Chip Select (CS) pin for the SPI connection to the LoRa module, you **must disable** the kernel's hardware SPI Chip Select. Without this step, initializing the radio will fail with an `lgpio.error: 'GPIO busy'` error.

1. Edit the boot firmware config:
   ```bash
   sudo nano /boot/firmware/config.txt
   ```
2. Locate `dtparam=spi=on` and add `dtoverlay=spi0-0cs` directly underneath it:
   ```ini
   dtparam=spi=on
   dtoverlay=spi0-0cs
   ```
3. Restart the Pi:
   ```bash
   sudo reboot
   ```

## Installation

1. Clone this repository to your specific project location (`/home/johnrankin/lora-receiver`).
2. Create and activate a virtual environment:
   ```bash
   python3 -m venv lora_env
   source lora_env/bin/activate
   ```
3. Install the dependencies (this includes `rpi-lgpio` which is critical for Raspberry Pi 5):
   ```bash
   pip install -r requirements.txt
   ```

*Note: The script requires the Adafruit `font5x8.bin` file to render text onto the OLED correctly on generic linux boards. This file is included in this repository and must reside in the directory from which you execute the Python script.*

## Running the Receiver

Ensure you are in the project folder and the virtual environment is activated. 
Accessing the hardware GPIO requires elevated permissions, so execute it using `sudo`:

```bash
cd /home/johnrankin/lora-receiver
sudo lora_env/bin/python3 lora_receive.py
```

If everything is wired and configured correctly, the OLED will say "LoRa Init OK" and it will start listening for packets from your Feather M0!
