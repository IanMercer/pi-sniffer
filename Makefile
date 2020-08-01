# pkg-config - brings in dependency gcc parameters
# -lm link with math library

# See https://github.com/eclipse/paho.mqtt.c for details as to which paho lib to use

CFLAGS = -Wall -Wextra -g `pkg-config --cflags --libs glib-2.0 gio-2.0` -lm -I. -lpaho-mqtt3as

DEPS = utility.h mqtt_send.h kalman.h bluetooth.h certs.h
OBJ = scan.o utility.o mqtt_send.o kalman.o certs.o

# Compile without linking
%.o: %.c $(DEPS)
	gcc -c -o $@ $< $(CFLAGS) 

# Link
scan: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

#
#scan: scan.c utility.c mqtt.c mqtt_pal.c
#	gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -g -o scan scan.c mqtt.c mqtt_pal.c -lm `pkg-config --libs glib-2.0 gio-2.0`
