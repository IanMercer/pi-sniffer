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

`sudo apt-get install libjson-glib-dev`

* optionally install and build MQTT

See [MQTT.md](MQTT.md). You do not need to do this if you are not using MQTT.

* clone this repository

````
git clone https://github.com/ianmercer/pi-sniffer`
cd pi-sniffer
````

* create a configuration file

Add the details of any iBeacons you have or other Bluetooth devices you want to track

````
sudo mkdir /etc/signswift
sudo cp config.json /etc/signswift/config.json
nano /etc/signswift/config.json
````

* edit the .service file to point to the scan executable location by editing the `ExecStart` and `WorkingDirectory` lines:
````

nano pi-sniffer.service
````

* recommend setting environment variable G_MESSAGES_DEBUG=all for initial run to see verbose logging including discovered devices

`export G_MESSAGES_DEBUG=all`

* build and deploy the code:

`./build.sh`

Watch the logs for a while and you should see it start to discover devices. Every minute it will dump a table to the logs showing all the devices present.
To report any issues, please include as much of the log as possible, e.g. to grab the last hour of logs and place it in a file:

`sudo journalctl -u pi-sniffer.service -S -1h > log.txt`

To transfer files back and forth from PC to Raspberry Pi try using `WinSCP` which is a free download for Windows.



* build the optional CGI script

`make cgijson`


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
Environment="HOST_NAME=<room name>"
Environment="HOST_DESCRIPTION=<description>"
Environment="HOST_PLATFORM=Pi3b+"

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

# In order to use Influx DB or webhook the DBUS needs to be enabled.
Environment="DBUS_SENDER=1"

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

# You can define other sensors in the mesh and named beacons in a config file. Copy the sample one to `/etc/signswift/config.json`
# Optionally you can point to a different location using this setting:
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

* [optional] For Ubuntu and some other Linx distributions open a firewall port so multiple instances can communicate

`sudo ufw allow 7779/udp`

* Now setup one or more displays. There are several display options depending on your needs.
 
 * A display based on the $20 M5Stack Matrix requiring no hardware skills [this repo](https://github.com/IanMercer/CrowdAlertM5StackMatrix).
 * A display using high-brightness 3W LEDs requiring mechanical and electrical fabrication skills [instructions coming soon](GettingStarted.md).
 * A display using small TFT screens on ESP32 boards, requiring a case but no soldering [instructions coming soon](GettingStarted.md).
 * A display using another Raspberry Pi or other computer [instructions coming soon](GettingStarted.md).
 * A browser-based display [instructions coming soon](GettingStarted.md).

A comparison of the costs and benefits of these different display approaches will be added shortly. Some displays may display details of crowding in different areas, some may display only an aggregate, some may show detailed numbers, some may display based on a single aggregate cut off number. Displays may also apply blending, hysteresis, transitions and other effects to improve their overall impact.


# Training

If you have more than one sensor in the system you can train it to recognize areas in different rooms. See [training](training.md).


# Web Page

The software includes a web site where you can view the status of the system, monitor crowded rooms and track assets. See [apache](apache.md) for details on
setting this up. This setup also includes an API that you can call to get the status of the system from any other system. The API is exposed at `/cgi-bin/cgijson.cgi`
within the web site.

# Webhook

The software includes the capability send the current status whenever it changes over HTPP. Configure this using `sudo systemctl edit pi-sniffer.service` and add
your webhook details:

````
Environment="WEBHOOK_DOMAIN=192.168.0.202"
Environment="WEBHOOK_PORT=8087"
Environment="WEBHOOK_PATH=/api/bluetooth/update"
Environment="WEBHOOK_USERNAME="
Environment="WEBHOOK_PASSWORD="
````

# InfluxDB and Grafana

The software can also send detailed statistics to InfluxDB and Grafana. InfluxDB is a time series database. The database makes it easy to document and analyze time series events. Grafana is a tool for dashboard creation using drag and drop data visualization techniques. Grafana has a direct plugin for InfluxDB requireing little configuration to visualize the device data. 

## Install InfluxDB and Grafana

To install a local influx DB instance on the raspberry pi running raspbian:

````
wget -qO- https://repos.influxdata.com/influxdb.key | sudo apt-key add -
echo "deb https://repos.influxdata.com/debian $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/influxdb.list
sudo apt update
sudo apt install influxdb
````
With influxDB installed you can now enable the service to start at boot and start the service:
````
sudo systemctl unmask influxdb
sudo systemctl enable influxdb
sudo systemctl start influxdb
````
Now with influxDB running we will have to create a database where we can log our data to. This can be done by going into the influxDB console by running `influx` in the shell. Then executing the create database query `CREATE DATABASE <database>` and exitign with `exit` afterwards.

Your influxDB is now setup and ready to use. Check out the influxDB documentation for further setup such as authentication. 

In order to install Grafana check out the following comprehensive [guide](https://grafana.com/tutorials/install-grafana-on-raspberry-pi/). 

## Setting up Pi-Sniffer for influxDB and Grafana

First in order to use any features connected over the DBUS we have to set the environment variable `DBUS_SENDER=1`. After that we can add all the influxDB variables. The default install port ist `8086` with the servername pointing to `localhost` and your newly created database. If you have not set up any authentication you can leave the username and password empty.
````
Environment="DBUS_SENDER=1"
Environment="INFLUX_SERVER=<domain>"
Environment="INFLUX_PORT=80"
Environment="INFLUX_DATABASE=<database>"
Environment="INFLUX_USERNAME=<username>"
Environment="INFLUX_PASSWORD=<password>"
````

Pi-sniffer does not immidiatly send statistics to influxDB. Therefore you have to wait a couple of minutes for the data to show up in the DB. 

You can also add a visualization to your Grafana dashboard by using the query `SELECT * from <HOST_NAME>` which will automatically create a time series diagram for each device type.

# UDP Send

The software sends a high-level summary (groups) as a small JSON message over UDP along with a scale factor (metadata) that can be used to adjust the sign display
without having to update the sign itself. e.g.
````
{"Unknown":0.0,"Barn":0.0,"Outside":4.6,"House":2.8,"sf":3}
````
This is designed to be easy to parse using ArduinoJSON or equivalent on a microcontroller.


# Creating a disk image for remote deployment

Follow the steps to set up a complete system and test it.

Next, remove the contents of the `recordings` and `beacons` subdirectories.

To avoid the other person from having to set their wireless settings country before WiFi works you can disable the service. Only do this if you
have includes the Wireless settings including country code in wpa_supplicant.conf.
````
sudo systemctl mask systemd-rfkill.service
````

Edit the wpa_supplicant.conf file to prepare it for the remote destination network:

````
sudo nano /etc/wpa_supplicant/wpa_supplicant.conf
````
Alternatively you can create one from Windows or Mac by editing a file with the same name in `/boot/wpa_supplicant.conf`. On startup
the Raspberry Pi will copy this file to the correct location.


Do a clean shutdown `sudo shutdown`.  Wait until it's finished and then remove the card from the Raspberry Pi.

Put the card into a Mac or Windows machine and create the `Wpa_supplicant.conf` file for the destination network.

Use `ApplePi Baker` to create a .IMG file. Share that `.IMG` file with the recipient and they can use `Etcher` to burn the image to a clean SD card.


They should 




# Hotspot Mode

hostapd (host access point daemon) is a user space daemon software enabling a network interface card to act as an access point and authentication server. 

````
sudo apt-get install hostapd
sudo apt-get install dnsmasq
sudo systemctl unmask hostapd

cp /usr/share/doc/hostapd/examples/hostapd.conf > /etc/hostapd/hostapd.conf
  # $EDITOR /etc/hostapd/hostapd.conf




sudo nano /etc/hostapd/hostapd.conf
````


````
#2.4GHz setup wifi 80211 b,g,n
interface=wlan0
driver=nl80211
ssid=CrowdAlert
hw_mode=g
channel=8
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=NoCrowdingHere
wpa_key_mgmt=WPA-PSK
wpa_pairwise=CCMP TKIP
rsn_pairwise=CCMP

#80211n - Change GB to your WiFi country code
country_code=US
ieee80211n=1
ieee80211d=1
````


See also `cat /usr/share/doc/hostapd/README.Debian`

Now set up dnsmasq:

````
sudo nano /etc/dnsmasq.conf
````

Add the following lines:

````
#RPiHotspot config - Internet
interface=wlan0
bind-dynamic
domain-needed
bogus-priv
dhcp-range=192.168.50.150,192.168.50.200,255.255.255.0,1h
````


Now `sudo nano /etc/network/interfaces` to check that it's unchanged from:
````
# interfaces(5) file used by ifup(8) and ifdown(8)
# Please note that this file is written to be used with dhcpcd
# For static IP, consult /etc/dhcpcd.conf and 'man dhcpcd.conf'
# Include files from /etc/network/interfaces.d:
source-directory /etc/network/interfaces.d
````








````
sudo nano /etc/sysctl.conf
````

Uncomment the next line to enable packet forwarding for IPv4
````
#net.ipv4.ip_forward=1
````


````
sudo nano /etc/dhcpcd.conf
````

Add a line at the bottom
````
nohook wpa_supplicant
````

or for permanent forwarding they suggested

````
nohook wpa_supplicant
interface wlan0
static ip_address=192.168.50.10/24
static routers=192.168.50.1
static domain_name_servers=8.8.8.8
````

First create the file for the ip table rules.

sudo nano /etc/iptables-hs

add the lines below or download from here

#!/bin/bash
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
iptables -A FORWARD -i eth0 -o wlan0 -m state --state RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -i wlan0 -o eth0 -j ACCEPT
now save (ctrl & o) and exit (ctrl & x)

Update the permissions so it can be run with

sudo chmod +x /etc/iptables-hs

Now the service file can be created which will activate the ip tables each time the Raspberry Pi starts up

Create the following file

sudo nano /etc/systemd/system/hs-iptables.service

Then add the lines below of download from here


[Unit]
Description=Activate IPtables for Hotspot
After=network-pre.target
Before=network-online.target

[Service]
Type=simple
ExecStart=/etc/iptables-hs

[Install]
WantedBy=multi-user.target






















autohotspot service file
Next we have to create a service which will run the autohotspot script when the Raspberry Pi starts up.

create a new file with the command

sudo nano /etc/systemd/system/autohotspot.service

Then enter the following text or download here


[Unit]
Description=Automatically generates an internet Hotspot when a valid ssid is not in range
After=multi-user.target
[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/bin/autohotspotN
[Install]
WantedBy=multi-user.target
and save (ctrl & o) and exit (ctrl & x)

 

For the service to work it has to be enabled. To do this enter the command

sudo systemctl enable autohotspot.service



#!/bin/bash
#version 0.961-N/HS-I

#You may share this script on the condition a reference to RaspberryConnect.com 
#must be included in copies or derivatives of this script. 

#Network Wifi & Hotspot with Internet
#A script to switch between a wifi network and an Internet routed Hotspot
#A Raspberry Pi with a network port required for Internet in hotspot mode.
#Works at startup or with a seperate timer or manually without a reboot
#Other setup required find out more at
#http://www.raspberryconnect.com

wifidev="wlan0" #device name to use. Default is wlan0.
ethdev="eth0" #Ethernet port to use with IP tables
#use the command: iw dev ,to see wifi interface name 

IFSdef=$IFS
cnt=0
#These four lines capture the wifi networks the RPi is setup to use
wpassid=$(awk '/ssid="/{ print $0 }' /etc/wpa_supplicant/wpa_supplicant.conf | awk -F'ssid=' '{ print $2 }' | sed 's/\r//g'| awk 'BEGIN{ORS=","} {print}' | sed 's/\"/''/g' | sed 's/,$//')
IFS=","
ssids=($wpassid)
IFS=$IFSdef #reset back to defaults


#Note:If you only want to check for certain SSIDs
#Remove the # in in front of ssids=('mySSID1'.... below and put a # infront of all four lines above
# separated by a space, eg ('mySSID1' 'mySSID2')
#ssids=('mySSID1' 'mySSID2' 'mySSID3')

#Enter the Routers Mac Addresses for hidden SSIDs, seperated by spaces ie 
#( '11:22:33:44:55:66' 'aa:bb:cc:dd:ee:ff' ) 
mac=()

ssidsmac=("${ssids[@]}" "${mac[@]}") #combines ssid and MAC for checking

createAdHocNetwork()
{
    echo "Creating Hotspot"
    ip link set dev "$wifidev" down
    ip a add 192.168.50.5/24 brd + dev "$wifidev"
    ip link set dev "$wifidev" up
    dhcpcd -k "$wifidev" >/dev/null 2>&1
    iptables -t nat -A POSTROUTING -o "$ethdev" -j MASQUERADE
    iptables -A FORWARD -i "$ethdev" -o "$wifidev" -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -i "$wifidev" -o "$ethdev" -j ACCEPT
    systemctl start dnsmasq
    systemctl start hostapd
    echo 1 > /proc/sys/net/ipv4/ip_forward
}

KillHotspot()
{
    echo "Shutting Down Hotspot"
    ip link set dev "$wifidev" down
    systemctl stop hostapd
    systemctl stop dnsmasq
    iptables -D FORWARD -i "$ethdev" -o "$wifidev" -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -D FORWARD -i "$wifidev" -o "$ethdev" -j ACCEPT
    echo 0 > /proc/sys/net/ipv4/ip_forward
    ip addr flush dev "$wifidev"
    ip link set dev "$wifidev" up
    dhcpcd  -n "$wifidev" >/dev/null 2>&1
}

ChkWifiUp()
{
	echo "Checking WiFi connection ok"
        sleep 20 #give time for connection to be completed to router
	if ! wpa_cli -i "$wifidev" status | grep 'ip_address' >/dev/null 2>&1
        then #Failed to connect to wifi (check your wifi settings, password etc)
	       echo 'Wifi failed to connect, falling back to Hotspot.'
               wpa_cli terminate "$wifidev" >/dev/null 2>&1
	       createAdHocNetwork
	fi
}

chksys()
{
    #After some system updates hostapd gets masked using Raspbian Buster, and above. This checks and fixes  
    #the issue and also checks dnsmasq is ok so the hotspot can be generated.
    #Check Hostapd is unmasked and disabled
    if systemctl -all list-unit-files hostapd.service | grep "hostapd.service masked" >/dev/null 2>&1 ;then
	systemctl unmask hostapd.service >/dev/null 2>&1
    fi
    if systemctl -all list-unit-files hostapd.service | grep "hostapd.service enabled" >/dev/null 2>&1 ;then
	systemctl disable hostapd.service >/dev/null 2>&1
	systemctl stop hostapd >/dev/null 2>&1
    fi
    #Check dnsmasq is disabled
    if systemctl -all list-unit-files dnsmasq.service | grep "dnsmasq.service masked" >/dev/null 2>&1 ;then
	systemctl unmask dnsmasq >/dev/null 2>&1
    fi
    if systemctl -all list-unit-files dnsmasq.service | grep "dnsmasq.service enabled" >/dev/null 2>&1 ;then
	systemctl disable dnsmasq >/dev/null 2>&1
	systemctl stop dnsmasq >/dev/null 2>&1
    fi
}


FindSSID()
{
#Check to see what SSID's and MAC addresses are in range
ssidChk=('NoSSid')
i=0; j=0
until [ $i -eq 1 ] #wait for wifi if busy, usb wifi is slower.
do
        ssidreply=$((iw dev "$wifidev" scan ap-force | egrep "^BSS|SSID:") 2>&1) >/dev/null 2>&1 
        #echo "SSid's in range: " $ssidreply
	printf '%s\n' "${ssidreply[@]}"
        echo "Device Available Check try " $j
        if (($j >= 10)); then #if busy 10 times goto hotspot
                 echo "Device busy or unavailable 10 times, going to Hotspot"
                 ssidreply=""
                 i=1
	elif echo "$ssidreply" | grep "No such device (-19)" >/dev/null 2>&1; then
                echo "No Device Reported, try " $j
		NoDevice
        elif echo "$ssidreply" | grep "Network is down (-100)" >/dev/null 2>&1 ; then
                echo "Network Not available, trying again" $j
                j=$((j + 1))
                sleep 2
	elif echo "$ssidreply" | grep "Read-only file system (-30)" >/dev/null 2>&1 ; then
		echo "Temporary Read only file system, trying again"
		j=$((j + 1))
		sleep 2
	elif echo "$ssidreply" | grep "Invalid exchange (-52)" >/dev/null 2>&1 ; then
		echo "Temporary unavailable, trying again"
		j=$((j + 1))
		sleep 2
	elif echo "$ssidreply" | grep -v "resource busy (-16)"  >/dev/null 2>&1 ; then
               echo "Device Available, checking SSid Results"
		i=1
	else #see if device not busy in 2 seconds
                echo "Device unavailable checking again, try " $j
		j=$((j + 1))
		sleep 2
	fi
done

for ssid in "${ssidsmac[@]}"
do
     if (echo "$ssidreply" | grep -F -- "$ssid") >/dev/null 2>&1
     then
	      #Valid SSid found, passing to script
              echo "Valid SSID Detected, assesing Wifi status"
              ssidChk=$ssid
              return 0
      else
	      #No Network found, NoSSid issued"
              echo "No SSid found, assessing WiFi status"
              ssidChk='NoSSid'
     fi
done
}

NoDevice()
{
	#if no wifi device,ie usb wifi removed, activate wifi so when it is
	#reconnected wifi to a router will be available
	echo "No wifi device connected"
	wpa_supplicant -B -i "$wifidev" -c /etc/wpa_supplicant/wpa_supplicant.conf >/dev/null 2>&1
	exit 1
}

chksys
FindSSID

#Create Hotspot or connect to valid wifi networks
if [ "$ssidChk" != "NoSSid" ]
then
       echo 0 > /proc/sys/net/ipv4/ip_forward #deactivate ip forwarding
       if systemctl status hostapd | grep "(running)" >/dev/null 2>&1
       then #hotspot running and ssid in range
              KillHotspot
              echo "Hotspot Deactivated, Bringing Wifi Up"
              wpa_supplicant -B -i "$wifidev" -c /etc/wpa_supplicant/wpa_supplicant.conf >/dev/null 2>&1
              ChkWifiUp
       elif { wpa_cli -i "$wifidev" status | grep 'ip_address'; } >/dev/null 2>&1
       then #Already connected
              echo "Wifi already connected to a network"
       else #ssid exists and no hotspot running connect to wifi network
              echo "Connecting to the WiFi Network"
              wpa_supplicant -B -i "$wifidev" -c /etc/wpa_supplicant/wpa_supplicant.conf >/dev/null 2>&1
              ChkWifiUp
       fi
else #ssid or MAC address not in range
       if systemctl status hostapd | grep "(running)" >/dev/null 2>&1
       then
              echo "Hostspot already active"
       elif { wpa_cli status | grep "$wifidev"; } >/dev/null 2>&1
       then
              echo "Cleaning wifi files and Activating Hotspot"
              wpa_cli terminate >/dev/null 2>&1
              ip addr flush "$wifidev"
              ip link set dev "$wifidev" down
              rm -r /var/run/wpa_supplicant >/dev/null 2>&1
              createAdHocNetwork
       else #"No SSID, activating Hotspot"
              createAdHocNetwork
       fi
fi

sudo chmod +x /usr/bin/autohotspotN





