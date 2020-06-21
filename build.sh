#!/bin/bash

# Stop on any error
set -e

gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -o scan scan.c mqtt.c mqtt_pal.c -lm `pkg-config --libs glib-2.0 gio-2.0`

chmod a+x ./scan

# run immediately
# sudo ./scan 192.168.0.120 1883

# update running service
sudo systemctl stop pi-sniffer.service
sudo systemctl start pi-sniffer.service

sudo journalctl -u pi-sniffer.service -f
