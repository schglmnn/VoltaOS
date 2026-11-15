# Spotify

Shows your playlists, library and Spotify's recommendations. **Playback** goes through
**spotifyd** (Spotify Connect): the UI becomes a Spotify device on your account.

**Requirements:** Spotify account (a free one is fine for browsing; to play via Connect a
Premium one is generally needed), `spotifyd` installed, `SPOTIFY_CLIENT_ID` and `SPOTIFY_CLIENT_SECRET`
([how to get them](../api-keys.md#spotify-step-by-step)). Service `spotify_scraper_service.py` (port 8098).

## Signing in

Tap **Sign in to Spotify** and sign in from the panel inside the UI. **Sign out** in the menu to disconnect.

The Web API login loads the library and controls playback; the local audio receiver needs
its own Spotify session. Authenticate it once with `spotifyd authenticate` and confirm
the browser page. Restart Volta OS afterwards. Alternatively, select **Volta OS (spotifyd)**
from the device picker in the official Spotify app on the same network. Volta OS starts
this receiver with the PulseAudio backend (also works with PipeWire's PulseAudio server).

## Screens

- **Home** — playlists and "Made for you" collections, new releases and recommendations.
- **Explore** — fixed categories (*New releases, Dance / Electronic, Charts, Discover, In the car,
  Trending, Workout music, Wellness, Kids and family…*); the content of each
  category is fetched on the fly, with automatic retries if the service is slow. Card covers
  update by themselves.
- **Library** — saved **Playlists, Tracks, Albums, Artists, Podcasts**.
- **Detail** — album, playlist and artist: tracks, *See all*, top tracks, singles.

> Spotify removed the category list from its official API (February 2026): that is why the list
> in *Explore* is fixed in the app.

## Playback, queue and search

As in YT Music: **heart** for favorite tracks, **synced lyrics**, **unified queue**,
**unified search**, shuffle and repeat (even without login they go through MPRIS). The logo menu at the top switches
to the other music apps. See [YT Music](ytmusic.md) and [Interface](../interface.md#unified-search-and-queue).

## If it does not play

- If the receiver log says `error audio key 0 1`, Spotify refused librespot's
  audio-key request. Changing volume or the output device does not fix that.
  Use the official [Spotify Soloist](https://developer.spotify.com/documentation/soloist)
  receiver instead: obtain your own Soloist key, download its official Linux x86_64
  executable into `.runtime/soloist/soloist`, and set `SPOTIFY_RECEIVER=soloist` and
  `SPOTIFY_SOLOIST_API_KEY=...` in your private `keys.env`. Restart Volta OS, then
  select **Volta OS (soloist)** in the official Spotify app on the same network once.
  The stored session is kept in `~/.local/share/volta_soloist`. Never commit the key
  or redistribute the downloaded binary. Update expired Soloist builds from Spotify.
- Check that `spotifyd` is installed: the UI starts it by itself.
- If no local receiver is connected, Volta OS reports that instead of pretending to play.
  Complete the receiver login above. Playback targets the local `spotifyd` receiver;
  it does not fall back to an unrelated active phone or speaker.
- If the library does not load, sign in again and check the Spotify app's Redirect URIs.

## Location-based discovery

With a valid GPS fix, Home charts, new releases and Explore searches follow the
country returned by the map service's existing MapTiler reverse geocoder. Location
lookups are limited to once per minute. Personal favourites and account settings
are unchanged. Without a valid fix or a successful lookup, the last country is
retained in local settings; the initial fallback is Germany. Simulated GPS fixes
use the same path. This changes discovery queries, not Spotify account territory.
