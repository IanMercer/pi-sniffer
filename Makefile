# pkg-config - brings in dependency gcc parameters
# -lm link with math library

CFLAGS = -Wall -Wextra -g `pkg-config --cflags --libs glib-2.0 gio-2.0` -lm -I.

DEPS = utility.h mqtt.h mqtt_pal.h mqtt_send.h kalman.h bluetooth.h posix_sockets.h
OBJ = scan.o utility.o mqtt.o mqtt_pal.o mqtt_send.o kalman.o posix_sockets.o

# Compile without linking
%.o: %.c $(DEPS)
	gcc -c -o $@ $< $(CFLAGS) 

# Link
scan: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

#
#scan: scan.c utility.c mqtt.c mqtt_pal.c
#	gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -g -o scan scan.c mqtt.c mqtt_pal.c -lm `pkg-config --libs glib-2.0 gio-2.0`
