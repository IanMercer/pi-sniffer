/*
    Bluetooth wrapper methods around BLUEZ over DBUS
*/


#include "bluetooth.h"

/*
    print_and_free_error
*/
void print_and_free_error(GError *error) 
{
  if (error)
  {
       g_print("Error: %s\n", error->message);
       g_error_free (error);
  }
}



int bluez_set_discovery_filter(GDBusConnection *conn)
{
    int rc;
    GVariantBuilder *b = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(b, "{sv}", "Transport", g_variant_new_string("le")); // or "auto"
    //g_variant_builder_add(b, "{sv}", "RSSI", g_variant_new_int16(-150));
    g_variant_builder_add(b, "{sv}", "DuplicateData", g_variant_new_boolean(TRUE));
    g_variant_builder_add(b, "{sv}", "Discoverable", g_variant_new_boolean(TRUE));
    g_variant_builder_add(b, "{sv}", "Pairable", g_variant_new_boolean(TRUE));
    g_variant_builder_add(b, "{sv}", "DiscoveryTimeout", g_variant_new_uint32(0));

    //GVariantBuilder *u = g_variant_builder_new(G_VARIANT_TYPE_STRING_ARRAY);
    //g_variant_builder_add(u, "s", argv[3]);
    //g_variant_builder_add(b, "{sv}", "UUIDs", g_variant_builder_end(u));

    GVariant *device_dict = g_variant_builder_end(b);

    //g_variant_builder_unref(u);
    g_variant_builder_unref(b);

    rc = bluez_adapter_call_method(conn, "SetDiscoveryFilter", g_variant_new_tuple(&device_dict, 1), NULL);

    // no need to ... g_variant_unref(device_dict);

    if (rc)
    {
        g_print("Not able to set discovery filter\n");
        return 1;
    }

    rc = bluez_adapter_call_method(conn, "GetDiscoveryFilters",
                                   NULL,
                                   bluez_get_discovery_filter_cb);
    if (rc)
    {
        g_print("Not able to get discovery filter\n");
        return 1;
    }
    return 0;
}


int bluez_adapter_connect_device(GDBusConnection *conn, char *address)
{
	int rc = bluez_device_call_method_address(conn, "Connect", address, NULL, NULL);
	if(rc) {
		g_print("Not able to call Connect\n");
		return 1;
	}
	return 0;
}

int bluez_device_call_method_address(GDBusConnection *conn, const char *method, char* address, GVariant *param, method_cb_t method_cb)
{
    //    g_print("bluez_device_call_method(%s)\n", method);
    GError *error = NULL;
    char path[100];

    // e.g. /org/bluez/hci0/dev_C1_B4_70_76_57_EE
    get_path_from_address(address, path, sizeof(path));
    //g_print("Path %s\n", path);

    g_dbus_connection_call(conn,
                           "org.bluez",
                           path,
                           "org.bluez.Device1",
                           method,
                           param,
                           NULL,                       // the expected type of the reply (which will be a tuple), or null
                           G_DBUS_CALL_FLAGS_NONE,
                           20000,                      // timeout in millseconds or -1
                           NULL,                       // cancellable or null
                           method_cb,                  // callback or null
                           &error);
    if (error != NULL)
    {
       print_and_free_error(error);
       return 1;
    }
    return 0;
}


int bluez_adapter_call_method(GDBusConnection *conn, const char *method, GVariant *param, method_cb_t method_cb)
{
    //    g_print("bluez_adapter_call_method(%s)\n", method);
    GError *error = NULL;

    g_dbus_connection_call(conn,
                           "org.bluez", /* TODO Find the adapter path runtime */
                           "/org/bluez/hci0",
                           "org.bluez.Adapter1",
                           method,
                           param,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           method_cb,
                           &error);
    if (error != NULL)
    {
       print_and_free_error(error);
       return 1;
    }
    return 0;
}


/*
      Get a single property from a Bluez device
*/

/*
static int bluez_adapter_get_property(const char* path, const char *prop, method_cb_t method_cb)
{
	GError *error = NULL;

	g_dbus_connection_call(conn,
				     "org.bluez",
                                     path,
				     "org.freedesktop.DBus.Properties",
				     "Get",
                                     g_variant_new("(ss)", "org.bluez.Adapter1", prop),
				     // For "set": g_variant_new("(ssv)", "org.bluez.Device1", prop, value),
				     NULL,
				     G_DBUS_CALL_FLAGS_NONE,
				     20000,
				     NULL,
                                     method_cb,
				     &error);
	if(error != NULL)
		return 1;

	return 0;
}
*/


/*
   get_discovery_filter
*/
void bluez_get_discovery_filter_cb(GObject *conn, GAsyncResult *res, gpointer data)
{
    (void)data;

    g_debug("START bluez_get_discovery_filter_cb\n");

    GVariant *result = NULL;
    GError *error = NULL;

    result = g_dbus_connection_call_finish((GDBusConnection *)conn, res, &error);

    if (result == NULL || error) 
    {
        g_warning("Unable to get result for GetDiscoveryFilter\n");
        print_and_free_error(error);
    }

    if (result)
    {
        GVariant *child = g_variant_get_child_value(result, 0);
        pretty_print("GetDiscoveryFilter", child);
        g_variant_unref(child);
    }
    g_variant_unref(result);
    g_debug("DONE bluez_get_discovery_filter_cb\n");
}

/*
   bluez_adapter_disconnect_device
*/
int bluez_adapter_disconnect_device(GDBusConnection *conn, char *address)
{
	int rc = bluez_device_call_method_address(conn, "Disconnect", address, NULL, NULL);
	if(rc) {
		g_print("Not able to call Disconnect\n");
		return 1;
	}
	return 0;
}

/*
    bluez_remove_device
 */
int bluez_remove_device(GDBusConnection *conn, char address[18])
{
        // RemoveDevice takes an argument to the object path i.e /org/bluez/hciX/dev_XX_YY_ZZ_AA_BB_CC.
        GVariant *vars[1];
        vars[0] = g_variant_new_string(address);
        GVariant *param = g_variant_new_tuple(vars, 1); // floating

        int rc = bluez_device_call_method_address(conn, "RemoveDevice", address, param, NULL);
        if (rc) g_debug("Not able to remove %s", address);
        return rc;
}



/*
    bluez_adapter_set_property
*/
int bluez_adapter_set_property(GDBusConnection *conn, const char *prop, GVariant *value)
{
    GVariant *result;
    GError *error = NULL;
    GVariant *gvv = g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value);

    result = g_dbus_connection_call_sync(conn,
                                         "org.bluez",
                                         "/org/bluez/hci0",
                                         "org.freedesktop.DBus.Properties",
                                         "Set",
                                         gvv,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if (result != NULL)
      g_variant_unref(result);

    // not needed: g_variant_unref(gvv);

    if (error != NULL)
    {
       print_and_free_error(error);
       return 1;
    }

    return 0;
}


/*
    A mini-database of names that can be mapped onto categories if they match the start
    of a Bluetooth name.
*/
char* POS_terminals[] = { "IBM", "Bluesnap", "NWTR040" };   // CATEGORY_FIXED
char* phones[] = { "iPhone" };
char* tablets[] = { "iPad", };
char* computers[] = { "MacBook Pro", "BOOTCAMP", "BOOTCAMP2"};
char* wearables[] = { "Apple Watch", "iWatch", "Galaxy Watch", "Gear S3", "fenix", 
    "Mi Smart Band", "Ionic", "Versa", "TICKR X",
    "Charge 2", "Charge 3" };
char* headphones[] = {"Sesh Evo-LE", "F2", "Jabra", "LE-Bose", "LE-reserved_C", "Blaze", "HarpBT"};
char* tvs[] = {"AppleTV", "Apple TV", "[TV] Samsung", "[Signage] Samsung"};
char* printers[] = {"ENVY Photo"};
char* beacons[] = {"AprilBeacon", "abtemp", "abeacon", "estimote", "EST", "Tile", "LYWSD03MMC"};
char* cars[] = {"Audi", "BMW", "VW ", "GM_PEPS_", "Land Rover", "Subaru"};
