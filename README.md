<div align="center">

<img src=".github/smartlab_logo.png" alt="Smartlab LoRa" width="120"/>
<h1>Smartlab LoRa — firmware</h1>

</div>

Firmware per schede LoRa ESP32, pensato per una rete di emergenza e di quartiere **off-grid**. Un progetto di
**Smart Lab Siena**.

È un fork del [firmware Meshtastic](https://github.com/meshtastic/firmware) e resta **compatibile al 100% con il
protocollo Meshtastic**: i nostri nodi parlano con quelli Meshtastic e con l'app
[smartlab-lora-app](https://github.com/smartlabsiena/smartlab-lora-app). Il protocollo non è modificato.

## Cosa cambia rispetto a Meshtastic

- Nome del nodo `SmartLab xxxx`, nome Bluetooth `SmartLab_xxxx`, host `SmartLab.local`, logo di avvio e testi a schermo.
- Server web HTTPS spento di default (`SMARTLAB_NO_HTTPS`): sulle schede ESP32-S3 senza PSRAM, come l'Heltec Wireless
  Tracker, lasciava poca memoria libera e faceva cadere l'API Wi-Fi (TCP 4403). Per il comportamento originale compilare
  con `-DSMARTLAB_NO_HTTPS=0`.
- Una riga di log al minuto con la memoria libera, per misurare quanta ne resta.

I dettagli e le scelte di compatibilità sono in `SMARTLAB_NOTICE.md`.

## Schede provate

Heltec Wireless Tracker V1.1 e Heltec Wireless Stick Lite V3 (ESP32-S3 + SX1262), regione EU_868.

## In sviluppo

Profili di ruolo per le schede: **router S.O.S.** (rete Wi-Fi aperta con pagina di aiuto e invio verso il canale
radio S.O.S., sempre accesa), **backbone** e **B.O.B.** (scheda da zaino con GPS).

## Compilare

```bash
python -m platformio run -e heltec-wireless-tracker
```

Gli ambienti disponibili sono in `variants/`. Per l'aggiornamento via Wi-Fi sulle ESP32-S3 serve il loader OTA nella
partizione dedicata.

## Licenza e attribuzioni

Software libero sotto licenza **GPL-3.0** (vedi `LICENSE`). Il codice originale è di Meshtastic LLC e dei suoi
collaboratori: le intestazioni di copyright sono mantenute.

Meshtastic® è un marchio registrato di Meshtastic LLC. Questo progetto **non è affiliato né approvato** dal progetto
Meshtastic. Il loro README originale è conservato in `README.meshtastic.md`.
