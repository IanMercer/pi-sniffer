# Raspberry Pi Headless Setup

The [Raspberry Pi 3 Model B+](https://www.amazon.com/gp/product/B07P4LSDYV/ref=as_li_qf_asin_il_tl?ie=UTF8&tag=abodit01-20&creative=9325&linkCode=as2&creativeASIN=B07P4LSDYV&linkId=bb998b957f8181fc90bb029247d63fce) is the recommended Raspberry Pi for Crowd Alert
because it has the best Bluetooth range of all the models tested. You'll also need a 5V USB power supply and micro-USB cable and an SD card. I recommend the [Sandisk Industrial 8GB UHS-1 micro-SD cards](https://www.amazon.com/gp/product/B07BLQHVQD/ref=as_li_tl?ie=UTF8&camp=1789&creative=9325&creativeASIN=B07BLQHVQD&linkCode=as2&tag=abodit01-20&linkId=03b8fd807cc5f403a952cf74b9084e89).

1. Download the *Raspberry Pi OS (32-bit) Lite* image from https://www.raspberrypi.org/downloads/raspberry-pi-os/

2. Burn the image to a micro-SD card using Etcher: https://www.balena.io/etcher/

3. Create a `wpa_supplicant.conf` following the instructions here: https://www.raspberrypi.org/documentation/configuration/wireless/headless.md

4. Add a blank file called `ssh` with no extension in the root directory too.

5. Plug the Pi in and find what IP address it was allocated
You could connect a monitor and keyboard, or you can look at your router to see what IP was allocated or
you can do a portscan on the local network to find it. (Change the base address to match your local network.)
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

8. Change the default password for the pi user.

9. Change the hostname and hosts by replacing `raspberrypi` with your chosen name
 in both of these locations, and reboot:

````
sudo nano \etc\hostname
sudo nano \hosts
sudo reboot
````

10. Reconnect and add a new root user (and delete the pi user later).

````
sudo adduser <username>
sudo usermod -aG sudo <username>
sudo groups pi
sudo adduser <username> adm
# repeat for any other groups pi was in that you want this user to be in
````

12. Optionally add password-less SSH access from your controlling computer

See https://lintut.com/how-to-setup-ssh-passwordless-login-on-centos-7-rhel-7-rhel-8/
````
ssh-copy-id -i ~/.ssh/id_rsa.pub ian@192.168.0.178
````

11. Log back in as the new root user after updating your Windows Terminal settings.

12. Update packages to latest version
````
sudo apt-get update
sudo apt-get upgrade
````

13. Set you local time zone and check it
````
sudo timedatectl set-timezone America/Los_Angeles
timedatectl status
````

14. Optionally allow `journalctl` to persist logs between boots:
````
sudo nano /etc/systemd/journald.conf
````

Under the `[Journal]` section, set the Storage= option to "persistent" to enable persistent logging:

````
[Journal]
Storage=persistent
````

This allows you to `journalctl --list-boots` and to display the journal for a specific boot, e.g. `-b -1`.

15. Now return to the main instructions [README.md](README.md)

