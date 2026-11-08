#pragma once
// Smartlab LoRa: S.O.S. router portal (open access point + captive portal). Built only with SMARTLAB_SOS_ROUTER.
#if defined(SMARTLAB_SOS_ROUTER) && defined(ARCH_ESP32)

/// Starts the open access point, the DNS that points every name to the router and the emergency page. Call once, after
/// the radio and the config are up.
void startSosPortal();

/// True when `ip` (host byte order as the 4 octets) belongs to the access point's own subnet (192.168.4.0/24).
bool isSosApClient(uint8_t a, uint8_t b, uint8_t c);

#endif
