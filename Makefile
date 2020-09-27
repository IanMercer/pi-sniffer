# pkg-config - brings in dependency gcc parameters
# -lm link with math library

# See https://github.com/eclipse/paho.mqtt.c for details as to which paho lib to use
CC = gcc -g

CFLAGS = -Wall -Wextra -g `pkg-config --cflags glib-2.0 gio-2.0 gio-unix-2.0` -I.

LIBS = -lm -lpaho-mqtt3as `pkg-config --libs glib-2.0 gio-2.0 gio-unix-2.0`

DEPS = utility.h mqtt_send.h kalman.h bluetooth.h udp.h cJSON.h sniffer-generated.h heuristics.h influx.h rooms.h accesspoints.h
SRC = scan.c utility.c bluetooth.c mqtt_send.c kalman.c udp.c cJSON.c device.c heuristic-apple.c heuristic-manufacturers.c heuristic-names.c heuristic-uuids.c influx.c rooms.c accesspoints.c
MQTTSRC = mqtt.c udp.c utility.c device.c cJSON.c mqtt_send.c sniffer-generated.c influx.c

scan: $(SRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

mqtt: $(MQTTSRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

sniffer-generated.h sniffer-generated.c: sniffer.xml
	gdbus-codegen --generate-c-code sniffer-generated --c-namespace pi --interface-prefix com.signswift sniffer.xml

