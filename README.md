# pi-sniffer
This project is a simple sniffer for Bluetooth LE on Raspberry Pi and sender to MQTT. It uses the built-in BlueZ libraries and Bluetooth antenna on a Raspberry Pi (W or 3+) to scan for nearby BLE devices. 
It reports all BLE devices found (Mac address, name, type, UUIDs, ...) and their approximate distance to an MQTT endpoint. It applies a simple
Kalman filter to smooth the distance values. It also handles iPhones and other Apple devices that randomize their mac addresses periodically and can give a reliable count of how many phones/watches/... are in-range.

![image](https://user-images.githubusercontent.com/347540/85953280-1cb7f300-b924-11ea-96d5-07c217a57e24.png "Multiple Pis and many BLE devices in action")
![image](https://user-images.githubusercontent.com/347540/85953412-dd3dd680-b924-11ea-8eeb-a3b328f91d19.png "A single stationary device")

# applications
* Detect cell phones entering your home, garden, barn, ...
* Put the heating or air conditioning on when there are two or more cellphones in the house and off otherwise
* Locate cars, dogs, ... using iBeacons attached to moving objects (reverse of iBeacon normal usage) 
* Gather other advertised data and transmit to MQTT (temperature, fitbit, cycleops, ...)

# goals
* Scan for BLE devices nearby a Raspberry Pi using the built-in Bluetooth adapter
* No external dependencies: no Python, no Node.js, no fragile package dependencies
* Simplicity: do one thing well, no frills

# iOS MAC address randomization
Tracking iOS (and many Android) devices is complicated by the fact that they switch Mac addresses unpredictably: sometimes after a few seconds, sometimes after many minutes. You can see two MAC address swaps in this example:
![image](https://user-images.githubusercontent.com/347540/85953525-cc419500-b925-11ea-9693-012aeaa61b60.png)

There is no (easy) way to distinguish a MAC-flipping event from a new device arrival event. Until the old mac address pings again they could be the same device.
The pi-sniffer code includes an algorithm to calculate the minimum possible number of devices present assuming that any overlap in time means two sequences are different devices, but otherwise packing them all together like events on a calendar to find the minimum possible number of devices present.

Pi-sniffer transmits this min-count every time it changes so you can easily see how many devices are in range of any of your RPi devices.
![image](https://user-images.githubusercontent.com/347540/85953581-54279f00-b926-11ea-8d02-fb155d409f61.png)


# status
This is a work in progress and is still changing fairly rapidly.
It includes the essential source and header files to make it compile and run without any external dependencies. 
There is a `build.sh` file that builds and runs the code. 
The MQTT topic prefix is hard-coded but the MQTT server IP (or FQDN) and port are configurable.
The environment value (2.0-4.0) used to calculate distance from RSSI will be configurable later for indoor / outdoor scenarios.

# plans
* Look into pairing iPhones to eliminate random mac addresses
* Decode advertised data for common iBeacons that also send environmental data (e.g. Sensoro)
* Gather other advertised data and transmit to MQTT including temperature, battery, steps, heart rate, ...
* Combine multiple Pi RSSI values to do trilateration and approximate location, simple ML model

# getting started

* clone this repository
* install dependencies:    `sudo apt-get install libglib2.0-dev`
* edit your Mosquitto connection details into the last line of build.sh that launches the sniffer
* build the code:   `sudo ./build/sh`
* try the sniffer: ./scan <mqtt server ip> [<port>]
* edit the .service file to point to the scan executable location:
    `nano pi-sniffer.service`

* copy the service file to systemd:
    `sudo cp pi-sniffer.service /etc/systemd/system/pi-sniffer.service`

* enable the service to restart after a reboot:
    `sudo systemctl enable pi-sniffer.service`

* start the service:
    `sudo systemctl start pi-sniffer.service`

* check it's running:
    `sudo systemctl status pi-sniffer.service`

* [optional] edit the configuration according to the environment
    `sudo systemctl edit pi-sniffer.service`

    For outside use a lower divisor, say 3.0, for inside use a higher divisor, say 3.5
    Keep this value in the range 2.0 - 4.0.
    There is also a received power at 1.0m setting, tail the log and figure this out for your RPi.

    Your configuration file should look like this:

    `[Service]`
    `Environment="RSSI_ONE_METER=-64"`
    `Environment="RSSI_FACTOR=3.5"`
