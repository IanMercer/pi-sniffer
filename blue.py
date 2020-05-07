import sys
import os
import struct
from ctypes import (CDLL, get_errno)
from ctypes.util import find_library
from socket import (
    socket,
    AF_BLUETOOTH,
    SOCK_RAW,
    BTPROTO_HCI,
    SOL_HCI,
    HCI_FILTER,
)

if not os.geteuid() == 0:
    sys.exit("script only works as root")

btlib = find_library("bluetooth")
if not btlib:
    raise Exception(
        "Can't find required bluetooth libraries"
        " (need to install bluez)"
    )
bluez = CDLL(btlib, use_errno=True)

dev_id = bluez.hci_get_route(None)

sock = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)
sock.bind((dev_id,))

err = bluez.hci_le_set_scan_enable(
    sock.fileno(),
    0,  # 1 - turn on;  0 - turn off
    0, # 0-filtering disabled, 1-filter out duplicates
    1000  # timeout
)
#if err < 0:
#    raise Exception("Could not stop previous scan")

err = bluez.hci_le_set_scan_parameters(sock.fileno(), 1, 0x10, 0x10, 0, 0, 1000);
if err < 0:
    raise Exception("Set scan parameters failed")
    # occurs when scanning is still enabled from previous call

# allows LE advertising events
hci_filter = struct.pack(
    "<IQH", 
    0x00000010, 
    0x4000000000000000, 
    0
)
sock.setsockopt(SOL_HCI, HCI_FILTER, hci_filter)

err = bluez.hci_le_set_scan_enable(
    sock.fileno(),
    1,  # 1 - turn on;  0 - turn off
    0, # 0-filtering disabled, 1-filter out duplicates
    1000  # timeout
)
if err < 0:
    errnum = get_errno()
    raise Exception("{} {}".format(
        errno.errorcode[errnum],
        os.strerror(errnum)
    ))

cache = {}
count = 0

while True:
    data = sock.recv(1024)
    #if ord(data[14]) == 2:
    #  print(':'.join("{0:02x}".format(ord(x)) for x in data[12:6:-1]), "{0:02x}".format(ord(data[13])),"{0:02x}".format(ord(data[14])),':'.join("{0:02x}".format(ord(x)) for x in data[15:]))
    #  continue
    #else:
      # print bluetooth address from LE Advert. packet
    #print('')
    #print(':'.join("{0:02x}".format(ord(x)) for x in data))

    macAddress = ':'.join("{0:02x}".format(ord(x)) for x in data[12:6:-1])

    if macAddress in cache:
       device = cache.get(macAddress)
    else:
       print ('NEW DEVICE')
       device = {}
       cache[macAddress] =  device
       device["name"] = macAddress
       device["count"] = 0

    device["count"] = device["count"] + 1;
    #print(device)

    packets = ''
    pointer = 14
    while pointer < len(data)-1 :
        size = ord(data[pointer])
        type = ord(data[pointer+1])

        if type == 8:
            packet = data[pointer+2:pointer+1+size].decode("utf-8")
            device["name"] = data[pointer+2:pointer+1+size].decode("utf-8")
        elif type == 9:
            packet = data[pointer+2:pointer+1+size].decode("utf-8")
            device["name"] = data[pointer+2:pointer+1+size].decode("utf-8")
        else:
          packet = "{0:02}".format(size) + ' ' + (':'.join("{0:02x}".format(ord(x)) for x in data[pointer+1:pointer+1+size]))

        packets = packets + '{' + packet + '} '

        pointer = pointer + 1 + size

    #print(macAddress + ' > ' + packets)
    count = (count + 1) % 100;
    if count == 0:
        print('')
        for item in cache:
            print(cache[item])
