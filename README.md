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

It actually transmits a count for each range (1, 2, 5, ...)
![image](https://user-images.githubusercontent.com/347540/86996091-8ffd0880-c15f-11ea-991b-8a613041e4a0.png)

# MQTT topics

The mqqt packet contains bytes as follows:

    00-05  access_point_address
    06-13  local time stamp (long seconds)
    14-..  data

For a specific BLE device the following topics containing that device's mac address are sent:

    BLF/<hostname>/56:DA:8D:18:25:8D/distance   -- the distance in meters as a string (rough, calculated from RSSI)
    BLF/<hostname>/56:DA:8D:18:25:8D/name       -- the name (or for unknown devices a best effort like 'iPhone' or 'Beacon')
    BLF/<hostname>/56:DA:8D:18:25:8D/alias      -- the BLUEZ alias (currently disabled)
    BLF/<hostname>/56:DA:8D:18:25:8D/power      -- the power level (currently disabled)
    BLF/<hostname>/56:DA:8D:18:25:8D/type       -- the mac address type: `public` or `random`
   
For a summary of all devices seen by the access point the following topic is sent:

    BLF/<hostname>/summary/dist_hist             -- an array of bytes containing the count of devices at each range 
   
# Time

Given delays in MQTT transmit, receive and re-transmit to the receiving application it's a good idea to use the timestamp passed in the packet. Make
sure all your Pis are synchronized to the same time.

# status
* Recently updated to use the Eclipse PAHO MQTT C library which support SSL and Async code. This is the only dependency.
* This is a work in progress and is still changing fairly rapidly.
* There is a `build.sh` file that builds and runs the code. 
* The MQTT topic prefix is hard-coded but the MQTT server IP (or FQDN) and port are configurable.
* Environment variables are used to configure the RSSI to distance conversion parameters for indoor/outdoor settings.

# plans
* Look into pairing iPhones to eliminate random mac addresses
* Decode advertised data for common iBeacons that also send environmental data (e.g. Sensoro)
* Gather other advertised data and transmit to MQTT including temperature, battery, steps, heart rate, ...
* Combine multiple Pi RSSI values to do trilateration and approximate location, simple ML model

# getting started

* update package lists: `sudo apt-get update`
* install GIT if you don't already have it `sudo apt-get install git-core`
* install dependencies: `sudo apt-get install libglib2.0-dev` and `libssl-dev`
* clone the Elcipse PAHO MQTT C source from GIT; 
    `git clone https://github.com/eclipse/paho.mqtt.c.git`
* build and install it
    `sudo make install`
* clone this repository
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

    For outside use a lower divisor, say 2.5, for inside use a higher divisor, say 3.5
    Keep this value in the range 2.0 - 4.0.

    There is also a received power at 1.0m setting, place a device at 1.0m, tail the log and figure this out for your RPi.

    Your configuration file should look like this:

````
    [Service]
    Environment="RSSI_FACTOR=3.5"
    Environment="RSSI_ONE_METER=-64"
````
