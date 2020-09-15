#!/bin/bash

# Script to tail the service log

sudo journalctl -u pi-sniffer.service -S -5min -f

