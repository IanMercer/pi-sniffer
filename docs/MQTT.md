# MQTT

Optional support for MQTT is included. To enable it build the MQTT version after following these steps:


* clone the Eclipse PAHO MQTT C source

`git clone https://github.com/eclipse/paho.mqtt.c.git`

* build and install it

`cd paho.mqtt.c`

`sudo make install`

`cd ..`


* build a version with MQTT

`make mqtt`

* configure the MQTT settings

`sudo systemctl edit pi-sniffer.service`

# Server is formatted: [ssl://]mqtt server:[port]
# For Azure you MUST use ssl:// and :8833

Environment="MQTT_SERVER=192.168.0.52:1883"
Environment="MQTT_TOPIC=BLF"
Environment="MQTT_USERNAME="
Environment="MQTT_PASSWORD="

# Set the level of MQTT messages to ONE of these:
Environment="VERBOSITY=counts|distances|details"

