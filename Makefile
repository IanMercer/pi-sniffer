# pkg-config - brings in dependency gcc parameters
# -lm link with math library

# See https://github.com/eclipse/paho.mqtt.c for details as to which paho lib to use
CC = gcc -g

CFLAGS = -Wall -Wextra -g `pkg-config --cflags glib-2.0 gio-2.0` -I.

LIBS = -lm -lpaho-mqtt3as `pkg-config --libs glib-2.0 gio-2.0`

DEPS = utility.h mqtt_send.h kalman.h bluetooth.h udp.h
SRC = scan.c utility.c mqtt_send.c kalman.c udp.c

# Set these if you have a PCA8833 connected to I2C with LEDs
ifeq (, $(shell which gpio))
$(echo "No gpio in $(PATH), assuming you don't want any local LED display")
else
CFLAGS := $(CFLAGS) -DINDICATOR="PCA8833"
LIBS := $(LIBS) -lwiringPi
DEPS := $(DEPS) pca9685.h
SRC := $(SRC) pca9685.c
endif

scan: $(SRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

