gcc `pkg-config --cflags glib-2.0 gio-2.0` -Wall -Wextra -o scan scan.c mqtt.c mqtt_pal.c `pkg-config --libs glib-2.0 gio-2.0`

chmod a+x ./scan

sudo ./scan
