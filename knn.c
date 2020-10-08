/*
    K-Nearest Neighbors Classifier
*/
#include "knn.h"
#include "accesspoints.h"
#include "cJSON.h"
#include "utility.h"

#include <gio/gio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>

// UTILITY METHODS

/*
   Get JSON for a recording as a string, must call free() on string after use
*/
char* recording_to_json (struct recording* r, struct AccessPoint* access_points)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

    // TODO: Add date time stamp
    cJSON_AddStringToObject(j, "room", r->room_name);

    cJSON * jarray = cJSON_AddArrayToObject(j, "distances");

    int i = 0;
    for (struct AccessPoint* ap = access_points; ap != NULL; ap = ap->next)
    {
        cJSON * jap = cJSON_CreateObject();
        cJSON_AddNumberToObject(jap, ap->client_id, r->access_point_distances[i]);
        i++;
        cJSON_AddItemToArray(jarray, jap);
    }

    string = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    return string;
}

/*
    Convert JSON lines value back to a recording value using current order of access points
*/
bool json_to_recording(char* buffer, struct AccessPoint* access_points, struct recording* r)
{
    cJSON *json = cJSON_Parse(buffer);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            g_warning("Error reading saved recording before: %s", error_ptr);
        }
        return FALSE;
    }

    cJSON* room_name = cJSON_GetObjectItemCaseSensitive(json, "room");
    if (!cJSON_IsString(room_name))
    {
        return FALSE;
    }

    strncpy(r->room_name, room_name->valuestring, META_LENGTH);

    cJSON* distances = cJSON_GetObjectItemCaseSensitive(json, "distances");
    if (!cJSON_IsArray(distances)){
        g_error("Could not parse distances[] from saved recording");
        return FALSE;
    }

    cJSON* distance = NULL;
    cJSON_ArrayForEach(distance, distances)
    {
        bool found = FALSE;
        int i = 0;
        for (struct AccessPoint* ap = access_points; ap != NULL; ap = ap->next)
        {
            if (strcmp(ap->client_id, distance->string) == 0)
            {
                if (cJSON_IsNumber(distance))
                {
                    r->access_point_distances[i] = distance->valuedouble;
                }
                found = TRUE;
                break;
            }
            i++;
        }

        if (!found)
        {
            g_warning("Did not find access point %s", distance->string);
        }
    }

    cJSON_Delete(json);

    g_debug("free buffer");
    g_free(buffer);
    return TRUE;
}

// KNN CLASSIFIER

float score (struct recording* recording, float access_points_distance[N_ACCESS_POINTS], int n_distances)
{

    float sum_delta_squared = 0.0;
    for (int i = 0; i < n_distances; i++)
    {
        float delta = access_points_distance[i] - recording->access_point_distances[i];
        sum_delta_squared += delta*delta;
    }

    return sum_delta_squared;
}


/*
  k_nearest classifier
  Like KNeighborsClassifier(weights='distance')

  parameters
    top_k_list

  NB You must free() the returned string

*/
const char* k_nearest(struct recording* recordings, float* access_point_distances, int n_access_point_distances)
{
    struct top_k result[TOP_K_N];
    int k = 0;
    for (struct recording* recording = recordings; recording != NULL; recording = recording->next)
    {
        // Insert recording into the list if it's a better match
        float distance = score(recording, access_point_distances, n_access_point_distances);

        struct top_k current;
        g_utf8_strncpy(current.room_name, recording->room_name, META_LENGTH);
        current.distance = distance;

        // Find insertion point
        for (int i = 0; i < TOP_K_N; i++)
        {
            if (i == k)
            {
                // Off the end, but still < k, so add the item here
                k++;
                result[i] = current;
                break;
            }
            else if (result[i].distance > current.distance)
            {
                // Insert at this position, pick up current and move it down
                struct top_k temp = result[i];
                result[i] = current;
                current = temp;
            }
            else
            {
                // keep going
            }
        }
    }

    if (k == 0) return "no match";

    struct top_k best = result[0];
    float best_score = 0.0;

    // Find the most common in the top_k weighted by distance
    for (int i = 0; i < k; i++)
    {
        if (result[i].distance <= 0) continue;       // already used
        float score = 1.0 / result[i].distance;
        for (int j = i+1; j < k; j++)
        {
            if (strcmp(result[i].room_name, result[j].room_name) == 0 && result[j].distance > 0)
            {
                score += 1.0 / result[j].distance;
                result[i].distance = -1;
            }
        }

        if (score > best_score)
        {
            best_score = score;
            best = result[i];
        }
    }

    return strdup(best.room_name);
}


// FILE OPERATIONS

/**
 * Read observations
 * @file: observations file name
 * @error: a %GError to put the error code and message in, or %NULL
 *
 * Reads observations into memory
 *
 * Return value: %TRUE if there were no errors.
 *
 **/
bool read_observations (char * filename, struct AccessPoint* access_points, struct recording** recordings, GError **error)
{
    //gchar *filename = g_strdup_printf("file://%s/.extension", g_get_home_dir());
    g_debug ("observations file: %s", filename);
    GFile *file = g_file_new_for_path (filename);

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	GError *error_local = NULL;
	//g_autoptr(GFileInputStream) is = NULL;
	//g_autoptr(GDataInputStream) input = NULL;

	GFileInputStream* is = g_file_read (file, NULL, &error_local);
	if (is == NULL) {
		g_propagate_error (error, error_local);
		return FALSE;
	}

	GDataInputStream * input = g_data_input_stream_new (G_INPUT_STREAM (is));

	/* read file line by line */
	while (TRUE) {
		gchar *line;
		line = g_data_input_stream_read_line (input, NULL, NULL, NULL);
		if (line == NULL)
			break;

        struct recording r;
        bool ok = json_to_recording(line, access_points, &r);
        if (ok)
        {
            g_debug("Got a valid recording for %s", r.room_name);
            // Append it to the front of the recordings list
            // TODO: Remove duplicates
            // NB Only allocate if ok
            struct recording* ralloc = malloc(sizeof(struct recording));
            *ralloc = r;
            ralloc->next = *recordings;
            *recordings = ralloc;
        }
	}

    // close stream
    g_object_unref(is);
    g_object_unref(file);
	return TRUE;
}

/*
   free recordings linked list
*/
void free_list(struct recording** head)
{
   struct recording* tmp;

   while (*head != NULL)
    {
       tmp = *head;
       *head = (*head)->next;
       free(tmp);
    }
}


// TODO FILE READING
// TODO MULTIPLE FILES IN DIRECTORY

bool record (char* filename, double access_distances[N_ACCESS_POINTS], struct AccessPoint* access_points, char* location)
{
    //GError **error = NULL;
    GFile *file = g_file_new_for_path (filename);

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	GError *error_local = NULL;

	GFileOutputStream* os = g_file_append_to (file, G_FILE_CREATE_NONE, NULL, &error_local);
	if (os == NULL) {
        g_warning("Cold not open %s for writing recordings", filename);
		//g_propagate_error (error, error_local);
        g_object_unref(file);
		return FALSE;
	}

	GDataOutputStream * output = g_data_output_stream_new (G_OUTPUT_STREAM (os));

    struct recording r;
    for (int i = 0; i < N_ACCESS_POINTS; i++)
    {
        r.access_point_distances[i] = access_distances[i];
    }
    g_utf8_strncpy(r.room_name, location, META_LENGTH);

    char* buffer = recording_to_json(&r, access_points);
    if (buffer != NULL)
    {
        gssize written =  g_data_output_stream_put_string(output, buffer, NULL, &error_local);
        written +=  g_data_output_stream_put_string(output, "\r", NULL, &error_local);

        free(buffer);
    }

    g_object_unref(os);
    g_object_unref(file);

    return TRUE;
}
