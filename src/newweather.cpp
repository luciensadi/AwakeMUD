#include <curl/curl.h>

#include "nlohmann/json.hpp"
using nlohmann::json;

#include "types.hpp"
#include "structs.hpp"
#include "comm.hpp"
#include "perfmon.hpp"

// NOTE: You must define this yourself in a NON-GIT-TRACKED file in this directory.
// You can safely tack it on to mysql_config.cpp, provided that you re-add it if you ever regenerate your DB creds.
extern const char *weather_api_key;

// Yes, this file uses globals for threaded calls. No, I don't want to hear about it. YOU refactor it.
CURLM *weather_curl_multi_handle;

struct weather_response_info {
  // Query data
  size_t size;
  char *query_response_data;

  // Curl data
  CURL *easy_handle;

  // Query string
  const char *query_string;

  weather_response_info(): size(0), query_response_data(NULL), easy_handle(NULL), query_string("Seattle") {}
};

#define NUM_WEATHER_ZONES 1

struct weather_response_info weather_responses[NUM_WEATHER_ZONES];

// TODO: Convert below functions to use the above weather_responses[] array rather than the hardcoded field. This lets Vancouver etc have separate weather.

size_t write_weather_data(void *ptr, size_t size, size_t nmemb, struct weather_response_info *data) {
  size_t index = data->size;
  size_t n = (size * nmemb);
  char* tmp;

  mudlog_vfprintf(NULL, LOG_SYSLOG, "write_weather_data: Increasing size from %ld to %ld", data->size, data->size + (size * nmemb));
  data->size += (size * nmemb);

  tmp = (char *) realloc(data->query_response_data, data->size + 1); /* +1 for '\0' */

  if (tmp) {
    data->query_response_data = tmp;
  } else {
    if (data->query_response_data) {
      free(data->query_response_data);
    }
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Failed to allocate memory in write_weather_data for query '%s'.", data->query_string);
    return 0;
  }

  memcpy((data->query_response_data + index), ptr, n);
  data->query_response_data[data->size] = '\0';

  return size * nmemb;
}

void send_async_weather_request() {
  PERF_PROF_SCOPE(pr_, __func__);
  int running_handles;

  // Destroy our existing multi-handle (if applicable) and create a new one.
  if (weather_curl_multi_handle) {
    curl_multi_cleanup(weather_curl_multi_handle);
  }
  weather_curl_multi_handle = curl_multi_init();

  if (!weather_curl_multi_handle) {
    mudlog("ERROR: Could not create curl multi handle-- weather system disabled!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // For each known zone:
  for (int idx = 0; idx < NUM_WEATHER_ZONES; idx++) {
    // Blow away our old data and instantiate new data.
    if (weather_responses[idx].query_response_data)
      free(weather_responses[idx].query_response_data);
    weather_responses[idx].size = 0;
    weather_responses[idx].query_response_data = (char *) malloc(4096);

    if(!weather_responses[idx].query_response_data) {
      mudlog("Failed to allocate memory for weather system response!", NULL, LOG_SYSLOG, TRUE);
      return;
    }

    weather_responses[idx].query_response_data[0] = '\0';

    // Compose our URL string.
    char url_string[1000];
    snprintf(url_string, sizeof(url_string), "https://api.weatherapi.com/v1/current.json?key=%s&q=%s", weather_api_key, weather_responses[idx].query_string);

    // Set up our API connection.
    weather_responses[idx].easy_handle = curl_easy_init();
    curl_easy_setopt(weather_responses[idx].easy_handle, CURLOPT_URL, url_string);
    curl_easy_setopt(weather_responses[idx].easy_handle, CURLOPT_WRITEFUNCTION, write_weather_data);
    curl_easy_setopt(weather_responses[idx].easy_handle, CURLOPT_WRITEDATA, &(weather_responses[idx]));

    // Add the request to our async handler.
    curl_multi_add_handle(weather_curl_multi_handle, weather_responses[idx].easy_handle);
  }

  // Send it all.
  curl_multi_perform(weather_curl_multi_handle, &running_handles);
}

void read_async_weather_request() {
  PERF_PROF_SCOPE(pr_, __func__);
  int running_handles, msgs_left;

  if (!weather_curl_multi_handle) {
    mudlog("SYSERR: Out-of-order call: read_async_weather_request() called with NULL weather_curl_multi_handle!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  CURLMcode mc = curl_multi_perform(weather_curl_multi_handle, &running_handles);

  if (mc || running_handles == 0) {
    // Read the results.
    CURLMsg *msg = curl_multi_info_read(weather_curl_multi_handle, &msgs_left);

    if (msg->msg == CURLMSG_DONE) {
      // Data has been fully generated for one of our weather zones. Parse it.
      // First, iterate through all the known handles for our weather zones and find the one that matches this one.
      // TODO: Save the handle for each zone so we can do this.
      for (int idx = 0; idx < NUM_WEATHER_ZONES; idx++) {
        if (msg->easy_handle == weather_responses[idx].easy_handle) {
          // Once we've found a match, parse the weather data for that zone.
          mudlog_vfprintf(NULL, LOG_SYSLOG, "Got API response: %s", weather_responses[idx].query_response_data);
          json weather_json = json::parse(weather_responses[idx].query_response_data);
          mudlog_vfprintf(NULL, LOG_SYSLOG, "Current temperature for query %s: %0.1f", weather_responses[idx].query_string, weather_json["current"]["temp_f"].get<float>());
          break;
        }
      }
    } else {
      log_vfprintf("Got unexpected curlmsg state %d from weather request.", msg->msg);
    }

    // Regardless, we need to remove the handle now.
    curl_multi_remove_handle(weather_curl_multi_handle, msg->easy_handle);
    curl_easy_cleanup(msg->easy_handle);

    // And since we know there's only one request in our multi-handle, we can clean that up now too.
    curl_multi_cleanup(weather_curl_multi_handle);
    weather_curl_multi_handle = NULL;
  }
}
