#!/bin/bash

# Stop on any error
set -e

git pull

echo building service
gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -g -o scan scan.c mqtt.c mqtt_pal.c -lm `pkg-config --libs glib-2.0 gio-2.0`

echo if you want to test the service, stop here and run under valgrind (passing your MQTT server address)
echo     valgrind --tool=memcheck --leak-check=full --track-origins=yes ./scan 192.168.0.52 1883

chmod a+x ./scan

# run immediately
# sudo ./scan 192.168.0.120 1883

# update running service

if [ -e /lib/systemd/system/pi-sniffer.service ]
then
echo stopping service
sudo systemctl stop pi-sniffer.service
fi

echo installing service
sudo cp pi-sniffer.service /etc/systemd/system/
sudo chmod 644 /etc/systemd/system/pi-sniffer.service
sudo systemctl enable pi-sniffer.service
sudo systemctl daemon-reload
sudo systemctl enable pi-sniffer.service

echo starting service
sudo systemctl start pi-sniffer.service

echo now tailing log, ctrl-c to stop but leave service running
sudo journalctl -u pi-sniffer.service -f
