# Getting Started

* First, if you haven't already, please set up your Raspberry Pi 
following the instructions [here](RaspberrySetup.md).

* update package lists: `sudo apt-get update`
* install GIT if you don't already have it `sudo apt-get install git-core`
* install dependencies: `sudo apt-get install libglib2.0-dev` and `libssl-dev`
* clone the Elcipse PAHO MQTT C source from GIT; 
    `git clone https://github.com/eclipse/paho.mqtt.c.git`
* build and install it
    `sudo make install`
* clone this repository
* edit the .service file to point to the scan executable location:
    `nano pi-sniffer.service`
* build and deploy the code:   `sudo ./build/sh`

* edit the systemd configuration overrides according to the environment
    `sudo systemctl edit pi-sniffer.service`

    For outside use a lower divisor, say 2.5, for inside use a higher divisor, say 3.5
    Keep this value in the range 2.0 - 4.0 and adjust it to get distances reported within range.

    There is also a received power at 1.0m setting, place a device at 1.0m, tail the log and figure this out for your RPi.  Note: not all devices transmit with the same power. There's a correction
    factor for iPads in there but not for all devices yet. The received TXPOWER appears to be fairly useless for most BLE devices.

    You can also set an (x,y,z) coordinate in meters that will be used for trilateration (coming soon)

    Your configuration file should look like this:

````
[Service]
Environment="RSSI_FACTOR=3.5"
Environment="RSSI_ONE_METER=-64"
Environment="POSITION_X=53.0"
Environment="POSITION_Y=20.0"
Environment="POSITION_Z=-6.0"
Environment="HOST_NAME=<room name>"
Environment="HOST_DESCRIPTION=<description>"
Environment="HOST_PLATFORM=Pi3b+"

# Server is formatted: [ssl://]mqtt server:[port]
# For Azure you MUST use ssl:// and :8833

Environment="MQTT_SERVER=192.168.0.52:1883"
Environment="MQTT_TOPIC=BLF"
Environment="MQTT_USERNAME="
Environment="MQTT_PASSWORD="

# Port on which to communicate with sensors in the same group in mesh mode
Environment="UDP_MESH_PORT=7779"

# Port on which to broadcast a count of people present x 10
# If you have multiple sensors in a group, only one should send to the sign
Environment="UDP_SIGN_PORT=7778"

# How to map people to the value sent, e.g. 0.5 so that 4 people = 2.0 sent
# Means that the sign can be configured without deploying new code there
Environment="UDP_SCALE_FACTOR=0.5"

# You can turn logging off entirely by replacing 'all' with specific log domains
# or by removing this line entirely
# And you can adjust the logging level in `build.sh` to exclude debug logging
Environment="G_MESSAGES_DEBUG=all"

````

* Alternatively, instead of using `build.sh` you can run the following steps manually:
* Run `make`
* copy the service file to systemd:
    `sudo cp pi-sniffer.service /etc/systemd/system/pi-sniffer.service`
* enable the service to restart after a reboot:
    `sudo systemctl enable pi-sniffer.service`
* start the service:
    `sudo systemctl start pi-sniffer.service`
* check it's running:
    `sudo systemctl status pi-sniffer.service`

* [optional, on Ubuntu in particular] open firewall so multiple instances can communicate
    `sudo ufw allow 7779/udp`

* Now setup one or more displays. There are several display options depending on your needs.
 
 * A display based on the $20 M5Stack Matrix requiring no hardware skills [this repo](https://github.com/IanMercer/CrowdAlertM5StackMatrix).
 * A display using high-brightness 3W LEDs requiring mechanical and electrical fabrication skills [instructions coming soon](GettingStarted.md).
 * A display using small TFT screens on ESP32 boards, requiring a case but no soldering [instructions coming soon](GettingStarted.md).
 * A display using another Raspberry Pi or other computer [instructions coming soon](GettingStarted.md).
 * A browser-based display [instructions coming soon](GettingStarted.md).

A comparison of the costs and benefits of these different display approaches will be added shortly. Some displays may display details of crowding in different areas, some may display only an aggregate, some may show detailed numbers, some may display based on a single aggregate cut off number. Displays may also apply blending, hysteresis, transitions and other effects to improve their overall impact.



