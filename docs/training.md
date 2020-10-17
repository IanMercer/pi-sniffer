# Training

The system learns the location of any iBeacons that you place around the area and these can then be used to define rooms and groups for reporting.

The system uses 'patches' which are a roughly 3m diameter (more in an open space) patch of a room that has a fairly consistent Bluetooth signal to the sensors.
You train the system to recognize each patch and then define how those patches aggregate to rooms and groups.

The patch files are `.jsonl` format (which is just a collection of JSON objects, one per line). They are stored in the `recordings` subdirectory. A second set of
patch files is created for you automatically in the subdirectory `beacons` from any iBeacons seen in the space that are not already at a define patch.

So, on startup, place a beacon in the first patch you want to train, wait maybe 15 min for some data to accumulate. You can use as many beacons at the same time as you have
and any brand of beacon is supported including Tile, DE-WALT, Milwaukee or your own iPhone acting as an iBeacon using the `nRF Connect` app from the AppStore.

Now look in the `beacons` subdirectory and you will see the beacon(s) listed there already in the correct format for `recordings`.

````
~/pi-sniffer$ ls ./beacons
AprilBeacon_BE155C.jsonl  BR502476.jsonl  DEWALT-TAG.jsonl  estimote.jsonl  _Milwaukee.jsonl

````

Take a look inside one of these files and you will see an initial line of metadata describing the patch and how it aggregates. Below that are a 
number of recordings of actual distances that were received for this beacon:

````
{"patch":"DEWALT-TAG","room":"Unknown","group":"House","tags":"tags"}

{"patch":"DEWALT-TAG","time":"2020-10-17T09:45:04","distances":{"officepiw":10.30,"store":7.40}}
{"patch":"DEWALT-TAG","time":"2020-10-17T09:45:24","distances":{"officepiw":10.30,"store":7.40,"study":3.90}}
{"patch":"DEWALT-TAG","time":"2020-10-17T09:45:44","distances":{"kitchenpi4":0.70,"officepiw":10.30,"store":7.40,"study":3.90}}
{"patch":"DEWALT-TAG","time":"2020-10-17T09:46:44","distances":{"garage":5.80,"kitchenpi4":0.70,"officepiw":9.60,"store":8.10,"study":3.90}}

````

Move this file from the `beacons` directory to the `recordings` directory and rename it in the process to the name of the patch. For example to create a
patch called `FarBay`:

````
    mv beacons/DEWALT-TAG.jsonl recordings/farbay.jsonl 
````

Now edit that file, replace all occurrences of `DEWALT-TAG` with `FarBay` and add room and group metadata and any tags you want to pass to Grafana or any
other reporting software.

e.g.
````
    {"patch":"FarBay","room":"Driveway","group":"Office","tags":"use=parking"}

    {"patch":"FarBay","time":"2020-10-17T09:45:04","distances":{"officepiw":10.30,"store":7.40}}
    {"patch":"FarBay","time":"2020-10-17T09:45:24","distances":{"officepiw":10.30,"store":7.40,"study":3.90}}

````

This defines a patch called 'FarBay' that aggregates up to a room called 'Driveway' in a group called 'Office'.

The `time` field is provided solely for your convenience, if you move a tag around make sure you are using the right section of the file based on time.

You should also do a sanity check on the recording lines to make sure they seem reasonable. Some distances will be missing in any recording but that
doesn't matter, the system learns to handle missing distances as well as distances that are present. But if you are consistently getting very few
distances listed you may need more sensors, or you may need to move the ones you have to a higher location with less obstacles.


If conditions change and the beacon is left in the same location further recordings will be placed in the beacon directory. You can append these to
an existing patch and edit them to remove the extra metadata and rename each of the patch names from the beacon name to the patch name:

````
    cat beacons/DEWALT-TAG.jsonl >> recordings/farbay.jsonl
    nano recordings/farbay.jsonl
````

