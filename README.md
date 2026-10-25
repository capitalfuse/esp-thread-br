# ESP Thread Border Router SDK (ESP32-C6 Single-Chip Edition)

This SDK provides the fundamental network features needed to build a [Thread Border Router](https://openthread.io/guides/border-router) (BR) on a **single ESP32-C6**, which integrates both Wi-Fi 6 and IEEE 802.15.4 radios.

![ESP32-C6 Devkitc-1](docs/images/esp32-c6-devkitc-1-v1.2-annotated-photo.png)

# Added Features

Added a feature to upload firmware to the ESP32 from a browser by selecting a firmware file.
![Firmware update](docs/images/firmware_update.png)

Added Basic authentication and a page for changing the username and password.
![Change username and password](docs/images/change_credentials.png)

# Software Components

The SDK is built on top of [ESP-IDF](https://github.com/espressif/esp-idf) and [OpenThread](https://github.com/openthread/openthread). The Wi-Fi backbone link and the Thread network are both handled by the same ESP32-C6 chip.

# Hardware Platforms

Unlike the conventional Thread BR architecture, which requires two separate SoCs (a host SoC and an RCP), this edition needs only **one ESP32-C6**:

* The Wi-Fi interface serves as the backbone link.
* The built-in IEEE 802.15.4 radio serves as the Thread interface.
* No external RCP, and therefore no UART/SPI wiring, RESET/BOOT pins or PTA coexistence wires between chips.

## Supported Hardware

Any ESP32-C6 development board (e.g. ESP32-C6-DevKitC-1) can be used. Connect the board to your PC via USB and no additional wiring is required.

| Item              | Requirement                                   |
|-------------------|-----------------------------------------------|
| SoC               | [ESP32-C6](https://www.espressif.com/en/products/socs/esp32-c6) |
| Backbone link     | Wi-Fi (2.4 GHz)                               |
| Thread radio      | Built-in IEEE 802.15.4                        |
| Extra components  | None                                          |

# Provided Features

* **Bi-directional IPv6 Connectivity**: The devices on the backbone link (Wi-Fi) and the Thread network can reach each other.
* **Service Discovery Delegate**: The devices on the Thread network can find the mDNS services on the backbone link.
* **Service Registration Server**: The devices on the Thread network can register services to the BR for devices on the backbone link to discover.
* **Multicast Forwarding**: The devices joining the same multicast group on the backbone link and the Thread network can be reached with one single multicast.
* **NAT64**: The devices can access the IPv4 Internet via the BR.
* **Credential Sharing**: The BR can safely share administrative access and allow extracting the network credentials of the network.
* **TREL**: It enables Thread devices to communicate directly over IPv6-based links other than IEEE 802.15.4, such as Wi-Fi.
* **Web GUI**: The BR enables a web server and provides practical functions including Thread network discovery, network formation, status query and topology monitor.
* **Built-in RF Coexistence**: Wi-Fi and Thread share the 2.4 GHz band on the same chip, and coexistence is arbitrated internally, so no external coexistence wiring is needed.

# Resources

* Documentation: https://docs.espressif.com/projects/esp-thread-br/
