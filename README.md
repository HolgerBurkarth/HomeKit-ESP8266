# Modern C++ Arduino HomeKit ESP8266

## Apple HomeKit accessory server library for ESP8266 Arduino with useful C++ support

![GitHub Release](https://img.shields.io/github/v/release/HolgerBurkarth/main)  
Author: Holger Burkarth (burkarth at prodad.com)

This Arduino library is a native Apple HomeKit accessory implementation for the
[ESP8266 Arduino core](https://github.com/esp8266/Arduino) and works without any additional bridges.

 > :exclamation: This library was built using the ESP8266 Arduino Core 3.1.2. Lower versions may compile with errors.

## Connect to Apple HomeKit:

A new IoT device can be registered in just a few steps through the Home app.

1. Open the Home app and tap the `+` icon.
2. Use the `More Options` button to add the new device to the list.  
![Begin](./media/de-hk-register01.jpg)

3. Select the new device. If it does not appear, check that
the new device has a WiFi connection to the shared WLAN.  
![Add](./media/de-hk-register02.jpg)  
 The login code `1111-1111` is used in all examples.

4. It may happen that the process fails even though all requirements have been met.
In this case, the registration process must be repeated. Try several times if necessary.  
![Issue](./media/de-hk-register03.jpg)

5. After successful registration, the device can be given a suitable name.  
![Rename](./media/de-hk-register04.jpg)

6. The device is now available in the Home app and can be controlled.  
![Overview](./media/de-hk-register05.jpg)

## Examples

### Garage Door Opener

![Garage Door Opener](./media/en-hp-garage01.jpg)
![Door](./media/en-hw-garage01.jpg)

This example shows a garage door opener that can be controlled by
Apple HomeKit. The device uses the same function as a traditional
button to open the door. An ultrasonic distance sensor monitors
the position of the door to tell Apple HomeKit whether the door
is closed, open, or half open.

Read more: [Garage Door Opener Project](./examples/GarageDoor/README.md)
  
 
## Further technical details

### Storage

* The pairing data is stored in the `EEPROM` address in the ESP8266 Arduino core.
* This project does not use the `EEPROM' library with data cache to reduce memory usage (call flash_read and write directly).
* See comments in `storage.c' and [ESP8266-EEPROM-doc](https://arduino-esp8266.readthedocs.io/en/3.1.2/libraries.html#eeprom).

### Recommended Arduino IDE tools menu Settings

* LwIP variant: `v2 lower memory` (for lower memory usage)
* Debug level: `None` (for less memory usage)
* VTables: `Flash` (may not matter)
* Erase Flash: select `All Flash Contents` on first upload
* CPU frequency: `160MHz` (must)

### Troubleshooting

* Check your serial output. The library will print debug information to the serial port.

### Change Log

#### v2.0.0

* Forked from Mixiaoxiao/Arduino-HomeKit-ESP8266
* Take over the project and make the necessary changes so that it can be compiled under Arduino IDE 2.3.2 (c++14).

