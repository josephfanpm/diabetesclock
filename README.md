# Diabetes Clock

Firmware that turns an Ulanzi TC001 pixel clock into a glucose display. It reads from Dexcom Share, Nightscout, LibreLink Up or Medtrum EasyFollow.

**[Install and preview the clock faces](https://josephfanpm.github.io/diabetesclock/)**

### Current version: 1.1.2

## What it does

- Five clock faces: Value and delta, Weather, Glucose graph and value, Current time and BG value, and Time only
- Restarts itself when readings stop: if no new reading arrives for 15 minutes, the clock restarts and signs in to the data source again
- mg/dL and mmol/L
- Configurable low and high limits, with a color for each range
- Audible high, low and urgent-low alarms
- Automatic brightness
- Marks old readings with a configurable color, and a cross in place of the trend arrow
- Daily face and brightness schedules, or automatic cycling through your faces
- Offline settings page with backup and restore

## Install

1. Get an [Ulanzi TC001](https://www.ulanzi.com/products/ulanzi-pixel-smart-clock-2882) (about $50).
2. Turn it on: hold `<` and `>` for a few seconds.
3. Connect it to your computer with the USB-C cable.
4. Open the [install page](https://josephfanpm.github.io/diabetesclock/) in Chrome or Edge and follow the steps. If it fails, see the [troubleshooting guide](TROUBLESHOOTING.md#installation).
5. On your phone, join the `nsclock` Wi-Fi network and open `http://192.168.4.1/`.
6. Pick your Wi-Fi from the list of networks the clock can see (or type a hidden one), enter its password, your data source login and your glucose limits, then select **Save and restart**.

## Update

Updating reinstalls the firmware and resets the clock's settings. First download a settings file from **Backup and restore** on the **WiFi & system** tab. After updating, load it on the same tab, review the settings, and select **Save and restart**. Include network settings to restore your Wi-Fi; the web login has to be set again.

## Clock faces

Preview every face on the [install page](https://josephfanpm.github.io/diabetesclock/).

| Face | What it shows |
| ---- | ------------- |
| Value and delta | The value, the trend arrow, and the change since the previous reading. This is the default face. |
| Weather | A three-letter city code, a weather icon, and the temperature. New York City until you set a city. Glucose alarms still sound on this face. |
| Glucose graph and value | A short glucose graph, the value, and trend dots on the right edge: 2 white dots for flat, white and green for 45°, 3 dots for straight up or down, 4 for double arrows; colored dots above mean rising, below mean falling. Bars under the value count the minutes since the last reading: green for 1–5, yellow from 6, and the old-data color once data is old. |
| Current time and BG value | The time and the value, with the same trend dots. |
| Time only | Only the time. Glucose is hidden, and new glucose alarms are paused while this face is selected. |

### Weather

All faces are on when the clock is first set up. Open the Weather drawer in the **Clock faces** card on the **Display** tab. If the city is empty, the page fills in your location; the location button does it again. Or start typing a city and pick one from the suggestions. A typed city without a pick also works: adding a state, province or country after a comma, for example `Austin, TX`, picks between cities with the same name. The clock shows the city's initials when the name has three or more words (Salt Lake City is SLC), otherwise its first three letters. Choose °F or °C.

Suggestions and the location need internet on the phone or computer, so they don't work while it's joined to the clock's own `nsclock` network. Browsers only share your exact location with HTTPS pages, so the clock's settings page uses your internet address instead, which gives a nearby city.

Weather and city suggestions come from [Open-Meteo](https://open-meteo.com/), which is open source and needs no account or key. Weather updates every 15 minutes.

### Choosing faces

The **Clock faces** card selects which faces are active. The left and right buttons move between active faces, and the default face is one of them. Automatic cycling runs through the active faces every 10 or 30 seconds, or every 1, 2, 3 or 5 minutes. The **Daily schedule** card can instead change the face and brightness at up to eight times a day. Cycling and the schedule can't both be on.

## Settings page

The settings page works offline, on a phone or a computer. Changes take effect when you select **Save and restart**.

- **Display:** clock faces, weather, cycling or a daily schedule, brightness, old-data color, and time zone and format
- **Glucose:** data source, units, range limits and colors
- **Alarms:** high, low and urgent-low thresholds, sounds, snooze, alert windows by weekday and time, and repeat interval
- **WiFi & system:** Wi-Fi and an optional second network, hostname, web login, firmware version, and settings backup and restore

The settings file contains your Wi-Fi and data source passwords, so keep it private.

## Troubleshooting

If the clock doesn't install, connect to Wi-Fi or show data, see the [troubleshooting guide](TROUBLESHOOTING.md). It also explains how to collect logs with the debug firmware.

## API data source

For testing, the clock can receive readings instead of fetching them. Select the API data source in the settings page, then POST Nightscout-style entries (`sgv`, `date`, and `trend` or `direction`) to `/api/v1/entries`. Send fewer than 10 entries at a time.

## Building

See [CONTRIBUTING.md](CONTRIBUTING.md) for the PlatformIO setup and build scripts.

## License

[LGPL-2.1](LICENSE). Not a medical device: always confirm readings with your CGM app or meter.
