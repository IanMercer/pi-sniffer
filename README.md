# pi-sniffer
This project is a simple sniffer for Bluetooth LE on Raspberry Pi and sender to MQTT. It uses the built-in BlueZ libraries and Bluetooth antenna on a Raspberry Pi (W or 3+) to scan for nearby BLE devices. 
It reports all BLE devices found and their received signal strength (RSSI) to an MQTT endpoint.

# applications
* Detect cell phones entering your home, garden, barn, ...
* Identify cell phones (provided they have been paired with the Raspberry Pi)
* Locate cars, dogs, ... using iBeacons attached to moving objects (reverse of iBeacon normal usage) 
* Gather other advertised data and transmit to MQTT (temperature, fitbit, cycleops, ...)

# goals
* Scan for BLE devices nearby a Raspberry Pi using the built-in Bluetooth adapter
* No external dependencies: no Python, no Node.js, no fragile package dependencies
* Simplicity: do one thing well, no frills

# status
This is an initial very rough commit and proof of concept. I have copied in the essential source and header files to make it compile and run without any external dependencies. There is a `build.sh` file that builds and runs the code. The MQTT topic and server address are currently hard-coded. None of the redundant code has been removed yet.

# plans
* Make the MQTT endpoint configurable by command line parameter
* Instructions for how to configure and install this as a service on a Raspberry Pi
* Maintain a running average for the RSSI and smooth the output somewhat
* Make the RSSI change threshold configurable
* Look into pairing iPhones to eliminate random mac addresses
* Gather other advertised data and transmit to MQTT including temperature, battery, steps, heart rate, ...

# getting started

* clone this repository
* install dependencies:    `sudo apt-get install libglib2.0-dev`
* edit your Mosquitto connection details into scan.c
* build the code:   `sudo ./build/sh`
* edit the .service file to point to the scan executable location:
    `nano pi-sniffer.service`

* copy the service file to systemd:
    `sudo cp pi-sniffer.service /etc/systemd/system/pi-sniffer.service`

* start the service:
    `sudo systemctl start pi-sniffer.service`

* check it's running:
    `sudo systemctl status pi-sniffer.service`



