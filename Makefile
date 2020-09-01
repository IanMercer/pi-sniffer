# pkg-config - brings in dependency gcc parameters
# -lm link with math library

# See https://github.com/eclipse/paho.mqtt.c for details as to which paho lib to use
CC = gcc -g

CFLAGS = -Wall -Wextra -g `pkg-config --cflags glib-2.0 gio-2.0` -I.

LIBS = -lm -lpaho-mqtt3as `pkg-config --libs glib-2.0 gio-2.0`

DEPS = utility.h mqtt_send.h kalman.h bluetooth.h udp.h cJSON.h
SRC = scan.c utility.c bluetooth.c mqtt_send.c kalman.c udp.c cJSON.c device.c

scan: $(SRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

