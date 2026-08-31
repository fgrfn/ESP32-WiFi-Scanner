# Web dashboard

## Access

Enable **SET > Web-Dashboard**, connect to `WiFi-Radar-V2` with password
`radar1234`, and open `http://192.168.4.1`. The ESP32 runs an isolated access
point and does not route internet traffic.

The page displays the network table, per-channel load, security distribution,
scan age, and recommendation. It follows the saved German/English selection and
can change language, scan interval, SSID grouping, hidden-network display,
theme, SD logging, and brightness (25/50/75/100%); start a scan; export CSVs; or
request a factory reset.

## Time synchronization

On page load, JavaScript sends Unix time and the browser timezone offset to the
device. Later SD scan entries then use local ISO-like timestamps. The ESP32 has
no RTC, so the synchronized time is valid only until reboot. Before sync, the
device uses `U+seconds` since startup.

## Local API

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | Dashboard |
| GET | `/api/networks` | Current scan and settings JSON |
| GET | `/api/csv` | Current scan CSV |
| GET | `/api/history.csv` | Tracked signal history CSV |
| POST | `/api/time` | Browser time sync |
| POST | `/api/settings` | Update settings |
| POST | `/api/scan` | Start/resume scanning |
| POST | `/api/factory-reset` | Clear NVS and restart |

The API has no authentication beyond Wi-Fi access. Do not expose it to an
untrusted network.
