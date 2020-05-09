echo "Compiling bluetoothctl"

gcc -I. \
  -I/usr/include/dbus-1.0 \
  -I/usr/lib/arm-linux-gnueabihf/dbus-1.0/include \
  -I/usr/include/glib-2.0 \
  -I/usr/include/bluetooth \
  -I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
  -g -O2 -MT main.o -MD -MP -MF sniffer.Tpo -c main.c advertising.c agent.c display.c gatt.c mqtt.c mqtt_pal.c

echo "Linking bluetoothctl"

gcc -g -O2 -o bluetoothctl main.o display.o agent.o advertising.o gatt.o mqtt.o mqtt_pal.o \
  libgdbus-internal.a libshared-glib.a -lglib-2.0 -lpthread -ldbus-1 -lreadline


echo "Running"

sudo ./bluetoothctl


