import time
import os
import sqlite3
import busio
import board
import digitalio
import adafruit_ssd1306
import adafruit_rfm9x
from digitalio import DigitalInOut, Direction, Pull

DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'lora_data.db')
RADIO_FREQ_MHZ = 915.0
NODE_ID = 1  # The Gateway (Pi)

def setup_database():
    """Initialize SQLite database for telemetry logging."""
    db_conn = sqlite3.connect(DB_PATH)
    cursor = db_conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            from_node INTEGER,
            to_node INTEGER,
            seq_id INTEGER,
            rssi INTEGER,
            snr REAL,
            payload TEXT
        )
    ''')
    db_conn.commit()
    return db_conn, cursor

def setup_oled():
    """Setup SSD1306 OLED Screen and physical buttons."""
    i2c = busio.I2C(board.SCL, board.SDA)
    reset_pin = DigitalInOut(board.D4)
    display = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c, reset=reset_pin)
    display.fill(0)
    display.show()
    
    # Setup Button A
    btnA = DigitalInOut(board.D5)
    btnA.direction = Direction.INPUT
    btnA.pull = Pull.UP
    return display, btnA

def setup_radio():
    """Initialize the RFM9x LoRa SPI Radio module."""
    CS = digitalio.DigitalInOut(board.CE1)
    RESET = digitalio.DigitalInOut(board.D25)
    spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
    rfm9x = adafruit_rfm9x.RFM9x(spi, CS, RESET, RADIO_FREQ_MHZ)
    rfm9x.node = NODE_ID
    return rfm9x

def log_and_save_packet(db_conn, cursor, display, header_from, header_to, header_id, flags, payload, rssi, snr):
    """Log packet to the terminal, OLED display, and SQLite database."""
    timestamp = time.strftime('%H:%M:%S', time.localtime())
    iso_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    
    print(f"\n========================================")
    print(f"[{timestamp}] MESH PACKET RECEIVED")
    print(f"From: Node {header_from}   To: Node {header_to}   SeqID: {header_id}")
    print(f"Flags: {hex(flags)}     Length: {len(payload)} bytes")
    print(f"Signal: RSSI {rssi} dBm   SNR {snr} dB")
    
    try:
        payload_text = str(payload, 'utf-8')
        print(f"Payload (UTF-8): {payload_text}")
    except UnicodeDecodeError:
        payload_text = str(payload)
        print(f"Payload (RAW Bytes): {payload}")
    print("========================================\n")
    
    # Update OLED Display
    display.fill(0)
    display.text(f"N{header_from}->N{header_to} S:{header_id}", 0, 0, 1)
    display.text(payload_text[:20], 0, 10, 1) # Trim for OLED
    display.text(f"R:{rssi}dBm S:{snr}dB", 0, 20, 1)
    display.show()
    
    # Save to SQLite
    try:
        cursor.execute('''
            INSERT INTO telemetry (timestamp, from_node, to_node, seq_id, rssi, snr, payload)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        ''', (iso_time, header_from, header_to, header_id, rssi, snr, payload_text))
        db_conn.commit()
    except Exception as e:
        print(f"Failed to save to SQLite database: {e}")

def main():
    db_conn, cursor = setup_database()
    display, btnA = setup_oled()
    
    print("Initializing LoRa Radio...")
    try:
        rfm9x = setup_radio()
        print("OLED and LoRa initialized successfully.")
        display.text('LoRa Init OK', 0, 0, 1)
        display.text(f'Freq: {RADIO_FREQ_MHZ} MHz', 0, 10, 1)
        display.text('Waiting for packet...', 0, 20, 1)
        display.show()
    except RuntimeError as error:
        print('Failed to initialize LoRa radio:', error)
        display.text('RFM9x Init Error!', 0, 0, 1)
        display.show()
        return

    # Keep track of recent sequence IDs to prevent repeating duplicate packets
    seen_messages = {}
    print(f"Node {NODE_ID} Listening for packets...")
    
    while True:
        packet = rfm9x.receive(with_header=True, timeout=1.0)
        
        if packet is not None and len(packet) >= 4:
            header_to, header_from, header_id, header_flags = packet[0], packet[1], packet[2], packet[3]
            payload = packet[4:]
            
            # Deduplication Check
            if header_from in seen_messages and seen_messages[header_from] == header_id:
                continue
            seen_messages[header_from] = header_id
            
            # Mesh Relaying Logic
            if header_to != 255 and header_to != NODE_ID:
                print(f"Relaying packet from Node {header_from} -> Node {header_to}")
                rfm9x.send(payload, destination=header_to, node=header_from, identifier=header_id, flags=header_flags, keep_listening=True)
                
            log_and_save_packet(db_conn, cursor, display, header_from, header_to, 
                              header_id, header_flags, payload, rfm9x.last_rssi, rfm9x.last_snr)
        
        # Optional check: Clear screen with physical Button A press
        if not btnA.value:
            display.fill(0)
            display.text('Screen Cleared', 0, 0, 1)
            display.show()
            time.sleep(0.5)
            display.fill(0)
            display.text('Waiting for packet...', 0, 0, 1)
            display.show()

if __name__ == "__main__":
    main()
