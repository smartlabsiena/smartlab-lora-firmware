<div align="center">

<img src=".github/smartlab_logo.png" alt="Smartlab LoRa" width="120"/>
<h1>Smartlab LoRa — firmware</h1>

</div>

Firmware per schede radio LoRa ESP32, pensato per una rete di emergenza e di quartiere **off-grid**. Un progetto di
**Smart Lab Siena**.

## A cosa serve

Ogni scheda è un nodo di una rete radio che non ha bisogno di internet né di ripetitori telefonici: riceve i messaggi
degli altri nodi e li ripete, così raggiungono zone lontane anche con poche schede. Con la scheda si comunica dal
telefono tramite l'app [smartlab-lora-app](https://github.com/smartlabsiena/smartlab-lora-app).

## Come funziona

- **Radio**: i messaggi viaggiano via LoRa (banda EU 868 MHz) da nodo a nodo; i canali sono cifrati con una chiave
  condivisa tra chi ne fa parte. Ogni nodo mantiene l'elenco dei vicini e, se ha un GPS, ne annuncia la posizione.
- **Collegamento al telefono**: via Bluetooth, USB o Wi-Fi (API sulla porta TCP 4403, annuncio mDNS `<nome>.local`).
- **Nomi**: nome del nodo, nome breve e nome Bluetooth si personalizzano dall'app. Di default il nodo si chiama
  `SmartLab xxxx` e il Bluetooth `LoRa_xxxx`; scelto un nome breve, Bluetooth e nome di rete diventano
  `<nome breve>_xxxx` e `<nome breve>-xxxx.local` al riavvio (`xxxx` sono le ultime cifre dell'indirizzo hardware).
- **Display**: sulle schede con schermo mostra nome del nodo, stato della rete, messaggi e memoria libera.
- **Gateway**: con il Wi-Fi collegato, una scheda può inoltrare i messaggi della rete radio verso un broker MQTT e
  viceversa, per fare da ponte verso internet.
- **Aggiornamenti**: via USB oppure via Wi-Fi (OTA) con un loader dedicato nella partizione di avvio.
- **Memoria**: il server web HTTPS è spento di default (`SMARTLAB_NO_HTTPS`) per lasciare memoria libera alle schede
  ESP32-S3 senza PSRAM; si riattiva compilando con `-DSMARTLAB_NO_HTTPS=0`. Un log al minuto indica la memoria libera.

## Schede provate

Heltec Wireless Tracker V1.1 e Heltec Wireless Stick Lite V3 (ESP32-S3 + SX1262), regione EU_868.

## Compilare

```bash
python -m platformio run -e heltec-wireless-tracker
```

Gli ambienti disponibili sono in `variants/`.

## Licenza e attribuzioni

Software libero sotto licenza **GPL-3.0** (vedi `LICENSE`). Il progetto nasce dal
[firmware Meshtastic](https://github.com/meshtastic/firmware) e ne usa il protocollo, quindi dialoga con i nodi
Meshtastic e con la nostra app. Le intestazioni di copyright originali sono mantenute; le modifiche sono elencate in
`SMARTLAB_NOTICE.md` e il README originale è in `README.meshtastic.md`.

Meshtastic® è un marchio registrato di Meshtastic LLC.