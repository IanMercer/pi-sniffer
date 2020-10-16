# pkg-config - brings in dependency gcc parameters
# -lm link with math library

# See https://github.com/eclipse/paho.mqtt.c for details as to which paho lib to use
CC = gcc -g

# for MQTT add -DMQTT to CFLAGS
# You will need to download and build MQTT Paho for this

CFLAGS = -Wall -Wextra -g `pkg-config --cflags glib-2.0 gio-2.0 gio-unix-2.0` -Isrc -Isrc/model -Isrc/dbus -Isrc/bluetooth -Isrc/core

LIBS = -lm -lpaho-mqtt3as `pkg-config --libs glib-2.0 gio-2.0 gio-unix-2.0`

DEPS = src/*.h src/model/*.h src/core/*.h src/bluetooth/*.h src/dbus/*.h
SRC = src/scan.c src/mqtt_send.c src/udp.c src/influx.c src/knn.c src/webhook.c src/core/*.c src/model/*.c src/bluetooth/*.c src/dbus/*.c
MQTTSRC = src/mqtt.c src/udp.c src/mqtt_send.c src/influx.c src/core/*.c src/model/*.c src/dbus/*.c
CGIJSON = src/cgijson.c src/dbus/sniffer-generated.c

scan: $(SRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

mqtt: $(MQTTSRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

# run codegen.sh instead, this isn't needed most of the time, only when the xml definition is updated
#sniffer-generated.h sniffer-generated.c: sniffer.xml
#	gdbus-codegen --generate-c-code sniffer-generated --c-namespace pi --interface-prefix com.signswift sniffer.xml

cgijson: $(CGIJSON)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)
