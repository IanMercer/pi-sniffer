# FAQ

## Why does the system use Raspberry Pi for the sensors and ESP8266 for the displays?

Raspberry Pis include Bluetooth and whilst the ESP32s does include Bluetooth the capabilities of the BLUEZ stack on the Raspberry Pi are better at this point. We also need plenty of memory to be able to support a large number of mobile phones in a larger space.

ESP8266 are incredibly cheap and can power the displays easily.

## What's the best location for the sensor components in a larger space?

There are two main options: place sensors around the space to be covered towards the edges or place sensors centrally within the space to be covered. Placing them at the corners and on the edges may well be the best option in the future once I get trilateration working reliably but for now locating them centrally for each area to be covered within the space seems to work best. Bluetooth signal strength variability increases with range so the further you are from a sensor the lower your accuracy.

## What are the networking requirements?

The sensing components and the displays need to be on the same network segment with no bridge or router between them. Currently they communicate using UDP broadcasts which are not normally carried across network boundaries. I am considering multicast as an option so please contact me if you need that.

# What kind of Raspberry Pi should I use? Pi W, Pi 3, Pi 4?

Although the software has been built and tested on a variety of Pi W, Pi 3b+ and Pi 4 and although it works just fine on all of them, I'm recommending the Pi 3b+ because it appears to have the best Bluetooth reception of any of these flavors of Raspberry Pi. The Pi 3b+ can see devices almost twice as far away as the Pi W and Pi 4.  I do also run it on a Pi 4 together with an MQTT server to handle the (optional) MQTT traffic.

# What are the display options?

Any sensing component can be configured to send messages to displays. Currently this is a simple UDP message containing an int value that's 10x the number of people x a configurable scale factor. You can change this to literally anything you want: HTTP, MQTT, TCP, UDP, ...  You can also configure it to send either a count of devices nearby, a count of devices nearby that are closest to this sensor, or a total count of all devices present on all sensors in the same group.

## Custom-built 3W LED display

This is the display that was shown in the HCL Hackathon entry. It consists of an ESP8266, a few TIP122 transistors, appropriate base and collector resistors and six 3W LEDs arranged to show two green people far apart, two blue people at a middle distance or two red people close together. This arrangement was designed to help convey 'social distancing' in a way that would be highly visible even to a color-blind person. It's cheap to build and highly visible and you can choose your own layout for the sign. A laser-cut, vinyl-cut or even hand-cut with a scalpel mask helps create distinct pools of light for each icon on the display and a sheet of tracing paper or translucent plastic helps diffuse the LED light.

# LED Matrix Display

The M5 Stack includes a 5x5 RGB LED matrix in one of their versions. This gives you a complete but quite small display in a single $10 unit package. I am testing this arrangement at the moment and will write it up shortly.

There are also separate LED Matrix displays like the very bright Adafruit Neopixel 8x4 display.

# RGB Pixel Displays

Pixel strings based on the WS2811 or WS2812 chip are common and come in many different forms. You can get encased waterproof ones that look like christmas tree lights or flat packages that can be easily stuck to walls or placed in aluminum channels. Some of the longer ones have as many as 150 LEDs on them so you can do quite fancy displays mixing color, position and fade effects.

## Small TFT screen on an ESP8266 or ESP32

There are a variety of TFT screens in the 1 inch to 4" range that you can attach easily to an ESP8266 over SPI. Some ESP32 boards come with a built-in TFT screen. I am evaluating these also and will report back.

## Phillips Hue

You could interface the system to Phillips Hue quite easily (there are open source libraries for this) and control a 110v or 240v light bulb to change color accordingly.

# Lutron Caseta, Zigbee, ZWave, X10 and other systems

These too can be easily interfaced from a Raspberry Pi so you could install two bright 110v lamp bulbs, one red, one green and illuminate them according to how crowded the space is.

# Traffic lights

You can purchase lamps that look like traffic lights from Amazon or eBay. Hack one of these and turn it into a crowding display!

# Servo arms

The PCA9685 chip offers easy servo motor control, or there's a [Raspberry Pi HAT from Adafruit](https://learn.adafruit.com/adafruit-dc-and-stepper-motor-hat-for-raspberry-pi) that offers PWM DC-motor or servo-motor control. Attach a large ARM to a servo and now you can have a purely analog indication as to how busy it is inside.


# Can I run two independent systems on the same network segment?

Yes, simply configure the communication port so that they are isolated into two or more groups.






# My Raspberry Pi has run out of disk space

Take a look at available disk space on `/dev/root` using `df -h`.

Find where the majority of the disk space is being used `sudo du -hsx * | sort -rh`.

Check how much the log is taking `journalctl --disk-usage`

Clean up any excess log files in `/var/log`; remove all the old gzipped log files.

Use `sudo nano /etc/systemd/journald.conf` to change `SystemKeepFree=` to 1GB.

# Error message "error: insufficient permission for adding an object to repository database .git/objects"

If you accidentally run git under `sudo` the `.git` files may move to being owned by the `root` user. Move them all back
to your account using `sudo chown -R <yourname> *` and/or `sudo chown -R <yourname> .git/*`.