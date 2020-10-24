#!/bin/bash

# Stop on any error
set -e

git pull

echo building service
make

echo "if you want to test the service, stop here and run under valgrind"
echo "    G_DEBUG=gc-friendly G_SLICE=always-malloc valgrind --tool=memcheck --suppressions=/usr/share/glib-2.0/valgrind/glib.supp --leak-check=full --track-origins=yes ./scan"

chmod a+x ./scan

# Create executable directory
if [ ! -e /opt/sniffer ]
then
sudo mkdir /opt/sniffer
fi

# Create working directory
if [ ! -e /var/sniffer ]
then
sudo mkdir /var/sniffer
fi

# copy executable over to opt directory
echo Copy scan to /opt/sniffer
sudo cp scan /opt/sniffer/

# run immediately
# sudo ./scan 192.168.0.52:1883

# update running service

if [ -e /etc/systemd/system/pi-sniffer.service ]
then
echo stopping service
sudo systemctl stop pi-sniffer.service
fi

echo installing service
# DBUS configuration necessary to allow name to be registered on bus
sudo cp com.signswift.sniffer.conf /etc/dbus-1/system.d/
sudo cp pi-sniffer.service /etc/systemd/system/
sudo chmod 644 /etc/systemd/system/pi-sniffer.service
sudo systemctl enable pi-sniffer.service
sudo systemctl daemon-reload
sudo systemctl enable pi-sniffer.service

echo starting service
sudo systemctl start pi-sniffer.service

echo now tailing log, ctrl-c to stop but leave service running
echo log level use -p 6 for info level, -p 7 for debug level or remove it
# Start two minutes back to see the transition and tail forward
sudo journalctl -u pi-sniffer.service -S -2min -f -p 7
