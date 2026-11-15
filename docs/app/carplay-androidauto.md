# CarPlay / Android Auto

The phone is shown inside the UI (video, audio and touch) through a **Carlinkit dongle** plugged into
a USB port. No keys, no Python service: the main program does the work.

**Requirements:** Carlinkit dongle, `libusb-1.0-0`. More details
in [Hardware](../hardware.md#carplay--android-auto-dongle).

## How to connect

1. Install USB permissions once (the source installer does this automatically):
   ```bash
   sudo install -m 644 packaging/51-volta-carlinkit.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules
   ```
   Unplug and reconnect the dongle afterwards. This grants access to the active local desktop
   user for Carlinkit USB IDs `1314:1520` and `1314:1521`; do not run Volta OS as root.
2. Plug the dongle into a USB port of the PC and choose **Dongle** as the connection source.
3. For a **CPC200-CMAP**, connect the iPhone with a USB data cable to the dongle's phone port and
   accept CarPlay on the iPhone. Wireless-capable adapters can instead pair over Bluetooth.
4. Open the **CarPlay / Android Auto** app from the drawer.

### CPC200-CMAP (wired CarPlay)

Tested on Ubuntu 26.04 with a CPC200-CMAP reporting USB `1314:1520`, firmware
`2020.08.07.1519CCY`, bulk IN `0x81` and bulk OUT `0x01`. The adapter uses the existing
AutoKit packet protocol; no firmware update is needed. An iPhone connected through the
adapter was confirmed working. Other firmware and Android Auto on this specific model
have not been verified.

If **USB access denied** appears, install the rule above and reconnect. If the adapter is
busy, close other CarPlay/AutoKit applications. The terminal log includes `[Carlink]` USB
identity, endpoints, firmware, display handshake, phone connection and first-video-frame messages.

If the host is restarted while the iPhone is still connected, the adapter may resume video
without another phone-connected notification. Volta OS requests a fresh keyframe whenever
incoming video lacks decoder parameters, and repeats the request every two seconds until
decoding starts. It also resynchronizes after decoding errors. Dependent slices are
held back until the parameter sets and a keyframe are available, avoiding a repeated
FFmpeg `non-existing PPS` / `no frame` loop during startup.

The states you see: *Starting the dongle…* → *Connect your phone* → *Connecting to …* → the phone's video.
If the dongle is not connected, *Dongle not available · Plug in the USB dongle* appears.

## Small panel and full screen

The app opens in the right-hand panel and can go **full screen**. The phone's video is always
16:9: in the small panel and in full screen you can choose how to fit it (see below).

## Settings

In [Settings › CarPlay / Android Auto](../settings/carplay-android-auto.md): theme, fit
(*Automatic* or *Fill*), resolution (480p / 720p / 1080p), density (DPI) and **Reconnect the phone**.
Resolution and DPI apply on reconnection, which starts by itself when you change them.

## Android Auto without a dongle

Volta OS also speaks Android Auto by itself, so an **Android** phone can connect with no dongle at all
(CarPlay still needs the Carlinkit dongle). Choose it in
[Settings › CarPlay / Android Auto › Connection](../settings/carplay-android-auto.md). The video, audio,
microphone and touch use the same screen and settings as the dongle.

### With a USB cable

1. Install the **Android Auto** app on the phone (it is built in on recent Android versions).
2. Install the USB permission rule once, so Volta OS may talk to the phone:
   ```bash
   sudo cp packaging/51-volta-android-auto.rules /etc/udev/rules.d/
   sudo udevadm control --reload && sudo udevadm trigger
   ```
   (with the AppImage, take the file from the repository). Then unplug and plug the phone again. If it
   still says *Cannot reach the phone*, use a wider rule: `SUBSYSTEM=="usb", TAG+="uaccess"`.
3. Plug the phone into a USB port with a **data** cable and open the app. The phone switches itself to
   accessory mode and the video appears after a few seconds.

**USB tethering and this mode use the same cable**: when the phone switches to Android Auto the USB
tethering (internet through the phone) drops. If your computer gets its internet that way, expect to
lose it while Android Auto is running.

### Over Wi-Fi

The protocol side is ready: Volta OS waits for the phone on TCP port 5288. What is **not done yet** is
the step that makes the phone connect by itself (the Bluetooth handshake that hands it the Wi-Fi name and
password, and the Wi-Fi network created by the computer), so for now this mode does not connect on its
own. It will need Bluetooth and a Wi-Fi adapter that can act as an access point.

### The certificate

Android Auto requires a head unit certificate. Volta OS includes the same public one used by the
open source head units (see `third_party/aap_cert/README.md`); if Google revokes it, put a replacement
`headunit.crt` and `headunit.key` in `~/.local/share/volta_os/`.

### If something does not work

Start Volta OS from a terminal: lines starting with `[AA` tell where it stops (`VOLTA_AA_TRACE=1` prints every
message). Attach them to your [issue](https://github.com/Omardigio/VoltaOS/issues).
