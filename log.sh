#!/bin/bash

# Script to tail the service log

sudo journalctl -u pi-sniffer.service -f

