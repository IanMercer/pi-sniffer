# pi-sniffer aka Crowd Alert
This project counts people!  It can figure out how many people are present in a given area using a handful of very cheap sensors. It achieves this by counting cellphones. Since most people carry one these days, and since iPhones automatically turn Bluetooth back on every day, there are enough people in most populations with active Bluetooth signals that we can calculate a total number of people with a high statistical significance.

This is not without challenges as cell phones randomize their mac address and a single person may have multiple bluetooth devices on them.

Using the count of people present you can power displays that warn people when it's too crowded. This capability can be used to reduce crowding to help prevent the spread of COVID-19. It also gives people confidence that the store, restroom, railway carriage or other enclosed space they are about to enter is not crowded, helping us all feel confident again going about our daily lives.

The code here recently won first place in the global [BetterHealth Hackathon](https://tinyurl.com/crowdalert) organized by HCL and Microsoft. 

But this system isn't limited to crowding signs. You can use the data it collects in many other ways: as an input to a smart home controlling lighting and heating based on how many people are home and which areas of the home are occupied, or as a feed to a marketing analysis system, ...

Not only does this project count how many phones are present, it tracks every other device that comes into range and can send data back over MQTT about them. For devices other than cellphones, i.e ones with either defined names or public (unchanging) mac addresses you can use this information to trigger actions. For example, put an iBeacon on your car and trigger an action when it comes home or after it's been gone for a set time.

It uses the built-in Linux BLUEZ libraries and the Bluetooth antenna on any Raspberry Pi (W, 3+, 4) to scan for nearby BLE devices. But it's also written to be as portable as possible using only the C language and avoiding dependencies unless absolutely necessary.

It reports all BLE devices found (Mac address, name, type, UUIDs, ...) and their approximate distance to an MQTT endpoint. It applies a simple Kalman filter to smooth the distance values. It also handles iPhones and other Apple devices that randomize their mac addresses periodically and can give a reliable count of how many phones/watches/... are in-range.

![image](https://user-images.githubusercontent.com/347540/85953280-1cb7f300-b924-11ea-96d5-07c217a57e24.png "Multiple Pis and many BLE devices in action")
![image](https://user-images.githubusercontent.com/347540/85953412-dd3dd680-b924-11ea-8eeb-a3b328f91d19.png "A single stationary device")

The system is also designed to be fault-tolerant. It runs just fine even when the internet is down. In a system with multiple sensors and multiple displays the loss of any one component should have minimal impact on the whole system.

# applications
* Power displays that warn when a confined space is getting too crowded to encourage people to 'socially distance' by coming back later or picking a less crowded store / room / railway carriage / bus / ...
* Detect cell phones entering your home, garden, barn, ...
* Put the heating or air conditioning on when there are two or more cellphones in the house and off otherwise
* Locate cars, dogs, ... using iBeacons attached to moving objects (reverse of iBeacon normal usage) 
* Gather other advertised data and transmit to MQTT (temperature, fitbit, cycleops, ...)

# goals
* Scan for BLE devices nearby a Raspberry Pi using the built-in Bluetooth adapter
* No external dependencies: no Python, no Node.js, no fragile package dependencies, easy portability
* Simplicity: do one thing well, no frills

# iOS MAC address randomization
Tracking iOS (and many Android) devices is complicated by the fact that they switch Mac addresses unpredictably: sometimes after a few seconds, sometimes after many minutes. You can see two MAC address swaps in this example:
![image](https://user-images.githubusercontent.com/347540/85953525-cc419500-b925-11ea-9693-012aeaa61b60.png)

There is no (easy) way to distinguish a MAC-flipping event from a new device arrival event. Until the old mac address pings again they could be the same device.
The pi-sniffer code includes an algorithm to calculate the minimum possible number of devices present assuming that any overlap in time means two sequences are different devices, but otherwise packing them all together like events on a calendar to find the minimum possible number of devices present.

Pi-sniffer transmits this min-count every time it changes so you can easily see how many devices are in range of any of your RPi devices.

![image](https://user-images.githubusercontent.com/347540/85953581-54279f00-b926-11ea-8d02-fb155d409f61.png)

It actually transmits a count for each range (1, 2, 5, ...)
![image](https://user-images.githubusercontent.com/347540/86996091-8ffd0880-c15f-11ea-991b-8a613041e4a0.png)

# MQTT topics

The MQTT packet is now JSON encoded. It includes the property that has changed and a timestamp. Properties may include `name`, `distance`, `alias`, `power`, `type`, `uuids`, `serviceData`, `manufacturer`, `manufdata`, `temperature`, `humidity`.

The MQTT topic is of the form: BLF/<hostname>/messages/events/<property_name>
   
For a summary of all devices seen by the access point the following topic is sent:

    BLF/<hostname>/summary/dist_hist             -- an array of bytes containing the count of devices at each range 

An `up` message is also sent on startup.

# Time

Given delays in MQTT transmit, receive and re-transmit to the receiving application it's a good idea to use the timestamp passed in the packet. Make sure all your Pis are synchronized to the same time.

# status
* Recently updated to use the Eclipse PAHO MQTT C library which support SSL and Async code. This is now the only dependency.
* This is a work in progress and is still changing fairly rapidly.
* There is a `build.sh` file that builds and runs the code. 
* There is a `log.sh` file that tails the log while it's running.
* The MQTT topic prefix, server IP (or FQDN) and port are configurable.
* Environment variables are also used to configure the RSSI to distance conversion parameters for indoor/outdoor settings.
* Multiple instances communicate over UDP on port 7779 (also configurable).
* The total number of people present x 10.0 x a configurable scale factor is sent over UDP 7778 (also configurable).  (It's actually the expectation of the probability curve for the number of people present so after a period of inactivity it goes down to zero following a curve.)
* A separate ESP8266 project is available that listens to the port and displays a crowd warning indication

# plans
* Decode advertised data for common iBeacons that also send environmental data (e.g. Sensoro)
* Gather other advertised data and transmit to MQTT including temperature, battery, steps, heart rate, ...
* Combine multiple distance values to do trilateration and approximate location, simple ML model

# getting started

* Set up instructions can be found [here](GettingStarted.md).


# FAQ

Please see the [FAQ](FAQ.md) for more information.

