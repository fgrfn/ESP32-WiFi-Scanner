# WiFi Radar

WiFi Radar ist ein eigenständiger 2,4-GHz-WLAN-Analysator für das
ESP32-2432S028R „Cheap Yellow Display“ (CYD). Er zeigt Netzwerke, Access Points,
Signalverläufe, Kanalbelegung und Sicherheitsarten auf dem Display und in einem
lokalen Web-Dashboard an.

## Funktionen

- automatische Touchkalibrierung beim ersten Start und manuelle Neukalibrierung
- WLAN-Liste mit Seiten, Laufschrift und optionaler SSID-/Mesh-Gruppierung
- BSSID-/AP-Vergleich, RSSI, Qualität, Kanal, Verschlüsselung und Offline-OUI
- Signalverlauf mit Minimum, Maximum, Mittelwert, Ausfällen und Kanalwechseln
- gewichtete Kanalanalyse mit Empfehlung für Kanal 1, 6 oder 11
- Sicherheitsübersicht, NVS-Einstellungen, Helligkeit und helles/dunkles Schema
- RGB-Status-LED, SD-CSV-Protokolle und lokales Web-Dashboard
- Browser-Zeitabgleich, CSV-Export und bestätigungspflichtiger Werksreset

## Schnellstart

1. Arduino IDE und ESP32 Board-Paket `3.3.11` installieren.
2. `TFT_eSPI` `2.5.43` und `XPT2046_Touchscreen` `1.4` installieren.
3. `WiFi_Radar/WiFi_Radar.ino` öffnen.
4. **ESP32 Dev Module** auswählen, Port wählen, kompilieren und hochladen.
5. Beim ersten Start die beiden Kalibrierkreuze berühren.

Die lokale Datei `tft_setup.h` wird über `build_opt.h` für Sketch und Bibliothek
geladen. Die globale `TFT_eSPI`-Installation muss nicht geändert werden. Die Include-Reihenfolge mit
`FS.h` vor `TFT_eSPI.h` und `WebServer.h` ist für ESP32 Core 3.3.11 zwingend.

## Bedienung

- Netzwerk antippen: Detailansicht und Signalverlauf
- **AP-LISTE**: BSSIDs derselben SSID vergleichen
- **LIST** oder horizontale Wischgeste: Seite wechseln
- **KANAL** / **SICHER**: Kanal- und Sicherheitsanalyse
- **SET**: Einstellungen, Kalibrierung und Werksreset
- BOOT-Taster / **PAUSE**: Scan anhalten oder fortsetzen

## Web-Dashboard

Unter **SET** aktivieren und mit `WiFi-Radar-V2` verbinden. Das Passwort lautet
`radar1234`, die Adresse `http://192.168.4.1`. Das Netz bietet keinen
Internetzugang. Beim Öffnen wird die Browserzeit übertragen; wegen der fehlenden
Echtzeituhr muss dies nach jedem Neustart erneut erfolgen.

## Grenzen

Der klassische ESP32 erkennt ausschließlich 2,4-GHz-WLAN. Die Empfehlung ist
keine Spektrumanalyse. Touch und SD nutzen denselben SPI-Controller mit
unterschiedlichen Pins; die kontrollierte Busumschaltung im Sketch muss erhalten
bleiben.

Ausführliche Informationen stehen in der [englischen README](README.md) und im
Ordner [`docs`](docs/).

## Ursprung und Danksagung

Grundidee und anfängliche Codebasis stammen aus Marcos MakerWorld-Projekt
[WIFI ANALIZER - ESP32 LVGL 2432S028 2.8\"](https://makerworld.com/de/models/2303837-wifi-analizer-esp32-lvgl-2432s028-2-8#profileId-2514600).
Die Firmware wurde anschließend erheblich erweitert. Dort wird auch ein
passendes druckbares Gehäuse angeboten; dessen Dateien werden in diesem
Repository nicht weitergegeben. Details stehen in [ATTRIBUTION.md](ATTRIBUTION.md).

Vor einer Veröffentlichung muss neben der eigenen Lizenzwahl geklärt werden,
unter welchen Bedingungen der ursprüngliche Arduino-Code weitergegeben werden
darf oder ob Marco eine ausdrückliche Freigabe erteilt.
