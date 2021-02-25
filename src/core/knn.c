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
        if (distance > 0 && distance < EFFECTIVE_INFINITE)
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
            g_warning("Line in error is `%s`", buffer);
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
                ralloc->access_point_distances[ap->id]=EFFECTIVE_INFINITE;
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

float get_probability (struct recording* recording,
    float accessdistances[N_ACCESS_POINTS],
    float accesstimes[N_ACCESS_POINTS], 
    struct AccessPoint* access_points, bool debug)
{
    if (access_points->next == NULL) 
    {
        float sum_delta_squared = 0.0;
        // single access point - one over distance squared for probability
        // and no need to take time into account
        struct AccessPoint* ap = access_points;
        float recording_distance = recording->access_point_distances[ap->id];
        float measured_distance = accessdistances[ap->id];
        float delta = (recording_distance - measured_distance);
        sum_delta_squared += delta*delta;
        // 0.5m = 1
        // 1m = 1
        // 2m = 0.4
        // 10m = 0.01
        return fmin(1.0, 2.0 / (sum_delta_squared + 1.0));
    } 
    else 
    {
        double probability = 1.0;

        // Rebase all times to since the latest observation
        // We want to know where the device WAS even if it has since left
        double min_delta = 30.0;
        for (struct AccessPoint* ap = access_points; ap != NULL; ap=ap->next)
        {
            if (accesstimes[ap->id] < min_delta) min_delta = accesstimes[ap->id];
        }

        for (struct AccessPoint* ap = access_points; ap != NULL; ap=ap->next)
        {
            float recording_distance = recording->access_point_distances[ap->id];
            float measured_distance = accessdistances[ap->id];
            float deltatime = accesstimes[ap->id] - min_delta;

            // 500s later could have moved a long way
            // first 30s are 0 as that's typical a gap in transmissions
            float p_gone_away = deltatime < 30 ? 0.0 : atan((deltatime-30)/100)/3.14159*2;

            if (recording_distance >= EFFECTIVE_INFINITE_TEST && measured_distance >= EFFECTIVE_INFINITE_TEST)
            {
                // OK: did not expect this distance to be here, and it's not but less information than a match
                // but better than a bad match on distances, e.g. 4m and 14m
                probability = probability * 0.8;
                //if (debug) g_debug("%s neither x 0.99", ap->client_id);
            }
            else if (recording_distance >= EFFECTIVE_INFINITE_TEST)
            {
                probability = probability * p_gone_away;
                // the observation could see the AP but this recording says you cannot
                // e.g. barn says you cannot see study, so if you can see study you can't be here
                if (debug) g_debug("%s was not expected, but %.2fm found x 0.2", ap->client_id, measured_distance);
            }
            else if (measured_distance >= EFFECTIVE_INFINITE_TEST)
            {
               probability = probability * 0.30;  // there is no time for a reading we don't have!
                if (debug) g_debug("%s was expected not found, expected at %.2f x 0.4", ap->client_id, recording_distance);
               // could not see an AP at all, but should have been able to, could just be a missing observation
            }
            else
            {
                // activation function, and max 0.8 for a reading with the same ap
                float delta = fabs(measured_distance - recording_distance);
                //float delta_squared = (delta * delta);
                // activation function - level until 2m and then drops off 1/x curve
                float p_in_range = delta < 2 ? 1.0 : 1.0 / (delta-1.0);

                // The more likely you are to be here, the more signficant it is if the distance is a miss
                // Either you have left this place or you are in range for this reading to be useful
                double prob = p_in_range + p_gone_away - (p_in_range * p_gone_away);
                probability = probability * prob;
                if (debug) g_debug("%s was expected and found %.2fm delta, x %.3f", ap->client_id, delta, prob);
            }
        }

        return probability;
    }
}


/*
  k_nearest classifier
  Like KNeighborsClassifier(weights='distance')

  parameters
    top_k_list

  NB You must free() the returned string

*/
int k_nearest(struct recording* recordings,
            float accessdistances[N_ACCESS_POINTS],
            float accesstimes[N_ACCESS_POINTS], 
            struct AccessPoint* access_points,
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

        bool debug2 = strcmp(recording->patch_name, "Gravel") == 0 || strcmp(recording->patch_name, "Garage") == 0; 

        // Insert recording into the list if it's a better match
        float distance = get_probability(recording, accessdistances, accesstimes, access_points, debug && debug2);

        struct top_k current;
        g_utf8_strncpy(current.patch_name, recording->patch_name, META_LENGTH);
        current.distance = distance;
        current.used = FALSE;
        //current.patch = recording;

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
            else if (result[i].distance < current.distance)
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

    // It must be somewhere so need to normalize the probabilities found to add to 1?

    // Find the most common in the top_k weighted by distance
    for (int i = 0; i < k; i++)
    {
        if (result[i].used) continue;

        double cumulative_probability = result[i].distance;
        double min_prob = result[i].distance;
        double max_prob = result[i].distance;
        int tc = 1;

        if (debug) { g_debug("%s   %.3f -> %.3f", result[i].patch_name, result[i].distance, cumulative_probability);}

        for (int j = i+1; j < k; j++)
        {
            if (strcmp(result[i].patch_name, result[j].patch_name) == 0)
            {
                tc++;
                result[j].used = TRUE;

                if (tc < 4)  // only allow top three readings to influence score
                {
                    // p(A or B) = p(A) + p(B) - p(A & B)
                    cumulative_probability = cumulative_probability + result[j].distance
                        - cumulative_probability * result[j].distance;

                    min_prob = fmin(min_prob, result[j].distance);
                    max_prob = fmax(max_prob, result[j].distance);

                    if (debug) { g_debug("%s + %.3f -> %.3f", result[j].patch_name, result[j].distance, cumulative_probability);}
                }
            }
        }

        if (debug) g_debug("Patch '%s' has probability %.3f over %i (%.3f-%.3f)", result[i].patch_name,
           cumulative_probability, tc, min_prob, max_prob);

        struct top_k current_value = result[i];  // copy name
        current_value.distance = cumulative_probability;

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
            else if (top_result[m].distance < current_value.distance)
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
bool record (const char* directory, const char* device_name, float access_distances[N_ACCESS_POINTS], struct AccessPoint* access_points, char* location)
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

/*
*  Compare two distances using heuristic with cut off and no match handling
*/
double score_one_pair(float a_distance, float b_distance, time_t a_time, time_t b_time)
{
    double delta_time = fabs(difftime(a_time, b_time));
    if (a_distance >= EFFECTIVE_INFINITE_TEST && b_distance >= EFFECTIVE_INFINITE_TEST)
    {
        return 1.00;
    }
    else if (a_distance >= EFFECTIVE_INFINITE_TEST)
    {
        // We have a B but no A.  Maybe A is too new and we haven't seen same AP yet.
        // If B is really old we may have moved away from it.
        if (delta_time > 300) return 1.0;
        else if (delta_time > 200) return 0.9;
        else return 0.8;
    }
    else if (b_distance >= EFFECTIVE_INFINITE_TEST)
    {
        // We have an A but no B, maybe we moved but less likely than prior case
        // we should have seen the same AP if we switched mac address
        return 0.50;
    }
    else
    {
        float delta = fabs(a_distance - b_distance);
        double prob = 1.0 - fmin(0.8, 0.1 * fmin(delta, 8));

        // The closer these are in time, the more likely they are to be related
        if (delta_time < 30)
        {
            prob = (prob + prob) - prob * prob;
        }

        return prob;
    }
}

/*
*  Compare two closest values
*/
float compare_closest (struct ClosestHead* a, struct ClosestHead* b, struct OverallState* state)
{
    double probability = 1.0;
    bool at_least_one = false;

    // Find a representative start time for comparison with earlier ranges that don't have a match
    // Use median start time
    time_t ordered_start_times[N_ACCESS_POINTS];
    int n = 0;
    for (struct ClosestTo* c = a->closest; c != NULL; c=c->next)
    {
        int insertion_point = 0;
        for (int i = 0; i < n; i++)
        {
            insertion_point = i;
            if (c->earliest < ordered_start_times[i]) break;
        }
        if (insertion_point < n)
        {
            for (int k = n; k > insertion_point; k--)
            {
                ordered_start_times[k] = ordered_start_times[k-1];
            }
        }
        ordered_start_times[insertion_point] = c->earliest;
    }

    time_t median_start_time = ordered_start_times[n / 2];

    for (struct AccessPoint* ap = state->access_points; ap != NULL; ap=ap->next)
    {
        float a_distance = EFFECTIVE_INFINITE;
        float b_distance = EFFECTIVE_INFINITE;
        time_t a_time = median_start_time;
        time_t b_time = time(0);

        // a is later than b

        for (struct ClosestTo* c = a->closest; c != NULL; c = c->next)
        {
            if (c->access_point->id == ap->id) 
            { 
                a_distance = c->distance; at_least_one = TRUE;
                a_time = c->earliest;
            }
        }

        for (struct ClosestTo* c = b->closest; c != NULL; c = c->next)
        {
            if (c->access_point->id == ap->id) 
            { 
                b_distance = c->distance; at_least_one = TRUE; 
                b_time = c->latest;
            }
        }

        double pair_score = score_one_pair(a_distance, b_distance, a_time, b_time);

        probability = probability * pair_score;
    }
    return at_least_one ? probability : 0.0;
}
