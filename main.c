// Switch IPTV - Xtream Codes client
//
// This app handles: credential entry (via the on-screen keyboard), talking to
// the Xtream player_api.php endpoint, and browsing categories/channels with
// the text console + d-pad.
//
// -----------------------------------------------------------------------
// IMPORTANT - what this app does NOT do yet: play video.
//
// Selecting a channel currently just prints the resolved stream URL
// (e.g. http://server:port/live/user/pass/12345.ts). To actually play it
// you need a decode+render pipeline, which is a separate, substantial
// project on its own:
//
//   1. Cross-compile FFmpeg for libnx (devkitPro has community pkgbuilds
//      for this, or build libavformat/libavcodec yourself against the
//      aarch64-none-elf toolchain).
//   2. Use FFmpeg to open the stream URL, demux the MPEG-TS, and decode
//      H.264/HEVC frames (the Switch's Tegra X1 supports hardware decode
//      via `nvdec`, but wiring that up from libnx is nontrivial - most
//      homebrew players fall back to software decode via FFmpeg, which
//      is CPU-heavy but workable for SD/HD streams).
//   3. Push decoded YUV/RGB frames to the screen - either via SDL2
//      (`switch-sdl2` portlib, simplest) or directly via deko3d/nvn.
//   4. Handle audio: decode via FFmpeg, output via libnx's `audout` or
//      SDL2 audio.
//
// I structured xtream_client.c/h so the URL-fetching and playback pieces
// are cleanly separated - `play_stream_stub()` below is exactly where
// you'd hand the URL off to that pipeline once it exists.
// -----------------------------------------------------------------------

#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xtream_client.h"

// Simple on-screen software keyboard wrapper. Returns 0 on success.
static int prompt_text(const char *header, char *out, size_t out_len, bool is_password) {
    SwkbdConfig kbd;
    Result rc = swkbdCreate(&kbd, 0);
    if (R_FAILED(rc)) return -1;

    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, header);
    swkbdConfigSetStringLenMax(&kbd, (int)out_len - 1);
    if (is_password) {
        swkbdConfigMakePresetPassword(&kbd); // masks input, keeps full qwerty keyboard
    }

    rc = swkbdShow(&kbd, out, out_len);
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) ? 0 : -1;
}

static void print_header(void) {
    printf("\x1b[2J\x1b[H"); // clear screen, home cursor
    printf("=== Switch IPTV (Xtream Codes client) ===\n\n");
}

// Placeholder for the playback pipeline described in the file header.
static void play_stream_stub(const char *url) {
    print_header();
    printf("Would play stream:\n  %s\n\n", url);
    printf("Playback isn't implemented yet - see the TODO block at the top\n");
    printf("of main.c for what's needed (FFmpeg decode + SDL2/deko3d render).\n\n");
    printf("Press B to go back.\n");

    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_B) break;
        consoleUpdate(NULL);
    }
}

static void browse_channels(const XtreamConfig *cfg, const char *category_id) {
    XtreamChannelList channels;
    print_header();
    printf("Loading channels...\n");
    consoleUpdate(NULL);

    if (xtream_get_live_streams(cfg, category_id, &channels) != 0) {
        printf("Failed to load channels. Press B to go back.\n");
        consoleUpdate(NULL);
        PadState pad;
        padInitializeDefault(&pad);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_B) break;
            consoleUpdate(NULL);
        }
        return;
    }

    size_t selected = 0;
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        print_header();
        printf("Channels (%zu) - A: play, B: back, dpad: navigate\n\n", channels.count);

        // Show a window of ~15 items around the selection so long lists scroll.
        size_t window = 15;
        size_t start = (selected >= window / 2) ? selected - window / 2 : 0;
        size_t end = start + window;
        if (end > channels.count) end = channels.count;

        for (size_t i = start; i < end; i++) {
            printf("%s %s\n", (i == selected) ? ">" : " ", channels.items[i].name);
        }

        consoleUpdate(NULL);

        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Down && selected + 1 < channels.count) selected++;
        if (kDown & HidNpadButton_Up && selected > 0) selected--;
        if (kDown & HidNpadButton_B) break;
        if (kDown & HidNpadButton_A && channels.count > 0) {
            char url[1024];
            xtream_build_stream_url(cfg, channels.items[selected].stream_id, "ts", url, sizeof(url));
            play_stream_stub(url);
        }
    }

    xtream_free_channels(&channels);
}

static void browse_categories(const XtreamConfig *cfg) {
    XtreamCategoryList cats;
    print_header();
    printf("Loading categories...\n");
    consoleUpdate(NULL);

    if (xtream_get_live_categories(cfg, &cats) != 0) {
        printf("Failed to load categories. Press B to exit.\n");
        consoleUpdate(NULL);
        PadState pad;
        padInitializeDefault(&pad);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_B) break;
            consoleUpdate(NULL);
        }
        return;
    }

    size_t selected = 0;
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        print_header();
        printf("Categories (%zu) - A: open, B: quit, dpad: navigate\n\n", cats.count);

        size_t window = 15;
        size_t start = (selected >= window / 2) ? selected - window / 2 : 0;
        size_t end = start + window;
        if (end > cats.count) end = cats.count;

        for (size_t i = start; i < end; i++) {
            printf("%s %s\n", (i == selected) ? ">" : " ", cats.items[i].category_name);
        }

        consoleUpdate(NULL);

        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Down && selected + 1 < cats.count) selected++;
        if (kDown & HidNpadButton_Up && selected > 0) selected--;
        if (kDown & HidNpadButton_B) break;
        if (kDown & HidNpadButton_A && cats.count > 0) {
            browse_channels(cfg, cats.items[selected].category_id);
        }
    }

    xtream_free_categories(&cats);
}

int main(int argc, char *argv[]) {
    consoleInit(NULL);

    PadState pad;
    padInitializeDefault(&pad);

    if (xtream_init() != 0) {
        printf("Failed to init network stack.\n");
        consoleUpdate(NULL);
        goto wait_exit;
    }

    XtreamConfig cfg = {0};

    print_header();
    printf("Enter your Xtream portal details.\n");
    printf("(Press A to bring up the keyboard for each field.)\n\n");
    consoleUpdate(NULL);

    prompt_text("Server URL (e.g. http://host:port)", cfg.server_url, sizeof(cfg.server_url), false);
    prompt_text("Username", cfg.username, sizeof(cfg.username), false);
    prompt_text("Password", cfg.password, sizeof(cfg.password), true);

    print_header();
    printf("Authenticating with %s ...\n", cfg.server_url);
    consoleUpdate(NULL);

    char status[256] = {0};
    if (xtream_authenticate(&cfg, status, sizeof(status)) != 0) {
        print_header();
        printf("Login failed: %s\n\n", status);
        printf("Press B to exit.\n");
        consoleUpdate(NULL);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_B) break;
            consoleUpdate(NULL);
        }
        goto cleanup;
    }

    browse_categories(&cfg);

cleanup:
    xtream_shutdown();
wait_exit:
    consoleExit(NULL);
    return 0;
}
