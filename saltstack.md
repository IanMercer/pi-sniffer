# Configuring for Salt-Stack deployment

Install salt stack minion:

````
curl -L https://bootstrap.saltstack.com -o install_salt.sh
sudo sh install_salt.sh -P -x python3
````

Edit the "master" directive in the minion configuration file, typically `sudo nano /etc/salt/minion`, as follows:

````
- #master: salt
+ master: salt.abodit.com
````

Edit the salt grains file to define information about this device:

````
sudo nano /etc/salt/grains
````

Add groups:

````
group: pi4
group: location_name
````



After updating the configuration file, restart the Salt minion

````
sudo /etc/init.d/salt-minion restart
````

See the [minion configuration reference](https://docs.saltstack.com/en/latest/ref/configuration/index.html) for more details about other configurable options.






# Master Salt Administration

This section only applies if you are using Salt to manage your own network of devices.

List any devices waiting to join: `sudo salt-key -L`
Accept all devices waiting to join: `sudo salt-key -A`

Check that all minions have specific grains (metadata) defined, e.g. a 'floor' value:

````
sudo salt -t 30 '*' grains.get 'floor'
````



