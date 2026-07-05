#include "xtream_client.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Growable buffer used to accumulate curl's response body in memory.
// ---------------------------------------------------------------------------
typedef struct {
    char   *data;
    size_t  size;
} MemBuf;

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemBuf *mem = (MemBuf *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0; // out of memory -> abort transfer

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    return realsize;
}

static int http_get(const char *url, MemBuf *out) {
    out->data = malloc(1);
    out->size = 0;
    if (!out->data) return -1;
    out->data[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) { free(out->data); out->data = NULL; return -1; }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)out);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SwitchIPTV/0.1");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(out->data);
        out->data = NULL;
        return -1;
    }
    if (http_code < 200 || http_code >= 300) {
        return -2; // request went through, but server returned an error status
    }
    return 0;
}

// Percent-encodes a string for safe use in a query parameter.
static void url_encode(const char *src, char *dst, size_t dst_len) {
    static const char *hex = "0123456789ABCDEF";
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 4 < dst_len; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[j++] = c;
        } else {
            dst[j++] = '%';
            dst[j++] = hex[c >> 4];
            dst[j++] = hex[c & 0xF];
        }
    }
    dst[j] = '\0';
}

int xtream_init(void) {
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : -1;
}

void xtream_shutdown(void) {
    curl_global_cleanup();
}

static void build_api_url(const XtreamConfig *cfg, const char *action,
                           const char *extra_params, char *out, size_t out_len) {
    char user_enc[256], pass_enc[256];
    url_encode(cfg->username, user_enc, sizeof(user_enc));
    url_encode(cfg->password, pass_enc, sizeof(pass_enc));

    if (action && action[0]) {
        snprintf(out, out_len, "%s/player_api.php?username=%s&password=%s&action=%s%s",
                 cfg->server_url, user_enc, pass_enc, action,
                 extra_params ? extra_params : "");
    } else {
        snprintf(out, out_len, "%s/player_api.php?username=%s&password=%s",
                 cfg->server_url, user_enc, pass_enc);
    }
}

int xtream_authenticate(const XtreamConfig *cfg, char *out_status_msg, size_t out_status_msg_len) {
    char url[1024];
    build_api_url(cfg, NULL, NULL, url, sizeof(url));

    MemBuf resp;
    int rc = http_get(url, &resp);
    if (rc != 0) {
        if (out_status_msg) snprintf(out_status_msg, out_status_msg_len,
            "Network error contacting portal (code %d). Check server URL / connection.", rc);
        return -1;
    }

    struct json_object *root = json_tokener_parse(resp.data);
    free(resp.data);
    if (!root) {
        if (out_status_msg) snprintf(out_status_msg, out_status_msg_len,
            "Portal did not return valid JSON. Check server URL.");
        return -1;
    }

    struct json_object *user_info = NULL;
    int ok = 0;
    if (json_object_object_get_ex(root, "user_info", &user_info)) {
        struct json_object *auth = NULL;
        if (json_object_object_get_ex(user_info, "auth", &auth)) {
            ok = json_object_get_int(auth) == 1;
        }
    }

    if (!ok && out_status_msg) {
        snprintf(out_status_msg, out_status_msg_len, "Authentication failed: check username/password.");
    }
    json_object_put(root);
    return ok ? 0 : -1;
}

int xtream_get_live_categories(const XtreamConfig *cfg, XtreamCategoryList *out) {
    out->items = NULL;
    out->count = 0;

    char url[1024];
    build_api_url(cfg, "get_live_categories", NULL, url, sizeof(url));

    MemBuf resp;
    if (http_get(url, &resp) != 0) return -1;

    struct json_object *root = json_tokener_parse(resp.data);
    free(resp.data);
    if (!root || json_object_get_type(root) != json_type_array) {
        if (root) json_object_put(root);
        return -1;
    }

    size_t n = json_object_array_length(root);
    out->items = calloc(n, sizeof(XtreamCategory));
    out->count = n;

    for (size_t i = 0; i < n; i++) {
        struct json_object *item = json_object_array_get_idx(root, i);
        struct json_object *field;

        if (json_object_object_get_ex(item, "category_id", &field))
            strncpy(out->items[i].category_id, json_object_get_string(field),
                    sizeof(out->items[i].category_id) - 1);

        if (json_object_object_get_ex(item, "category_name", &field))
            strncpy(out->items[i].category_name, json_object_get_string(field),
                    sizeof(out->items[i].category_name) - 1);
    }

    json_object_put(root);
    return 0;
}

void xtream_free_categories(XtreamCategoryList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

int xtream_get_live_streams(const XtreamConfig *cfg, const char *category_id, XtreamChannelList *out) {
    out->items = NULL;
    out->count = 0;

    char extra[64] = {0};
    if (category_id && category_id[0]) {
        snprintf(extra, sizeof(extra), "&category_id=%s", category_id);
    }

    char url[1024];
    build_api_url(cfg, "get_live_streams", extra, url, sizeof(url));

    MemBuf resp;
    if (http_get(url, &resp) != 0) return -1;

    struct json_object *root = json_tokener_parse(resp.data);
    free(resp.data);
    if (!root || json_object_get_type(root) != json_type_array) {
        if (root) json_object_put(root);
        return -1;
    }

    size_t n = json_object_array_length(root);
    out->items = calloc(n, sizeof(XtreamChannel));
    out->count = n;

    for (size_t i = 0; i < n; i++) {
        struct json_object *item = json_object_array_get_idx(root, i);
        struct json_object *field;

        if (json_object_object_get_ex(item, "stream_id", &field))
            out->items[i].stream_id = json_object_get_int(field);

        if (json_object_object_get_ex(item, "name", &field))
            strncpy(out->items[i].name, json_object_get_string(field),
                    sizeof(out->items[i].name) - 1);

        if (json_object_object_get_ex(item, "category_id", &field))
            strncpy(out->items[i].category_id, json_object_get_string(field),
                    sizeof(out->items[i].category_id) - 1);

        if (json_object_object_get_ex(item, "stream_icon", &field))
            strncpy(out->items[i].stream_icon, json_object_get_string(field),
                    sizeof(out->items[i].stream_icon) - 1);
    }

    json_object_put(root);
    return 0;
}

void xtream_free_channels(XtreamChannelList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

void xtream_build_stream_url(const XtreamConfig *cfg, int stream_id, const char *ext,
                              char *out_url, size_t out_url_len) {
    snprintf(out_url, out_url_len, "%s/live/%s/%s/%d.%s",
             cfg->server_url, cfg->username, cfg->password, stream_id, ext ? ext : "ts");
}
