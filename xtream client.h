#pragma once
#include <stddef.h>

// ---------------------------------------------------------------------------
// Xtream Codes API client
//
// An Xtream "portal" is defined by three things the user provides:
//   - server_url  e.g. "http://portal.example.com:8080"
//   - username
//   - password
//
// The panel exposes a PHP endpoint (player_api.php) that returns JSON.
// Typical calls:
//   player_api.php?username=U&password=P                          -> account info
//   player_api.php?username=U&password=P&action=get_live_categories
//   player_api.php?username=U&password=P&action=get_live_streams[&category_id=N]
//   player_api.php?username=U&password=P&action=get_vod_categories
//   player_api.php?username=U&password=P&action=get_vod_streams
//
// A playable live stream URL is built as:
//   {server_url}/live/{username}/{password}/{stream_id}.{ext}   (ext usually "ts" or "m3u8")
// ---------------------------------------------------------------------------

typedef struct {
    char server_url[256];   // e.g. "http://portal.example.com:8080" (no trailing slash)
    char username[128];
    char password[128];
} XtreamConfig;

typedef struct {
    int  stream_id;
    char name[256];
    char category_id[32];
    char stream_icon[512];
} XtreamChannel;

typedef struct {
    char category_id[32];
    char category_name[128];
} XtreamCategory;

typedef struct {
    XtreamCategory *items;
    size_t count;
} XtreamCategoryList;

typedef struct {
    XtreamChannel *items;
    size_t count;
} XtreamChannelList;

// Must be called once at startup (initializes curl); xtream_shutdown() at exit.
int  xtream_init(void);
void xtream_shutdown(void);

// Verifies credentials against {server_url}/player_api.php. Returns 0 on success.
// out_status_msg (optional) receives a human-readable error if it fails.
int xtream_authenticate(const XtreamConfig *cfg, char *out_status_msg, size_t out_status_msg_len);

// Fetches live TV categories. Caller must free with xtream_free_categories().
int xtream_get_live_categories(const XtreamConfig *cfg, XtreamCategoryList *out);
void xtream_free_categories(XtreamCategoryList *list);

// Fetches live channels, optionally filtered by category_id (pass NULL for all).
// Caller must free with xtream_free_channels().
int xtream_get_live_streams(const XtreamConfig *cfg, const char *category_id, XtreamChannelList *out);
void xtream_free_channels(XtreamChannelList *list);

// Builds a playable stream URL for a given channel into out_url (size out_url_len).
// ext is typically "ts" (raw MPEG-TS, best for a custom decoder) or "m3u8".
void xtream_build_stream_url(const XtreamConfig *cfg, int stream_id, const char *ext,
                              char *out_url, size_t out_url_len);
