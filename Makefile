# pkg-config - brings in dependency gcc parameters
# -lm link with math library
prefix=/usr/local


# See https://github.com/eclipse/paho.mqtt.c for details as to which paho lib to use
CC = gcc -g

# for MQTT make scanwithmqtt
# You will need to download and build MQTT Paho for this

CFLAGS = -Wall -Wextra -g `pkg-config --cflags glib-2.0 gio-2.0 gio-unix-2.0` -Isrc -Isrc/model -Isrc/dbus -Isrc/bluetooth -Isrc/core

LIBS = -lm `pkg-config --libs glib-2.0 gio-2.0 gio-unix-2.0` -L./lib -ldbus -lbt -lmodel

DEPS = Makefile src/model/*.h src/core/*.h lib/libdbus.a lib/libbt.a lib/libmodel.a
SRC = src/core/*.c
MQTTSRC = src/mqtt.c src/udp.c src/mqtt_send.c src/influx.c src/core/*.c src/model/*.c src/dbus/*.c
CGIJSON = src/cgijson.c

ARCH := $(shell uname -m)

# To cross-compile to ARM we would also need ARM libraries locally ...
#ARMOPTS = -static -std=c99
#ARMGCC = /opt/pi/tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
#              ~/x-tools/arm-raspbian-linux-gnueabihf/arm-raspbian-linux-gnueabihf/sysroot
#ARMGCC = ~/x-tools/arm-raspbian-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc

# TODO: Compile dbus to a library without warnings for unused parameters

all: scan report cgijson ./lib/libdbus.a ./lib/libbt.a ./lib/libmodel.a

# DBUS
DBUS_SRC := src/dbus
DBUS_OBJ := obj/dbus
DBUS_SOURCES := $(wildcard $(DBUS_SRC)/*.c)
DBUS_OBJECTS := $(patsubst $(DBUS_SRC)/%.c, $(DBUS_OBJ)/%.o, $(DBUS_SOURCES))

$(DBUS_OBJ)/%.o: $(DBUS_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) -I$(DBUS_SRC) $(CFLAGS) $(LIBS) -c $< -o $@

./lib/libdbus.a : $(DBUS_OBJECTS)
	@mkdir -p $(@D)
	ar rcs $@ $(DBUS_OBJECTS)


# BLUETOOTH
BT_SRC := src/bluetooth
BT_OBJ := obj/bluetooth
BT_SOURCES := $(wildcard $(BT_SRC)/*.c)
BT_OBJECTS := $(patsubst $(BT_SRC)/%.c, $(BT_OBJ)/%.o, $(BT_SOURCES))

$(BT_OBJ)/%.o: $(BT_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) -I$(BT_SRC) $(CFLAGS) $(LIBS) -c $< -o $@

./lib/libbt.a : $(BT_OBJECTS)
	@mkdir -p $(@D)
	ar rcs $@ $(BT_OBJECTS)

# MODEL
MODEL_SRC := src/model
MODEL_OBJ := obj/model
MODEL_SOURCES := $(wildcard $(MODEL_SRC)/*.c)
MODEL_OBJECTS := $(patsubst $(MODEL_SRC)/%.c, $(MODEL_OBJ)/%.o, $(MODEL_SOURCES))

$(MODEL_OBJ)/%.o: $(MODEL_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) -I$(MODEL_SRC) $(CFLAGS) $(LIBS) -c $< -o $@

./lib/libmodel.a : $(MODEL_OBJECTS)
	@mkdir -p $(@D)
	ar rcs $@ $(MODEL_OBJECTS)

# MAIN
scan: src/*.c $(SRC) $(DEPS)
	gcc -o scan src/scan.c $(SRC) $(CFLAGS) $(LIBS)
	#tar -czvf sniffer-1.0.tar.gz ./src

#report: src/*.c $(SRC) $(DEPS)
#	gcc -o report src/report.c $(SRC) $(CFLAGS) $(LIBS)

cgijson: $(CGIJSON)
	gcc -o cgijson.cgi $(CGIJSON) $(CFLAGS) $(LIBS)
	echo assuming you have apache set up on your Raspberry Pi
	echo cp cgijson.cgi /usr/lib/cgi-bin/

armversion: $(SRC) $(DEPS)
	$(ARMGCC) $(ARMOPTS) -o scan_pi src/scan.c $(SRC) $(CFLAGS) $(LIBS)

scanwithmqtt: $(SRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS) -lpaho-mqtt3as -DMQTT

mqtt: $(MQTTSRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS) -lpaho-mqtt3as -DMQTT


install: scan
	#cp pi-sniffer.service ./snifferpkg/DEBIAN/conffiles/etc/systemd/system/pi-sniffer.service
	mkdir -p ./snifferpkg/usr/lib/systemd/system
	cp pi-sniffer.service ./snifferpkg/usr/lib/systemd/system/pi-sniffer.service
	dpkg -b ./snifferpkg ./snifferpkg_1.0.0-0_$(ARCH).deb
	# Build a package for it ^
	install -D ./scan $(DESTDIR)$(prefix)/bin/scan

clean:
	echo Not handled yet (clean)

distclean: clean

uninstall:
	-rm -f $(DESTDIR)$(prefix)/bin/scan

.PHONY: all install clean distclean uninstall

# run codegen.sh instead, this isn't needed most of the time, only when the xml definition is updated
#sniffer-generated.h sniffer-generated.c: sniffer.xml
#	gdbus-codegen --generate-c-code sniffer-generated --c-namespace pi --interface-prefix com.signswift sniffer.xml

#cgijson: $(CGIJSON)
#	gcc -o $@ $^ $(CFLAGS) $(LIBS)
#	echo assuming you have apache set up on your Raspberry Pi
#	cp cgijson /usr/lib/cgi-bin/
