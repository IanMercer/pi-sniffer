# Getting Started

* First, if you haven't already, please set up your Raspberry Pi 
following the instructions [here](RaspberrySetup.md).

* update package lists:

`sudo apt-get update`

* install GIT if you don't already have it

`sudo apt-get install git-core`

* install dependencies: 

`sudo apt-get install libglib2.0-dev`

`sudo apt-get install libssl-dev`

* clone the Eclipse PAHO MQTT C source

`git clone https://github.com/eclipse/paho.mqtt.c.git`

* build and install it

`cd paho.mqtt.c`

`sudo make install`

`cd ..`

* clone this repository

`git clone https://github.com/ianmercer/pi-sniffer`

* edit the .service file to point to the scan executable location by editing the `ExecStart` and `WorkingDirectory` lines:

`cd pi-sniffer`

`nano pi-sniffer.service`

* build and deploy the code:

`./build/sh`

* build the optional CGI script

`make cgijson`

* build a version with MQTT (optional)

`make mqtt`

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

# Set the level of MQTT messages to ONE of these:
Environment="VERBOSITY=counts|distances|details"

# Port on which to communicate with sensors in the same group in mesh mode
Environment="UDP_MESH_PORT=7779"

# Port on which to broadcast a count of people present x 10
# If you have multiple sensors in a group, only one should send to the sign, set this to zero for the others
Environment="UDP_SIGN_PORT=7778"

# How to map people to the value sent, e.g. 0.5 so that 4 people = 2.0 sent
# Means that the sign can be configured without deploying new code there
Environment="UDP_SCALE_FACTOR=0.5"

# You can turn logging off entirely by replacing 'all' with specific log domains
# or by removing this line entirely
Environment="G_MESSAGES_DEBUG=all"

# You can connect directly to Influx DB
Environment="INFLUX_SERVER=<influx_server_domain_and_path>"
Environment="INFLUX_PORT=80"
Environment="INFLUX_DATABASE=<database_name>"
Environment="INFLUX_USERNAME=<username>"
Environment="INFLUX_PASSWORD=<password>"

# You can send updates to a webhook passing a JSON object
Environment="WEBHOOK_DOMAIN=<webhook_domain>"
Environment="WEBHOOK_PORT=80"
Environment="WEBHOOK_PATH=<path>"
Environment="WEBHOOK_USERNAME="
Environment="WEBHOOK_PASSWORD="

# You can define patches, areas and other metadata using an optional JSON file, see sample config.json
# which is typically installed to /etc/signswift/config.json but can be placed anywhere
Environment="CONFIG=/etc/signswift/config.json"


````

* Alternatively, instead of using `build.sh` you can run the following steps manually:
* Run 

`make`
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

