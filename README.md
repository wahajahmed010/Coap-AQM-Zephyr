# Air Quality Monitoring over CoAP and Thread Mesh using Zephyr RTOS

This project was developed as part of my coursework for the *Smart Sensors and Networked Systems* module in the M.Sc. program in High Integrity Systems. The objective was to integrate multiple embedded systems concepts — including sensor interfacing, real-time constraints, low-power mesh networking, and protocol-level communication — using Zephyr RTOS and OpenThread.

## Project Overview

This system monitors air quality using multiple sensors and sends structured JSON data to a server over a CoAP (Constrained Application Protocol) layer built on top of an OpenThread mesh network. It uses the Nordic Semiconductor nRF21540 DK (with an nRF52840 SoC) on both client and server nodes.

The project includes three components:

- **Air Quality Monitor + CoAP Client (Merged Application)**:  
  Collects sensor data and sends JSON payloads via CoAP POST to a remote CoAP server over Thread.

- **CoAP Server (Thread endpoint)**:  
  Receives and logs CoAP payloads at a specific URI (`/storedata`).

- **Standalone CoAP Client (initial test app)**:  
  Sends a static CoAP POST on button press for early validation (later merged into main app).

## Hardware Used

- **Boards**:  
  - 2× [Nordic nRF21540 DK](https://www.nordicsemi.com/products/development-hardware/nrf21540-dk) (with nRF52840 SoC)
  
- **Sensors** (connected to the client board):
  - **SCD41**: CO₂, temperature, and humidity sensor
  - **CCS811**: eCO₂ and TVOC sensor
  - **SPS30**: Particulate Matter (PM1.0, PM2.5, etc.) sensor

## Software Stack

- **RTOS**: [Zephyr RTOS](https://zephyrproject.org) via nRF Connect SDK v2.6.2
- **Mesh Network**: OpenThread (FTD mode)
- **Transport Protocol**: UDP over IPv6
- **Application Layer**: CoAP (non-observe, non-secure, non-blockwise)
- **Build Tool**: West and CMake
- **Toolchain**: Zephyr SDK 0.16.5, NCS Toolchain (`cf2149caf2`)
- **Logging and Shell**: Zephyr Shell + deferred logging

## Project Structure

```
air-quality-thread/
├── coap-aq-client/       # Main sensor + CoAP app
│   ├── src/main.c
│   ├── prj.conf
│   ├── nrf52840dk.overlay
│   └── README.md
├── coap-server/          # CoAP receiver
│   ├── src/main.c
│   ├── prj.conf
│   ├── nrf52840dk.overlay
│   └── README.md
├── .gitignore
└── README.md             # This file
```

## Features

### CoAP Client + Sensor Node

- Reads structured data from SCD41, CCS811, and SPS30
- Formats output into compact JSON (e.g. `{"co2":616.0,"temp":24.3,...}`)
- Sends POST requests to:
  ```
  coap://[mesh-local-prefix]::0001/storedata
  ```
- Sends on every sensor polling cycle using plain `send()` (no threads or async)
- Uses UDP over Thread mesh

### CoAP Server Node

- Binds to a UDP socket
- Initializes a CoAP resource at `/storedata`
- Logs incoming payloads with timestamps

## Configuration Highlights

All OpenThread nodes are configured to use the same:

```ini
CONFIG_OPENTHREAD_NETWORK_NAME="WSN69"
CONFIG_OPENTHREAD_PANID=10000
CONFIG_OPENTHREAD_XPANID="fb:02:00:00:ab:cd:00:01"
CONFIG_OPENTHREAD_NETWORKKEY="00:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:ff"
CONFIG_OPENTHREAD_CHANNEL=20
```

Ensure `CONFIG_OPENTHREAD_MANUAL_START=n` to let Zephyr start the mesh automatically.

## Build Instructions

1. Navigate to either app directory (e.g. `coap-aq-client/`)
2. Run:
   ```sh
   west build -b nrf21540dk_nrf52840 --pristine
   west flash
   ```
3. Open serial terminal (e.g. via `nRF Connect Serial Terminal`) to monitor logs

## Observations & Learnings

- Working with multiple I²C sensors under a unified polling cycle required tight control over timing and resource usage.
- CoAP in Zephyr is minimalistic — integration required careful socket and packet structuring with `coap_packet_init`, `coap_packet_append_option`, and `sendto()`.
- Thread network formation is sensitive to channel and address prefix mismatches. Matching `PANID`, `XPANID`, `CHANNEL`, and `MESH_LOCAL_PREFIX` is critical.
- Debugging mesh packet delivery was done via `ot ping` and serial logs, including packet loss stats and payload verification.
- Keeping the system single-threaded (no work queues or k_thread) helped simplify debugging, especially on the client side.

## Future Improvements

- Add basic CoAP ACK handling or retransmissions
- Offload JSON payloads to cloud via border router or MQTT bridge
- Encrypt payloads or use DTLS for CoAP secure transport

## License

This project was built as an academic exercise for coursework and is shared for reference purposes under the MIT license.
