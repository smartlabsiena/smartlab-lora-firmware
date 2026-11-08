# Smartlab LoRa firmware: avviso di modifica / modification notice

**IT** — Questo firmware è una versione **modificata** del [firmware Meshtastic](https://github.com/meshtastic/firmware), modificata da Smart Lab Siena a partire dal 06/11/2026. Resta sotto licenza **GPL-3.0** (vedi `LICENSE`); le intestazioni di copyright originali non sono state rimosse. Meshtastic® è un marchio registrato di Meshtastic LLC; questo progetto non è affiliato né approvato dal progetto Meshtastic ed è compatibile con i nodi Meshtastic (il protocollo non è modificato).

**EN** — This firmware is a **modified** version of the Meshtastic firmware, modified by Smart Lab Siena starting 2026-11-06. It remains under **GPL-3.0**; the original copyright headers are kept. Meshtastic® is a registered trademark of Meshtastic LLC; this project is not affiliated with or endorsed by the Meshtastic project and stays compatible with Meshtastic nodes (the protocol is unchanged).

## Modifiche / Changes

- 2026-11-06 — **HTTPS web server off by default** (`SMARTLAB_NO_HTTPS`, in `src/mesh/http/WebServer.cpp` and `ContentHandler.cpp`). On ESP32-S3 boards without PSRAM (Heltec Wireless Tracker) it left only ~21 KB of free heap, and the Wi-Fi API (TCP 4403) dropped data during the phone's initial config download. Phone apps use TCP 4403, not HTTPS, so they are unaffected. Build with `-DSMARTLAB_NO_HTTPS=0` for the original behaviour.
- 2026-11-07 — **Free-heap log line once a minute** (`[Heap] free=… min=… largest-block=…`) in the web server thread, while Wi-Fi is on, to measure the memory left by the API and web servers.
- 2026-11-07 — **Branding changes** (Smartlab LoRa): default node name `SmartLab xxxx`, BLE name `SmartLab_xxxx`, mDNS host `SmartLab.local`, DHCP hostname `SmartLab-xxxx`, NTP default `pool.ntp.org`, boot logo (`src/graphics/img/icon*.xbm`) and on-screen/web texts. **Not changed on purpose** (compatibility with the Meshtastic protocol and nodes): protobuf package/type names, BLE service UUIDs, the mDNS service type `_meshtastic._tcp`, the NVS namespace `meshtastic`, MQTT topic scheme, and all copyright/licence headers (GPL-3.0 requires keeping them).
