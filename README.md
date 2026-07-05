# Switch IPTV — Xtream Codes client (homebrew)

A homebrew Nintendo Switch app that logs into an Xtream Codes IPTV portal,
lists live TV categories/channels, and resolves playable stream URLs.

**Status: menu/API layer works. Video playback is NOT implemented** — see
"Adding playback" below. Selecting a channel currently just prints the
resolved URL instead of playing it.

## What you need

- A Switch with **Atmosphère** (or another CFW) and homebrew launching set up
- **devkitPro** with the `switch-dev` pacman group installed
  (https://devkitpro.org/wiki/Getting_Started)
- These portlibs:
  ```
  sudo dkp-pacman -S switch-curl switch-mbedtls switch-json-c switch-zlib
  ```
- Legitimate access to an Xtream Codes portal — your own media server, or a
  provider you actually have a license/subscription with. This app is a
  generic Xtream API client; it doesn't include or point at any content.

## Build

```bash
export DEVKITPRO=/opt/devkitpro   # wherever you installed it
make
```

This produces `switch-iptv.nro`.

## Install

Copy `switch-iptv.nro` to `/switch/switch-iptv/switch-iptv.nro` on your SD
card, then launch it from the Homebrew Menu (hbmenu).

## Usage

1. Launch the app.
2. Enter your portal's **server URL** (e.g. `http://portal.example.com:8080`),
   **username**, and **password** using the on-screen keyboard.
3. Browse categories → channels with the d-pad, `A` to select, `B` to go back.
4. Selecting a channel prints the resolved stream URL (see status above).

## Project layout

```
switch-iptv/
├── Makefile              devkitPro/libnx build rules
├── source/
│   ├── main.c             console UI: login flow, category/channel browser
│   ├── xtream_client.c    Xtream Codes API client (auth, categories, streams)
│   └── xtream_client.h
└── romfs/                 (empty — add fonts/assets here if you add SDL2 UI)
```

## How the Xtream API client works

`xtream_client.c` wraps calls to `{server_url}/player_api.php`:

| Action | Purpose |
|---|---|
| *(none)* | Auth check / account info |
| `get_live_categories` | List live TV categories |
| `get_live_streams` | List channels (optionally by `category_id`) |

A playable URL is built as:
```
{server_url}/live/{username}/{password}/{stream_id}.{ext}
```
(`ext` is usually `ts` for raw MPEG-TS or `m3u8` for HLS.)

VOD/series endpoints (`get_vod_categories`, `get_vod_streams`,
`get_series_categories`, etc.) follow the same pattern and aren't wired into
the UI yet, but `xtream_client.c` is structured so adding them is mostly
copy-pasting `xtream_get_live_streams` with the different action name.

## Building with only a phone (no PC)

This repo includes `.github/workflows/build.yml`, which compiles the app
in the cloud using devkitPro's official Docker image — you never need a
local toolchain.

### One-time setup

1. **Create a GitHub repo** from your phone: GitHub app (or
   github.com in a mobile browser) → "+" → New repository.
2. **Upload the project files** into it. Easiest ways on mobile:
   - GitHub app: repo → "Add file" → "Create new file" (paste content
     one file at a time), or
   - GitHub mobile web: same "Add file" flow, or
   - if you have Working Copy / a Git client on iOS, or Termux + git on
     Android, you can `git init && git add . && git commit && git push`
     the whole folder at once — much faster than pasting file by file.
3. Make sure the folder structure on GitHub matches this repo exactly:
   `Makefile`, `source/*.c`, `source/*.h`, `.github/workflows/build.yml`.

### Every time you want a build

1. Open the repo in the GitHub app → **Actions** tab.
2. Tap **"Build Switch IPTV (.nro)"** → **"Run workflow"** → confirm.
   (It also runs automatically whenever you push a change to `source/`
   or the `Makefile`.)
3. Wait for the run to go green (a minute or two).
4. Open the completed run → scroll to **Artifacts** → tap
   **`switch-iptv-nro`** to download it. On a phone this saves as a
   `.zip` — extract it (most file managers / "Files" on iOS can unzip
   in place) to get `switch-iptv.nro`.
5. Copy `switch-iptv.nro` onto your SD card at
   `/switch/switch-iptv/switch-iptv.nro` (via a USB-OTG card reader, or
   any phone app that can browse/write to the SD card).
6. Launch it from the Homebrew Menu on your Switch.

If a build fails, tap into the failed step in the Actions log — it's
plain compiler/linker output, same as you'd see locally, just read on
your phone instead of a terminal.



This is genuinely the bulk of the remaining work. Rough path:

1. **Cross-compile FFmpeg** for `aarch64-none-elf` / libnx. There are
   community devkitPro pkgbuilds for this (search `switch-ffmpeg` in the
   `switch-examples`/`libnx` ecosystem); building it yourself is also
   possible but fiddly (needs a matching `pkg-config` setup).
2. **Demux + decode**: open the stream URL with `avformat_open_input`,
   find the video/audio streams, decode with `avcodec`. The Tegra X1 in
   the Switch has hardware video decode, but there's no mature libnx
   binding for it yet — most homebrew players (e.g. some emulator
   front-ends) fall back to **software decode**, which works for
   SD/720p streams but will struggle with heavy 1080p bitrates.
3. **Render frames**: easiest path is the `switch-sdl2` portlib —
   convert decoded YUV frames to an `SDL_Texture` and blit each frame.
   (Lower-level alternative: deko3d/nvn directly, more work, more control.)
4. **Audio**: decode via FFmpeg, push PCM to libnx's `audout`/`audren`,
   or use SDL2's audio device if you're already using SDL2 for video.
5. **Sync**: keep audio/video roughly in sync using PTS timestamps from
   the decoded frames — a basic clock (e.g. audio-clock-driven) is enough
   to start.

If you want, I can scaffold step 1–3 next (an SDL2 render loop + FFmpeg
demux/decode skeleton) as a follow-up — it's a substantial enough chunk of
code that it's worth doing as its own pass rather than bolting on here.

## Legal note

This is a generic Xtream Codes protocol client, comparable to apps like
TiviMate or IPTV Smarters. Whether using it is fine depends entirely on
whether you have the rights to the content on the portal you point it at —
that's on you to make sure of.
