/*
    K-Nearest Neighbors Classifier
*/
#include "state.h"
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
#include <math.h>
#include <time.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// UTILITY METHODS

/*
   Get JSON for a recording as a string, must call free() on string after use
*/
char* recording_to_json (struct recording* r, struct AccessPoint* access_points)
{
    char *string = NULL;
    cJSON *j = cJSON_CreateObject();

//    cJSON_AddStringToObject(j, "patch", r->patch_name);

    // distances

    cJSON *distances = cJSON_AddObjectToObject(j, "distances");
    for (struct AccessPoint* ap = access_points; ap != NULL; ap = ap->next)
    {
        double distance = r->access_point_distances[ap->id];
        if (distance > 0 && distance != EFFECTIVE_INFINITE)
        {
            char print_num[18];
            snprintf(print_num, 18, "%.2f", distance);
            cJSON_AddRawToObject(distances, ap->client_id, print_num);
        }
    }

    string = cJSON_PrintUnformatted(j);
    g_debug("%s", string);

    cJSON_Delete(j);
    return string;
}

/*
    Convert JSON lines value back to a recording value
*/
bool json_to_recording(char* buffer, struct OverallState* state, struct patch** current_patch, bool confirmed)
{
    if (strlen(buffer) == 0) return TRUE;

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

    // META
    cJSON* patch_name = cJSON_GetObjectItemCaseSensitive(json, "patch");
    cJSON* room_name = cJSON_GetObjectItemCaseSensitive(json, "room");
    cJSON* group_name = cJSON_GetObjectItemCaseSensitive(json, "group");
    cJSON* tags = cJSON_GetObjectItemCaseSensitive(json, "tags");

    // RECORDING
    cJSON* distances = cJSON_GetObjectItemCaseSensitive(json, "distances");

    bool has_meta = cJSON_IsString(patch_name) && cJSON_IsString(room_name) && cJSON_IsString(group_name) && cJSON_IsString(tags);
    bool has_distances = cJSON_IsObject(distances);

    if (!has_meta && !has_distances)
    {
        char missing[128];
        missing[0]='\0';
        if (!cJSON_IsString(room_name)) append_text(missing, sizeof(missing), "'%s',", "room");
        if (!cJSON_IsString(group_name)) append_text(missing, sizeof(missing), "'%s',", "group");
        if (!cJSON_IsString(tags)) append_text(missing, sizeof(missing), "'%s',", "tags");
        if (!cJSON_IsObject(distances)) append_text(missing, sizeof(missing), "'%s',", "distances");
        g_warning("Missing metadata or distances (%s) in '%s'", missing, buffer);
        cJSON_Delete(json);
        return FALSE;
    }

    // A recording should be either metadata or distances but if combined, no problem

    if (has_meta)
    {
        //g_debug("Meta OK %s", buffer);

        if ((*current_patch) != NULL)
        {
            // Not critical, if someone leaves headings in file from beacons, that's OK to make it easier to operate
            g_debug("Ignoring second heading '%s' in file for '%s'; please remove it", patch_name->valuestring, (*current_patch)->name);
        }
        else
        {
            //g_info("Heading: Patch '%s' Group name '%s', tags '%s'", patch_name->valuestring, group_name->valuestring, tags->valuestring);
            *current_patch = get_or_create_patch(patch_name->valuestring, room_name->valuestring, group_name->valuestring, tags->valuestring, &state->patches, &state->groups, confirmed);
        }
    }

    if (has_distances)
    {
        //g_debug("Distances OK %s", buffer);

        if (*current_patch == NULL)
        {
            g_warning("Missing metadata heading before distances");
            cJSON_Delete(json);
            return FALSE;
        }

        int count = 0;

        struct recording* ralloc = malloc(sizeof(struct recording));
        ralloc->confirmed = confirmed;
        g_utf8_strncpy(ralloc->patch_name, (*current_patch)->name, META_LENGTH);
        ralloc->next = state->recordings;
        state->recordings = ralloc;

        for (struct AccessPoint* ap = state->access_points; ap != NULL; ap = ap->next)
        {
            cJSON* dist = cJSON_GetObjectItem(distances, ap->client_id);
            if (cJSON_IsNumber(dist))
            {
                ralloc->access_point_distances[ap->id] = dist->valuedouble;
                count++;
            }
            else
            {
                ralloc->access_point_distances[ap->id]=30.0;  // effective infinite distance away
            }
        }

        // NOTE: This ignores access points until they have been seen independenly
        // TODO: Create these access points too? (used to be in config.json)

        if (count < 1)
        {
            //g_warning("No values found in %s", buffer);
            cJSON_Delete(json);
            return FALSE;
        }
    }

    cJSON_Delete(json);
    return TRUE;
}

// KNN CLASSIFIER

// float score (struct recording* recording, double access_points_distance[N_ACCESS_POINTS], struct AccessPoint* access_points)
// {
//     float sum_delta_squared = 0.0;
//     int matches = 0;
//     for (struct AccessPoint* ap = access_points; ap != NULL; ap=ap->next)
//     {
//         float recording_distance = recording->access_point_distances[ap->id];
//         float measured_distance = access_points_distance[ap->id];

//         // A missing distance is treated as being off at infinity (>20m)
//         // If both are zero this cancels out, but if it was expected to be here and isn't or vice-versa a distant reading is better
//         if (recording_distance == 0.0) recording_distance = 35.0;
//         if (measured_distance == 0.0) measured_distance = 35.0;

//         float delta = measured_distance - recording_distance;
//         sum_delta_squared += delta*delta;

//         if (recording_distance != 0 && measured_distance != 0) matches++;
//     }

//     // better score for more matches
//     if (matches > 0) return sqrt(sum_delta_squared) / matches;
//     // otherwise some large number
//     return sqrt(sum_delta_squared);
// }

#define MAX_RANGE 30

// Ratio of a to b when between but straight line outside -ve or +ve depending on which direction it must be
float calculate_ratio(float distance_a, float distance_b)
{
    if (distance_a != 0 && distance_b != 0) {
        return distance_a / (distance_a + distance_b);
    } else if (distance_a != 0) {
        return -distance_a / MAX_RANGE;
    }
    else if (distance_b != 0) {
        return 1 + distance_b / MAX_RANGE;
    }
    else {
        return 0.0;
    }
}


// New approach
//   Look across all rows in a patch, each row could support or deny a patch match
//   If there's a patch row with x=1.2m then a value 9.8m cannot be a good match

// So summarize all rows in a patch
//  Get distribution for x
//  Calculate probability that y is from distribution x

// RECORDING
//
//   Has a matching access point.  Calculate difference ... squared. Take best.
//   Does not have this access point for any row.  Score = 30^30 if observation has it.
//   
//   AP             A    B    C
//   Observation:   2   10    5
//   Recording:     3    -    8
//
//


//  For each row, can observation explain it?
//    If yes, 


float score (struct recording* recording, double access_points_distance[N_ACCESS_POINTS], struct AccessPoint* access_points)
{
    if (access_points->next == NULL) 
    {
        float sum_delta_squared = 0.0;
        // single access point
        struct AccessPoint* ap = access_points;
        float recording_distance = recording->access_point_distances[ap->id];
        float measured_distance = access_points_distance[ap->id];
        float delta = (recording_distance - measured_distance) / 30.0;  // scale to similar to ratios
        sum_delta_squared += delta*delta;
        return sqrt(sum_delta_squared);
    } 
    else 
    {
        float sum_delta_squared = 0.0;
        // int matched_distances = 0;
        // int missing_distances = 0;
        // int extra_distances = 0;

        for (struct AccessPoint* ap = access_points; ap != NULL; ap=ap->next)
        {
            float recording_distance = recording->access_point_distances[ap->id];
            float measured_distance = access_points_distance[ap->id];

            if (recording_distance > EFFECTIVE_INFINITE-0.10 && measured_distance > EFFECTIVE_INFINITE-0.10)
            {
                // good, did not expect this distance to be here, but less information than a match
                sum_delta_squared += 0.5;
            }
            else if (recording_distance > EFFECTIVE_INFINITE - 0.10)
            {
                sum_delta_squared += 2;  // the observation could see the AP but this recording says you cannot
                // e.g. barn says you cannot see study, so if you can see study you can't be here
            }
            else if (measured_distance > EFFECTIVE_INFINITE - 0.10)
            {
                sum_delta_squared += 0.9;  // could not see an AP at all, but should have been able to, could just be a missing observation
            }
            else
            {
                // could be zero if both are undefned, hence ^
                float delta = (recording_distance - measured_distance);
                float increment = (delta * delta) / (EFFECTIVE_INFINITE * EFFECTIVE_INFINITE);
                if (increment > 0.92) g_warning("Increment %.2f %.2f - %.2f", increment, recording_distance, measured_distance);
                sum_delta_squared += increment;
            }
        }
        return sqrt(sum_delta_squared);
    }

}


/*
  k_nearest classifier
  Like KNeighborsClassifier(weights='distance')

  parameters
    top_k_list

  NB You must free() the returned string

*/
int k_nearest(struct recording* recordings, double* access_point_distances, struct AccessPoint* access_points,
              struct top_k* top_result, int top_count, 
              bool confirmed, bool debug)
{
    int k_result = 0;   // length of the final array after summarization

    struct top_k result[TOP_K_N];
    int k = 0;
    int test_count = 0;
    for (struct recording* recording = recordings; recording != NULL; recording = recording->next)
    {
        if (confirmed && !recording->confirmed) continue;
        test_count++;

        // Insert recording into the list if it's a better match
        float distance = score(recording, access_point_distances, access_points);

        struct top_k current;
        g_utf8_strncpy(current.patch_name, recording->patch_name, META_LENGTH);
        current.distance = distance;
        current.used = FALSE;

        //if (debug) g_debug("Patch %s Distance %.2f", current.patch_name, distance);

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

    if (k == 0) {
        top_result->distance = 100000;
        g_utf8_strncpy(top_result->patch_name, "unknown", META_LENGTH);
        return 0;
    };

    //if (debug) g_debug("Test count %i", test_count);

    float smoothing = 0.1;

    // Find the most common in the top_k weighted by distance
    for (int i = 0; i < k; i++)
    {
        if (result[i].used) continue;

        float best_distance = result[i].distance;
        float score = 1.0 / (smoothing + result[i].distance);
        int tc = 1;

        for (int j = i+1; j < k; j++)
        {
            if (strcmp(result[i].patch_name, result[j].patch_name) == 0)
            {
                tc++;
                result[j].used = TRUE;
                if (result[j].distance > 0)
                {
                    score += 1.0 / (smoothing + result[j].distance);
                    if (result[j].distance < best_distance)
                    {
                        best_distance = result[i].distance;
                    }
                }
            }
        }

        score = score / tc;   // average of scores for same patch that landed in top K

        // if (debug) g_debug("Patch '%s' %s has score %.3f over %i best distance=%.2f", result[i].patch_name,
        //   confirmed ? "confirmed" : "all",
        //   score, tc, best_distance);

        struct top_k current_value = result[i];
        current_value.distance = score;

        // Find insertion point, highest score at top 
        for (int m = 0; m < top_count; m++)   // top_count is maybe 3 but other array is larger
        {
            if (m == k_result)
            {
                // Off the end, but still < k, so add the item here
                k_result++;
                top_result[m] = current_value;
                break;
            }
            else if (top_result[m].distance < score)
            {
                // Insert at this position, pick up current and move it down
                struct top_k temp = top_result[m];
                top_result[m] = current_value;
                current_value = temp;
            }
            else
            {
                // keep going
            }
        }
    }

    return k_result;       // just the top result for now
}


// FILE OPERATIONS

bool read_observations_file (const char * dirname, const char* filename, struct OverallState* state, bool confirmed)
{
	g_return_val_if_fail (filename != NULL, FALSE);

    // only match JSONL files
    if (!string_ends_with(filename, ".jsonl")) return FALSE;

    char fullpath[128];
    g_snprintf(fullpath, sizeof(fullpath), "%s/%s", dirname, filename);

    //gchar *filename = g_strdup_printf("file://%s/.extension", g_get_home_dir());

    GFile *file = g_file_new_for_path (fullpath);

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

    GError **error = NULL;
	GError *error_local = NULL;

	GFileInputStream* is = g_file_read (file, NULL, &error_local);
	if (is == NULL) {
		g_propagate_error (error, error_local);
        g_warning("Could not open file %s: %s", fullpath, error_local->message);
		return FALSE;
	}

	GDataInputStream * input = g_data_input_stream_new (G_INPUT_STREAM (is));

	/* read file line by line */
    int line_count = 0;

    // Track the current patch from a heading and apply to recordings in same file
    struct patch* current_patch = NULL;

	while (TRUE) {
		gchar *line;
        gsize length;
		line = g_data_input_stream_read_line (input, &length, NULL, NULL);
		if (line == NULL)
			break;

        line_count++;

        // Skip comment lines
        if (string_starts_with(line, "#")) { g_free(line); continue; }

        trim(line);
        if (strlen(line) > 0)
        {
            //g_debug("%s", line);
            bool ok = json_to_recording(line, state, &current_patch, confirmed);
            if (!ok)
            {
                // TODO: Log just once per missing access point
                //g_warning("Could not use '%s' in %s (%i)", line, filename, line_count);
            }
        }
        g_free(line);
	}

    // close stream
    g_input_stream_close(G_INPUT_STREAM(is), NULL, &error_local);
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

/*
  Create the recordings or beacon directory
*/
void ensure_directory(const char* directory)
{
	GError *error_local = NULL;

    GFile* dir2 = g_file_new_for_path(directory);
    if (g_file_make_directory(dir2, NULL, &error_local))
    {
        g_info("Created recordings directory '%s'", directory);
    }
    g_object_unref(dir2);
}

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
bool read_observations (const char * dirname,  struct OverallState* state, bool confirmed)
{
    ensure_directory(dirname);

    //g_info("Reading directory '%s'",dirname);

    GDir *dir;
    GError *error;
    const gchar *filename;

    bool ok = TRUE;

    dir = g_dir_open(dirname, 0, &error);

    if (dir != NULL)
    {
        while ((filename = g_dir_read_name(dir)))
        {
            //g_debug("Reading file '%s'",filename);
            ok = read_observations_file(dirname, filename, state, confirmed) && ok;
        }
        g_dir_close(dir);
    }

    return TRUE;
}

/*
    record_observation
*/
bool record (const char* directory, const char* device_name, double access_distances[N_ACCESS_POINTS], struct AccessPoint* access_points, char* location)
{
	GError *error_local = NULL;

    ensure_directory(directory);

    if (strlen(device_name) == 0) {
        g_debug("Empty device name passed to record method, skipped");
        return FALSE;
    }

    char fullpath[128];
    g_snprintf(fullpath, sizeof(fullpath), "%s/%s.jsonl", directory,  device_name);

    GFile *file = g_file_new_for_path (fullpath);

	g_return_val_if_fail (G_IS_FILE (file), FALSE);

    bool is_new = !g_file_query_exists(file, NULL);

	GFileOutputStream* os = g_file_append_to (file, G_FILE_CREATE_NONE, NULL, &error_local);
	if (os == NULL) {
        g_warning("Could not open %s for writing recordings: %s", fullpath, error_local->message);
		//g_propagate_error (error, error_local);
        g_object_unref(file);
		return FALSE;
	}

	GDataOutputStream * output = g_data_output_stream_new (G_OUTPUT_STREAM (os));

    if (is_new)
    {
        g_data_output_stream_put_string(output, "# Copy this file into the recordings directory to create a new patch.\n", NULL, &error_local);
        g_data_output_stream_put_string(output, "# Edit the name for the patch, the room, the group and the tags.\n", NULL, &error_local);
        g_data_output_stream_put_string(output, "# Or append it to an existing patch using the following command:\n", NULL, &error_local);
        g_data_output_stream_put_string(output, "# cat ", NULL, &error_local);
        g_data_output_stream_put_string(output, fullpath, NULL, &error_local);
        g_data_output_stream_put_string(output, " >> recordings/SomeExistingPatchName.jsonl\n", NULL, &error_local);

        char header[128];
        snprintf(header, sizeof(header), "{\"patch\":\"%s\",\"room\":\"%s\",\"group\":\"House\",\"tags\":\"tags\"}\n\n", device_name, device_name);
        g_data_output_stream_put_string(output, header, NULL, &error_local);
    }

    struct recording r;
    for (int i = 0; i < N_ACCESS_POINTS; i++)
    {
        r.access_point_distances[i] = access_distances[i];
    }
    g_utf8_strncpy(r.patch_name, location, META_LENGTH);

    char* buffer = recording_to_json(&r, access_points);
    if (buffer != NULL)
    {
        // time
        time_t rawtime;
        time (&rawtime);
        char time_comment[64];
        strftime(time_comment, sizeof(time_comment), "# time %Y-%m-%dT%H:%M:%S\n", localtime(&rawtime));

        gssize written = 0;
        written +=  g_data_output_stream_put_string(output, time_comment, NULL, &error_local);
        written +=  g_data_output_stream_put_string(output, buffer, NULL, &error_local);
        written +=  g_data_output_stream_put_string(output, "\n", NULL, &error_local);

        free(buffer);
    }

    g_output_stream_close(G_OUTPUT_STREAM (os), NULL, &error_local);      // handles flush
    g_object_unref(output);
    g_object_unref(os);
    g_object_unref(file);

    if (is_new)
    {
        if (chmod(fullpath, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH) < 0)
        {
            g_warning("Could not CHMOD file '%s'", fullpath);
        }
    }

    return TRUE;
}
