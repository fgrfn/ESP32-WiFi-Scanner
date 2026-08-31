# SD logging and CSV formats

Enable **SET > SD CSV-Log** with a compatible FAT-formatted card inserted.

## `/wifi-scanner.csv`

One row per detected BSSID after every completed scan:

```text
timestamp,ssid,bssid,vendor,channel,rssi,quality,security
```

## `/wifi-history.csv`

One row per completed scan while a BSSID is selected:

```text
timestamp,ssid,bssid,channel,rssi,visible,channel_changes,outages
```

Commas in SSIDs are replaced with underscores. Before browser time sync,
timestamps use `U+seconds`; afterward they use local `YYYY-MM-DDTHH:MM:SS`.

The web history export has a separate sample-oriented format:

```text
sample,ssid,bssid,channel,rssi
```

Touch and SD share a hardware SPI controller. Short input pauses during writes
are expected; preserve the controlled bus switching code.
