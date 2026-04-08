import time
import busio
import board
import digitalio
import adafruit_ssd1306
from digitalio import DigitalInOut, Direction, Pull
import adafruit_rfm9x

# Button A, B, C for the Pi OLED bonnet
btnA = DigitalInOut(board.D5)
btnA.direction = Direction.INPUT
btnA.pull = Pull.UP

btnB = DigitalInOut(board.D6)
btnB.direction = Direction.INPUT
btnB.pull = Pull.UP

btnC = DigitalInOut(board.D12)
btnC.direction = Direction.INPUT
btnC.pull = Pull.UP

# Create the I2C interface for the OLED display
i2c = busio.I2C(board.SCL, board.SDA)

# 128x32 OLED Display
reset_pin = DigitalInOut(board.D4)
display = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c, reset=reset_pin)

# Clear the display
display.fill(0)
display.show()

# Set up LoRa Radio
# For the Adafruit RFM9x Bonnet, CS is CE1 (D7 usually, but use CE1 in board)
# and RESET is D25
CS = digitalio.DigitalInOut(board.CE1)
RESET = digitalio.DigitalInOut(board.D25)
spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)

# Configure the Radio frequency. Adjust if your Feather M0 is different (e.g. 433.0)
RADIO_FREQ_MHZ = 915.0

print("Initializing LoRa Radio...")
try:
    rfm9x = adafruit_rfm9x.RFM9x(spi, CS, RESET, RADIO_FREQ_MHZ)
    print("OLED and LoRa initialized successfully.")
    display.fill(0)
    display.text('LoRa Init OK', 0, 0, 1)
    display.text(f'Freq: {RADIO_FREQ_MHZ} MHz', 0, 10, 1)
    display.text('Waiting for packet...', 0, 20, 1)
    display.show()
except RuntimeError as error:
    print('Failed to initialize LoRa radio:', error)
    display.fill(0)
    display.text('RFM9x Init Error!', 0, 0, 1)
    display.show()
    exit(1)

# Wait to receive packets
print("Waiting for packets...")
while True:
    packet = rfm9x.receive(timeout=2.0)  # Wait for a packet
    
    if packet is not None:
        # Received a packet!
        display.fill(0)
        
        try:
            packet_text = str(packet, 'utf-8')
            print(f"Received (ASCII): {packet_text}")
            display.text("Received!", 0, 0, 1)
            display.text(packet_text, 0, 10, 1)
        except UnicodeDecodeError:
            print(f"Received (Raw Bytes): {packet}")
            display.text("Received (Raw):", 0, 0, 1)
            display.text(str(packet), 0, 10, 1)
            
        RSSI = rfm9x.last_rssi
        print(f"RSSI: {RSSI} dB")
        display.text(f"RSSI: {RSSI} dB", 0, 20, 1)
        display.show()
    
    # Optional check to clear screen with button press
    if not btnA.value:
        display.fill(0)
        display.text('Screen Cleared', 0, 0, 1)
        display.show()
        time.sleep(0.5)
        display.fill(0)
        display.text('Waiting for packet...', 0, 0, 1)
        display.show()
