# Fault-tolerance

The system is designed to be fault-tolerant. If one sensor has repeated problems communicating
or runs out of memory it will restart the systemd service.

When a sensor restarts it may quickly regain its state both from BLUEZ's cache and from other
sensors nearby which may send it updates to devices it has seen but has not yet learned about.

Sensors in the same area compete to own a device as the closest sensor. When a sensor is removed
those devices are automatically reallocated to other sensors in the group that can still seen
the device. This maintains an accurate count even in the event of a single (or multiple) sensor
loss.

