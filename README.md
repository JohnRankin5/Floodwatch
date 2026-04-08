# LoRa Ultrasonic Mesh Network (UVA Capstone)

A robust, ultra-low-power LoRa based mesh network designed for remote physical distance and water-level sensing. This repository contains the complete hardware schematics, C++ node scripts, and Python Gateway scripts to establish a decentralized "Dumb-Flooding" LoRa mesh that bridges offline field sensors directly into centralized cloud infrastructure.

## 📡 Architecture Overview

This project abandons strict, cloud-dependent LoRaWAN protocols (like TTN) in favor of a high-speed, local **Point-to-Point Mesh Architecture**.

1.  **Field Nodes (Feather M0):** Distributed sensor nodes constantly poll a physical ultrasonic probe and broadcast the readings over 915MHz LoRa.
2.  **Dumb-Flooding Repeaters:** Every field node is programmed to eavesdrop on the network (`promiscuous mode`). If a node hears a remote packet meant for the Gateway that it hasn't seen before, it perfectly duplicates and relays that packet forward, bridging massive geographical dead-zones.
3.  **Central Gateway (Raspberry Pi 5):** The central hub aggregates the cascading LoRa packets, formats them into JSON, and can securely transmit them to AWS/InfluxDB via Wi-Fi, Ethernet, or Cellular LTE-M.

## 🧰 Hardware Requirements

**Central Gateway (Node 1):**
*   Raspberry Pi 5 (4GB / 8GB) with Official 27W USB-C PSU.
*   Adafruit LoRa Radio Bonnet (RFM95W @ 915MHz)
*   915MHz SMA Antenna & uFL Adapter.

**Field Sensor Nodes (Node 2+):**
*   Adafruit Feather M0 RFM95 LoRa
*   DFRobot SEN0311 Ultrasonic Water Level Sensor (IP67)
*   3.7V LiPo Battery (2000mAh+)
*   IP67 Weatherproof Junction Box
*   Surface Mount uFL Connector (Adafruit #1661)

## 💻 Repository Structure

*   **/src/main.cpp**: The C++ Arduino code for the Feather M0 nodes. Uses the `RadioHead` library. Contains built-in Mesh Repeater logic and a dedicated `MOCK_SENSOR_MODE` toggle to rapidly test the mesh without physically connecting ultrasonic probes.
*   **/gateway/lora_receive.py**: The CircuitPython script running on the Raspberry Pi 5 Gateway. Handles the OLED display output and the `seen_messages` deduplicator logic.
*   **/platformio.ini**: The strict compiler definitions to flash the Adafruit Feather M0 seamlessly.

## 🚀 Setup & Installation

### 1. Flash the Sensor Nodes
1. Install PlatformIO in VSCode.
2. Open `src/main.cpp`.
3. Set `MY_NODE_ID` to a unique number (e.g. 2, 3, 4).
4. Click **Build & Upload**.

### 2. Configure the Gateway 
1. Boot the Raspberry Pi 5.
2. Open a terminal and install the required radio packages:
   ```bash
   pip3 install adafruit-circuitpython-rfm9x adafruit-circuitpython-ssd1306
   ```
3. Run the gateway listener:
   ```bash
   python3 gateway/lora_receive.py
   ```

*(Note: Data will instantly begin flooding into the Raspberry Pi terminal and displaying on the mounted OLED screen every 10 seconds!)*
