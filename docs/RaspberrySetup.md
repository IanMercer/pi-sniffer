# Raspberry Pi Headless Setup

The [Raspberry Pi 4 2GB](https://amzn.to/2HlLWjy) or the [Raspberry Pi 3 Model A+ or B+](https://amzn.to/2H2wYQ3) are the recommended Raspberry Pi
versions. The 3+ has slightly better Bluetooth range but the 4 is faster and more 'future-proof'. You'll also need a 5V USB power supply and micro-USB cable and an SD card. I recommend the [Sandisk Industrial 8GB UHS-1 micro-SD cards](https://amzn.to/3lZa7U5).

1. Download the *Raspberry Pi OS (32-bit) Lite* image from https://www.raspberrypi.org/downloads/raspberry-pi-os/

2. Burn the image to a micro-SD card using Etcher: https://www.balena.io/etcher/

3. Create a `wpa_supplicant.conf` following the instructions here: https://www.raspberrypi.org/documentation/configuration/wireless/headless.md
You will need to set the correct country code, SSID and password for your network.

To encrypt the password (as you should) use this command:

````
wpa_passphrase "ssid" "password"
````

You can add multiple Wi-Fi connections provided each has a `priority=` value.


4. Add a blank file called `ssh` with no extension in the root directory. This will allow remote access over SSH.

5. Plug the Pi in and find what IP address it was allocated. You can probably find it at `raspberrypi.ocal` or you can connect a monitor and keyboard, or you can look at your router to see what IP was allocated, or you can do a portscan on the local network to find it (change the base address to match your local network in this nmap command.)
````
sudo nmap -sn 192.168.0.0/24
````

6. Connect to the Pi over SSH. My favorite tool for this is Windows Terminal. In Windows Terminal settings add the new Pi with a configuration like this and create a new color scheme 'Blue' so you can easily tell when you are connected to the Pi:

````
{
    "guid": "{3f9294f1-2403-5e85-acce-98e5da3839be}",
    "hidden": false,
    "name": "Rasbpi3 LOCATION",
    "colorScheme" : "Blue",
    "icon" : "ms-appx:///ProfileIcons/{9acb9455-ca41-5af7-950f-6bca1bc9722f}.png",
    "commandline" : "bash.exe -c \"ssh pi@192.168.0.178\""
},
````

7. The first time you connect use `pi` as the user and `raspberry` as the password.

9. Run `raspi-config` and perform the following steps:

    a. Change the default password for the pi user
    a. resize the partition to fill the SD card.
    b. change the host name
    c. set the timezone

Reboot after this.

10. Some of the above steps can be carried out at the command line instead, e.g.

    a. Change the default password for the pi user using `passwd`.

    b. Change the hostname and hosts by replacing `raspberrypi` with your chosen name in both of these locations, and reboot:

````
sudo nano \etc\hostname
sudo nano \hosts
sudo reboot
````

    c. Set you local time zone and check it
````
sudo timedatectl set-timezone America/Los_Angeles
timedatectl status
````


11. Reconnect and add a new root user (and delete the pi user later).

````
sudo adduser <username>

# view all the groups the 'pi' user is in:
sudo groups pi

# add the user to all the linux groups you need, here's a full set like the 'pi' user:
sudo usermod -aG sudo,adm,dialout,cdrom,sudo,audio,video,plugdev,games,users,input,netdev,spi,i2c,gpio <username>
````

12. Optionally add password-less SSH access from your controlling computer

See https://lintut.com/how-to-setup-ssh-passwordless-login-on-centos-7-rhel-7-rhel-8/
````
ssh-copy-id -i ~/.ssh/id_rsa.pub ian@192.168.0.178
````

13. Log back in as the new root user after updating your Windows Terminal settings.

14. Update packages to latest version
````
sudo apt-get update
sudo apt-get upgrade
````

16. Optionally allow `journalctl` to persist logs between boots:
````
sudo nano /etc/systemd/journald.conf
````

Under the `[Journal]` section, set the Storage= option to "persistent" to enable persistent logging:

````
[Journal]
Storage=persistent
````

This allows you to `journalctl --list-boots` and to display the journal for a specific boot, e.g. `-b -1`.

17. Optionally add a RTC chip (only really useful if you will use the Pi in a disconnected, mobile state)

You can find instructions for adding a real-time clock chip [here](https://pimylifeup.com/raspberry-pi-rtc/). I recommend the [DS3231](https://www.amazon.com/gp/product/B01N1LZSK3/ref=as_li_tl?ie=UTF8&camp=1789&creative=9325&creativeASIN=B01N1LZSK3&linkCode=as2&tag=abodit01-20&linkId=daa1415a90f1e578374ad1a2e3fa2282) which has the best resolution.


18. Optionally, turn off the on-board LED so that the code can use it to indicate activity (recommended)

`sudo sh -c "echo none > /sys/class/leds/led0/trigger"`

19. Now return to the main instructions [GettingStarted.md](GettingStarted.md)



# ADDENDUM

I no longer recommend the Raspberry PiW. It seems to be unreliable maintaining a consistent network connection. To fix that
you can try the following:-

````
sudo nano /etc/network/interfaces
````

Add the following:

````
auto lo

iface lo inet loopback
# iface eth0 inet dhcp

auto wlan0
allow-hotplug wlan0
iface wlan0 inet dhcp
wpa-conf /etc/wpa_supplicant/wpa_supplicant.conf
wireless-power off
iface default inet dhcp
````

And then reboot and check with `iwconfig` that power management is now off for `wlan0`.


# OPTIONAL RTC

The Raspberry PiW seems to suffer from clock drift. You may wish to install an RTC based on the more accurate DS3231 chip. Full instructions can be found on [pimylifeup](https://pimylifeup.com/raspberry-pi-rtc/).

In short:

1. Enable I2C using raspi-config
2. Reboot
3. `sudo apt-get install python-smbus i2c-tools`


# OPTIONAL AUTO-NAMING

If you have many Raspberry Pis that you want to setup from a common image, add this to `/etc/rc.local` before you create the image, but do not run it. On first-run
it will change the name from `raspberrypi` to `Crowd-XXXXXX` where XXXXXX is the serial number of the Raspberry Pi.

````
MAC="crowd-""$(grep 'Serial' /proc/cpuinfo | sed 's/^Serial.*000\([1-9a-f][0-9a-f]*\)$/\1/')"
CURRENT_HOSTNAME=$(cat /proc/sys/kernel/hostname)

if [ $CURRENT_HOSTNAME = "raspberrypi" ]
then
echo "Changing name to" $MAC "from" $CURRENT_HOSTNAME
chattr -i /etc/hostname
echo "$MAC" > "/etc/hostname"
echo "$MAC" > "/etc/salt/minion_id"
chattr -i /etc/hosts
sed -i "s/127.0.1.1\s*$CURRENT_HOSTNAME/127.0.1.1\t$MAC/g" /etc/hosts
hostname $MAC
#chattr +i /etc/hostname
#chattr +i /etc/hosts
fi
````

# SHRINK LOG FILES

````
sudo nano /etc/logrotate.conf
````

````
# see "man logrotate" for details
# rotate log files daily
daily

# keep 2 days worth of backlogs
rotate 2

# create new (empty) log files after rotating old ones
create

# use date as a suffix of the rotated file
#dateext

# uncomment this if you want your log files compressed
#compress

# packages drop log rotation information into this directory
include /etc/logrotate.d

# system-specific logs may be also be configured here.
````