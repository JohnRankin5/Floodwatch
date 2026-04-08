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
    
    # Mesh Node Configuration
    NODE_ID = 1  # The Gateway (Pi)
    rfm9x.node = NODE_ID
    
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

# Mesh message tracker (OriginNodeID -> LastMessageID)
seen_messages = {}

# Wait to receive packets
print(f"Node {NODE_ID} Listening for packets...")
while True:
    packet = rfm9x.receive(with_header=True, timeout=1.0)  # Wait for a packet with header
    
    if packet is not None and len(packet) >= 4:
        # Received a packet! Parse the RadioHead header
        header_to = packet[0]
        header_from = packet[1]
        header_id = packet[2]
        header_flags = packet[3]
        payload = packet[4:]
        
        # Mesh Deduplication Check
        if header_from in seen_messages and seen_messages[header_from] == header_id:
            # Already saw this message, drop it to prevent infinite storm
            continue
            
        seen_messages[header_from] = header_id
        display.fill(0)
        
        # Mesh Relaying Logic (Option B):
        # If this packet is not meant for us and not broadcast, relay it!
        if header_to != 255 and header_to != NODE_ID:
            print(f"Relaying packet from Node {header_from} -> Node {header_to}")
            rfm9x.send(payload, destination=header_to, node=header_from, identifier=header_id, flags=header_flags, keep_listening=True)
            
        # Gather more info
        RSSI = rfm9x.last_rssi
        SNR = rfm9x.last_snr
        timestamp = time.strftime('%H:%M:%S', time.localtime())
        
        # Terminal detailed logging
        print("\n" + "="*40)
        print(f"[{timestamp}] MESH PACKET RECEIVED")
        print(f"From: Node {header_from}   To: Node {header_to}   SeqID: {header_id}")
        print(f"Flags: {hex(header_flags)}     Length: {len(payload)} bytes")
        print(f"Signal: RSSI {RSSI} dBm   SNR {SNR} dB")
        
        try:
            payload_text = str(payload, 'utf-8')
            print(f"Payload (UTF-8): {payload_text}")
            display.text(f"N{header_from}->N{header_to} S:{header_id}", 0, 0, 1)
            display.text(payload_text, 0, 10, 1)
        except UnicodeDecodeError:
            print(f"Payload (RAW Bytes): {payload}")
            display.text(f"N{header_from}->N{header_to} S:{header_id}", 0, 0, 1)
            display.text(f"RAW: {len(payload)}B", 0, 10, 1)
        print("="*40 + "\n")
            
        # Squeeze Signal metrics onto the bottom of the tiny OLED
        display.text(f"R:{RSSI}dBm S:{SNR}dB", 0, 20, 1)
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
