/*
    Rooms
*/

#include "rooms.h"
#include "cJSON.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <glib.h>

// Max ever
#define N_ROOMS 15

// global singleton
struct room* house[N_ROOMS];


struct room* create(char* jsonstring)
{
    struct room* r = malloc(sizeof(struct room));
    r->weights = NULL;  // head of chain

    cJSON *json = cJSON_Parse(jsonstring);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            g_error("Error before: %s", error_ptr);
        }
        exit(EXIT_FAILURE);
    }

    cJSON* name = cJSON_GetObjectItemCaseSensitive(json, "name");
    if (cJSON_IsString(name) && (name->valuestring != NULL))
    {
        r->name = strdup(name->valuestring);
    }

    cJSON* group = cJSON_GetObjectItemCaseSensitive(json, "group");
    if (cJSON_IsString(group) && (group->valuestring != NULL))
    {
        r->group = strdup(group->valuestring);
    }

    struct weight* current = NULL;  // linked list

    cJSON* weights = cJSON_GetObjectItemCaseSensitive(json, "weights");
    if (!cJSON_IsArray(weights)){
            g_error("Could not parse weights[] for %s in %s", r->name, r->group);
            exit(EXIT_FAILURE);
    }

    cJSON* weight = NULL;
    cJSON_ArrayForEach(weight, weights)
    {
        cJSON *ap = cJSON_GetObjectItemCaseSensitive(weight, "n");
        cJSON *w = cJSON_GetObjectItemCaseSensitive(weight, "v");

        if (cJSON_IsString(ap) && cJSON_IsNumber(w)){
            struct weight* we = malloc(sizeof(struct weight));
            we->name = strdup(ap->valuestring);
            we->weight = w->valuedouble;
            we->next = NULL;

            if (current == NULL)
            {
                r->weights = we;
            }
            else
            {
                current->next = we;
            }
            current = we;
        }
        else {
            g_error("Could not parse weights for %s in %s", r->name, r->group);
            exit(EXIT_FAILURE);
        }
    }

    cJSON_Delete(json);

    return r;
}


/*
    Initalize the rooms database
*/
struct room** get_rooms(int* room_count)
{
    // Read from file ...

    // If no file, calculate from access points


    // {"barn":3.8,"barn2":0,"garage":0,"kitchen":0,"livingroom":11.8,"mobile":10.9,"office":0,"store":0,"study":6.5,"ubuntu":7}
    struct room* office = create ("{\"name\":\"Office\",\"group\":\"House\",\"weights\":[{\"n\":\"office\",\"v\":2.5},{\"n\":\"livingroom\",\"v\":11.8},{\"n\":\"mobile\",\"v\":10.9},{\"n\":\"study\",\"v\":6.5},{\"n\":\"ubuntu\",\"v\":7.6}]}");

    // {"office":0,"store":0,"study":0,"garage":8.1,"livingroom":0,"mobile":0,"barn2":0,"ubuntu":3.2,"kitchen":0}
    // {"office":5.7,"store":23.7,"study":12,"garage":0,"livingroom":0,"mobile":0,"barn2":0,"ubuntu":4.6,"kitchen":0}
    struct room* media_room = create("{\"name\":\"Media\",\"group\":\"House\",\"weights\":[{\"n\":\"garage\", \"v\":8.1},{\"n\":\"ubuntu\", \"v\":3.5},{\"n\":\"office\", \"v\":5.7}]}");

    // {"barn":0,"barn2":0,"garage":0,"kitchen":26.1,"livingroom":3.8,"mobile":3.4,"office":0,"store":0,"study":1.6,"ubuntu":0}
    // {"barn":0,"barn2":0,"garage":6.3,"kitchen":12.1,"livingroom":4,"mobile":6.5,"office":0,"store":6.8,"study":4.4,"ubuntu":5.6}
    struct room* study = create("{\"name\":\"Study\",\"group\":\"House\",\"weights\":[{\"n\":\"kitchen\", \"v\":17.1 },{\"n\":\"livingroom\",\"v\":3.8},{\"n\":\"mobile\", \"v\":5.4 },{\"n\":\"study\", \"v\":3.0 },{\"n\":\"ubuntu\", \"v\":5.6 }]}");

    // {"barn":0,"barn2":0,"garage":0,"kitchen":0,"livingroom":10.3,"mobile":0,"office":0,"store":0,"study":5.6,"ubuntu":0}
    // {"barn":0,"barn2":0,"garage":5.5,"kitchen":10.7,"livingroom":5,"mobile":10.6,"office":0,"store":6.7,"study":5.1,"ubuntu":6.3}
    // {"garage":3.4,"kitchen":17.8,"livingroom":6.6,"mobile":8.1,"office":0,"store":8.1,"study":0,"ubuntu":0}
    //
    // {"barn":0,"barn2":0,"garage":3.9,"kitchen":13.6,"livingroom":5.7,"mobile":9.9,"office":0,"store":5.8,"study":6.7,"ubuntu":7.8}
    // {"barn":0,"barn2":0,"garage":3.3,"kitchen":13.1,"livingroom":8.2,"mobile":13.1,"office":0,"store":10,"study":6.2,"ubuntu":4}
    struct room* kitchen = create("{\"name\":\"Kitchen\",\"group\":\"House\", \"weights\":[{\"n\":\"garage\",\"v\":4.5},{\"n\":\"kitchen\", \"v\":13.7},{\"n\":\"livingroom\", \"v\":5},{\"n\":\"mobile\", \"v\":9.6},{\"n\":\"store\", \"v\":7.7}, {\"n\":\"study\", \"v\":5.1}, {\"n\":\"ubuntu\", \"v\":6.3}]}");

// {"office":0,"store":4.4,"mobile":11.9,"study":0,"livingroom":0,"ubuntu":0,"garage":4.1,"kitchen":16.3}
// {"office":0,"study":5.8,"kitchen":12.8,"store":3.9,"ubuntu":9.2,"livingroom":5.5,"mobile":8.8,"garage":4.7}
// {"office":0,"ubuntu":0,"mobile":10.9,"livingroom":6.9,"store":3.1,"study":10.3,"garage":2.6,"kitchen":10,"barn2":0,"media":0}
// {"office":0,"ubuntu":0,"mobile":11.4,"livingroom":13.9,"store":1.5,"study":13.9,"garage":2.6,"kitchen":10,"barn2":0,"media":0}
struct room* masterbath = create("{\"name\":\"MasterBath\",\"group\":\"House\", \"weights\":[{\"n\":\"store\",\"v\":4.4},{\"n\":\"mobile\",\"v\":11.9 },{\"n\":\"garage\",\"v\":4.1 },{\"n\":\"kitchen\",\"v\":16.3}]}");

// {"garage":0,"kitchen":19.6,"livingroom":0,"mobile":12.8,"office":0,"store":1.1,"study":0,"ubuntu":0}
struct room* masterbed = create("{\"name\":\"MasterBed\",\"group\":\"House\", \"weights\":[{\"n\":\"kitchen\",\"v\":19.6},{\"n\":\"mobile\", \"v\":12.8 }, {\"n\":\"garage\", \"v\":4.1 },{\"n\":\"store\", \"v\":1.1}]}");

struct room* garage = create("{\"name\":\"Garage\",\"group\":\"House\", \"weights\":[{\"n\":\"kitchen\", \"v\":12.0 },{\"n\":\"livingroom\", \"v\":10.6 }, {\"n\":\"garage\", \"v\":2.4 },{\"n\":\"store\", \"v\":5.6}, { \"n\":\"ubuntu\", \"v\":10.0}]}");

struct room* upstairs = create("{\"name\":\"Upstairs\",\"group\":\"House\", \"weights\":[{\"n\":\"livingroom\",\"v\":10.9},{\"n\":\"garage\", \"v\":14.1 }, {\"n\":\"store\", \"v\":4.2 }, {\"n\":\"mobile\", \"v\":3.2 }, {\"n\":\"study\", \"v\":10.9}]}");

// {"office":0,"mobile":6.4,"study":8,"store":0,"ubuntu":0,"garage":0,"livingroom":2.7,"kitchen":1.3}
struct room* sunroom = create("{\"name\":\"Sunroom\",\"group\":\"House\", \"weights\":[{\"n\":\"livingroom\",\"v\":3.7},{\"n\":\"garage\", \"v\":9.5 }, {\"n\":\"mobile\", \"v\":6.2 }, {\"n\":\"study\", \"v\":9.5 },{\"n\":\"kitchen\", \"v\":2.2}]}");

// {"office":0,"store":0,"study":8.1,"garage":0,"livingroom":1.3,"mobile":0,"barn2":0,"ubuntu":0,"kitchen":6.1}
// {"office":0,"store":0,"study":6.3,"garage":7.8,"livingroom":0.7,"mobile":0,"barn2":0,"ubuntu":0,"kitchen":4.1}
// {"office":0,"store":0,"study":7.2,"garage":0,"livingroom":1.4,"mobile":7.8,"barn2":0,"ubuntu":0,"kitchen":5.6}
// {"office":0,"garage":7.4,"mobile":7.1,"kitchen":1.7,"ubuntu":0,"store":14.1,"study":5.3,"livingroom":1.2}
// {"barn":0,"barn2":0,"garage":9.2,"kitchen":7.1,"livingroom":0.8,"mobile":8.6,"office":0,"store":15,"study":2.2,"ubuntu":0}
struct room* livingroom = create("{\"name\":\"Living\",\"group\":\"House\", \"weights\":[{\"n\":\"garage\", \"v\":9.2 },{\"n\":\"kitchen\", \"v\":7.1 }, {\"n\":\"livingroom\", \"v\":0.8 }, {\"n\":\"mobile\", \"v\":8.1 }, {\"n\":\"study\", \"v\":3.0}]}");

struct room* barn = create("{\"name\":\"Barn\",\"group\":\"House\", \"weights\":[{\"n\":\"barn\", \"v\":10.0}, {\"n\":\"barn2\", \"v\":10.0},{\"n\":\"garage\", \"v\":-1 },{\"n\":\"kitchen\", \"v\":-1 }, {\"n\":\"livingroom\", \"v\":-1 }, {\"n\":\"mobile\", \"v\":-1 }, {\"n\":\"study\", \"v\":-1 },{\"n\":\"store\", \"v\":-1.0}]}");

struct room* neighbors = create("{\"name\":\"Neighbors\", \"group\":\"Away\", \"weights\":[{\"n\":\"barn\", \"v\":-1.0 },{\"n\":\"store\", \"v\":15.0}]}");

struct room* outside_east = create("{\"name\":\"Outside East\",\"group\":\"Away\", \"weights\":[{\"n\":\"barn\", \"v\":-1.0 }, {\"n\":\"barn2\", \"v\":-1.0 },{\"n\":\"store\", \"v\":8.0}]}");

// {"garage":0,"kitchen":0,"livingroom":10.3,"mobile":14.4,"office":0,"store":0,"study":0,"ubuntu":0}  ???
struct room* outside_west = create("{\"name\":\"Outside West\",\"group\":\"Away\", \"weights\":[{\"n\":\"barn\", \"v\":-1.0 }, {\"n\":\"barn2\", \"v\":-1.0 },{\"n\":\"livingroom\", \"v\":10.3 },{\"n\":\"mobile\", \"v\":14.4}]}");

struct room* outside_north = create("{\"name\":\"Outside North\",\"group\":\"Away\", \"weights\":[{\"n\":\"barn\", \"v\":-1.0 }, {\"n\":\"barn2\", \"v\":-1.0 },{\"n\":\"kitchen\", \"v\":14.6}]}");

    // TODO: Load this from a file once on startup or default to a per-sensor style of rooms
    house[0] = office;
    house[1] = media_room;
    house[2] = barn;
    house[3] = garage;
    house[4] = study;
    house[5] = kitchen;
    house[6] = neighbors;
    house[7] = outside_east;
    house[8] = outside_west;
    house[9] = outside_north;
    house[10] = upstairs;
    house[11] = livingroom;
    house[12] = sunroom;
    house[13] = masterbath;
    house[14] = masterbed;
    *room_count = N_ROOMS;
    return house;
}

