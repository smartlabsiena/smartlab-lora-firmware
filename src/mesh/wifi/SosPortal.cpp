#include "configuration.h"

#if defined(SMARTLAB_SOS_ROUTER) && defined(ARCH_ESP32)
#include "SosPortal.h"
#include "Channels.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "Router.h"
#include "concurrency/OSThread.h"
#include "modules/TextMessageModule.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <esp_netif.h>

// Smartlab LoRa S.O.S. router portal: an open access point with a captive portal.
//  - every DNS name resolves to the router and every request for another host is redirected to the page;
//  - the page sends a short text on the public channel "S.O.S." (index 1) and lists the recent messages of that channel;
//  - limits: one message per 30 s per phone, 20 per hour in total; messages vanish from the page after 24 h.
// Nothing here gives access to the node API (port 4403 is refused for this subnet in ServerAPI.cpp) or to other channels.

#ifndef SMARTLAB_SOS_MAX_CLIENTS
#define SMARTLAB_SOS_MAX_CLIENTS 3
#endif
#define SOS_TEXT_MAX 100
#define SOS_KEEP 20
#define SOS_EXPIRE_MS (24UL * 3600UL * 1000UL)
#define SOS_PER_PHONE_MS 30000UL
#define SOS_PER_HOUR 20

static const IPAddress apIp(192, 168, 4, 1);

/// Index of the channel named "S.O.S." in the channel table (wherever the user put it), or -1 when there is none.
static int sosChannelIndex()
{
    for (uint8_t i = 0; i < channels.getNumChannels(); i++) {
        const meshtastic_Channel &c = channels.getByIndex(i);
        if (c.role != meshtastic_Channel_Role_DISABLED && strcmp(c.settings.name, "S.O.S.") == 0)
            return i;
    }
    return -1;
}

static const char PAGE[] =
    "<!doctype html><html lang=it><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>S.O.S.</title><style>body{font-family:sans-serif;margin:0;background:#142b3a;color:#f5e9d8}main{padding:18px;max-width:"
    "520px;margin:auto}h1{color:#63d4c6;margin:.2em 0}h2{font-size:17px;margin:20px 0 6px}.b{display:block;width:100%;padding:16px;"
    "margin:10px 0;font-size:18px;border:0;border-radius:10px;color:#fff}.s{background:#d83a2e}.o{background:#2e9e4f}.n{background:"
    "#3f6fd1}textarea{width:100%;box-sizing:border-box;font-size:16px;padding:10px;border-radius:8px;border:0;height:70px}small{"
    "opacity:.8}pre{white-space:pre-wrap;font:14px sans-serif;background:#0e2230;padding:10px;border-radius:8px;min-height:40px}"
    "#st{margin:8px 0;color:#63d4c6}</style></head><body><main><h1>S.O.S.</h1>"
    "<p>Rete di emergenza off-grid. I messaggi vanno sul canale pubblico S.O.S. della radio LoRa.</p>"
    "<textarea id=t maxlength=100 placeholder='Messaggio breve (facoltativo)'></textarea>"
    "<button class='b s' onclick=\"s('SOS! ')\">SOS</button><button class='b o' onclick=\"s('Sto bene. ')\">Sto bene</button>"
    "<button class='b n' onclick=\"s('')\">Invia messaggio</button><div id=st></div>"
    "<h2>Messaggi recenti</h2><pre id=l>...</pre>"
    "<small>Canale pubblico: non scrivere dati riservati. Questa pagina non chiede mai dati personali, password o pagamenti. "
    "I messaggi scompaiono dopo 24 ore.</small></main><script>"
    "function s(p){var t=document.getElementById('t'),m=(p+t.value).trim();if(!m)return;"
    "fetch('/api/s',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'m='+encodeURIComponent(m)})"
    ".then(function(r){return r.text()}).then(function(x){document.getElementById('st').textContent=x;t.value='';u()})"
    ".catch(function(){document.getElementById('st').textContent='Errore di connessione'})}"
    "function u(){fetch('/api/m').then(function(r){return r.text()}).then(function(x){document.getElementById('l').textContent="
    "x||'Nessun messaggio'}).catch(function(){})}u();setInterval(u,5000)</script></body></html>";

struct SosMsg {
    uint32_t atMs;
    char who[12];
    char text[SOS_TEXT_MAX + 1];
    bool used;
};

class SosPortalThread : public concurrency::OSThread
{
  public:
    SosPortalThread() : OSThread("SosPortal") {}

    void begin()
    {
        // With the home Wi-Fi configured the board stays connected to it too (OTA updates, MQTT): access point + station.
        WiFi.mode((config.network.wifi_enabled && config.network.wifi_ssid[0]) ? WIFI_AP_STA : WIFI_AP);
        WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
        // Open network (no password). The name follows the node's long name, so the place is set with the normal
        // Meshtastic configuration.
        const char *ssid = owner.long_name[0] ? owner.long_name : "S.O.S.";
        WiFi.softAP(ssid, nullptr, 1, 0, SMARTLAB_SOS_MAX_CLIENTS);
        announceCaptivePortal();
        dns.start(53, "*", apIp);
        server.begin(80);
        LOG_INFO("[SOS] access point '%s' on 192.168.4.1, max %d phones", ssid, SMARTLAB_SOS_MAX_CLIENTS);
    }

    int onText(const meshtastic_MeshPacket *mp)
    {
        int sosIdx = sosChannelIndex();
        if (sosIdx < 0 || mp->channel != (uint32_t)sosIdx || mp->which_payload_variant != meshtastic_MeshPacket_decoded_tag)
            return 0;
        char who[12];
        const meshtastic_NodeInfoLite *n = nodeDB->getMeshNode(mp->from);
        if (n && nodeInfoLiteHasUser(n) && n->short_name[0])
            snprintf(who, sizeof(who), "%s", n->short_name);
        else
            snprintf(who, sizeof(who), "!%04x", (unsigned)(mp->from & 0xffff));
        char text[SOS_TEXT_MAX + 1];
        size_t len = mp->decoded.payload.size < SOS_TEXT_MAX ? mp->decoded.payload.size : SOS_TEXT_MAX;
        memcpy(text, mp->decoded.payload.bytes, len);
        text[len] = 0;
        addMessage(who, text);
        return 0;
    }

  protected:
    int32_t runOnce() override
    {
        if (!observing && textMessageModule) {
            textObserver.observe(textMessageModule);
            observing = true;
        }
        dns.processNextRequest();
        WiFiClient client = server.accept();
        if (client)
            handle(client);

        static uint32_t lastLog = 0;
        if (millis() - lastLog > 60000) {
            lastLog = millis();
            LOG_INFO("[SOS] clients=%d heap free=%u min=%u largest=%u", WiFi.softAPgetStationNum(), (unsigned)ESP.getFreeHeap(),
                     (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
        }
        return 20;
    }

  private:
    DNSServer dns;
    WiFiServer server;
    SosMsg msgs[SOS_KEEP] = {};
    uint8_t next = 0;
    bool observing = false;
    uint32_t phoneIp[4] = {};
    uint32_t phoneLast[4] = {};
    uint32_t sentAt[SOS_PER_HOUR] = {};
    uint8_t sentNext = 0;
    CallbackObserver<SosPortalThread, const meshtastic_MeshPacket *> textObserver =
        CallbackObserver<SosPortalThread, const meshtastic_MeshPacket *>(this, &SosPortalThread::onText);

    /// DHCP option 114 (RFC 8910): tells recent Android and iOS where the portal is, so they open it by themselves.
    void announceCaptivePortal()
    {
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (!netif)
            return;
        static const char uri[] = "http://192.168.4.1/capport";
        esp_netif_dhcps_stop(netif);
        esp_err_t err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, (void *)uri, strlen(uri));
        esp_netif_dhcps_start(netif);
        LOG_INFO("[SOS] DHCP option 114 %s", err == ESP_OK ? "set" : "not accepted");
    }

    void addMessage(const char *who, const char *text)
    {
        SosMsg &m = msgs[next];
        next = (next + 1) % SOS_KEEP;
        m.atMs = millis();
        snprintf(m.who, sizeof(m.who), "%s", who);
        snprintf(m.text, sizeof(m.text), "%s", text);
        m.used = true;
    }

    static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

    /// Reads the value of form field "m" (application/x-www-form-urlencoded) into out, dropping control characters.
    static void formText(const char *body, char *out, size_t cap)
    {
        const char *p = strstr(body, "m=");
        size_t n = 0;
        if (p) {
            for (p += 2; *p && *p != '&' && n + 1 < cap; p++) {
                char c = *p;
                if (c == '+')
                    c = ' ';
                else if (c == '%' && hexv(p[1]) >= 0 && hexv(p[2]) >= 0) {
                    c = (char)(hexv(p[1]) * 16 + hexv(p[2]));
                    p += 2;
                }
                if ((unsigned char)c >= 0x20 && c != 0x7f)
                    out[n++] = c;
            }
        }
        // do not cut a multi-byte character in half
        while (n > 0 && ((unsigned char)out[n - 1] & 0xC0) == 0x80)
            n--;
        if (n > 0 && ((unsigned char)out[n - 1] & 0x80) && ((unsigned char)out[n - 1] & 0x40)) // lead byte without its tail
            n--;
        out[n] = 0;
    }

    const char *trySend(IPAddress ip, const char *text)
    {
        if (!text[0])
            return "Messaggio vuoto.";
        int sosIdx = sosChannelIndex();
        if (sosIdx < 0)
            return "Canale S.O.S. non configurato sul router.";
        uint32_t now = millis();
        uint32_t key = (uint32_t)ip;
        int slot = -1;
        for (int i = 0; i < 4; i++)
            if (phoneIp[i] == key)
                slot = i;
        if (slot < 0) {
            slot = 0;
            for (int i = 1; i < 4; i++)
                if (phoneLast[i] < phoneLast[slot])
                    slot = i;
            phoneIp[slot] = key;
            phoneLast[slot] = 0;
        }
        if (phoneLast[slot] != 0 && now - phoneLast[slot] < SOS_PER_PHONE_MS)
            return "Attendi 30 secondi prima di inviare un altro messaggio.";
        int inHour = 0;
        for (int i = 0; i < SOS_PER_HOUR; i++)
            if (sentAt[i] != 0 && now - sentAt[i] < 3600000UL)
                inHour++;
        if (inHour >= SOS_PER_HOUR)
            return "Troppi messaggi nell'ultima ora: riprova più tardi.";

        meshtastic_MeshPacket *p = router ? router->allocForSending() : nullptr;
        if (!p)
            return "Memoria insufficiente, riprova.";
        p->to = NODENUM_BROADCAST;
        p->channel = sosIdx;
        p->want_ack = false;
        p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
        size_t len = strlen(text);
        memcpy(p->decoded.payload.bytes, text, len);
        p->decoded.payload.size = len;
        service->sendToMesh(p, RX_SRC_LOCAL, true);

        phoneLast[slot] = now ? now : 1;
        sentAt[sentNext] = now ? now : 1;
        sentNext = (sentNext + 1) % SOS_PER_HOUR;
        addMessage("Io", text);
        LOG_INFO("[SOS] message sent on the S.O.S. channel (%u bytes)", (unsigned)len);
        return "Inviato sulla radio. Nessuna conferma di ricezione garantita.";
    }

    void reply(WiFiClient &c, const char *status, const char *type, const char *body, const char *extra = "")
    {
        c.printf("HTTP/1.1 %s\r\nContent-Type: %s\r\nCache-Control: no-store\r\nConnection: close\r\n%sContent-Length: %u\r\n\r\n", status,
                 type, extra, (unsigned)strlen(body));
        c.write((const uint8_t *)body, strlen(body));
    }

    void handle(WiFiClient &c)
    {
        char req[900];
        size_t got = 0;
        uint32_t start = millis();
        // read until the end of the headers (and the small body that follows) or a short timeout
        while (c.connected() && millis() - start < 400 && got < sizeof(req) - 1) {
            int a = c.available();
            if (a <= 0) {
                delay(5);
                continue;
            }
            int r = c.read((uint8_t *)req + got, min((size_t)a, sizeof(req) - 1 - got));
            if (r > 0)
                got += r;
            req[got] = 0;
            const char *hdrEnd = strstr(req, "\r\n\r\n");
            if (hdrEnd) {
                const char *cl = strcasestr(req, "content-length:");
                size_t want = cl ? (size_t)atoi(cl + 15) : 0;
                if (got >= (size_t)(hdrEnd + 4 - req) + want)
                    break;
            }
        }
        req[got] = 0;

        bool isPost = strncmp(req, "POST ", 5) == 0;
        const char *path = req + (isPost ? 5 : 4);
        bool isGet = strncmp(req, "GET ", 4) == 0;
        if (!isPost && !isGet) {
            c.stop();
            return;
        }
        char pathBuf[40];
        size_t pl = 0;
        while (path[pl] && path[pl] != ' ' && path[pl] != '?' && pl < sizeof(pathBuf) - 1) {
            pathBuf[pl] = path[pl];
            pl++;
        }
        pathBuf[pl] = 0;
        const char *host = strcasestr(req, "\r\nhost:");

        if (isPost && strcmp(pathBuf, "/api/s") == 0) {
            const char *body = strstr(req, "\r\n\r\n");
            char text[SOS_TEXT_MAX + 1] = {};
            if (body)
                formText(body + 4, text, sizeof(text));
            reply(c, "200 OK", "text/plain; charset=utf-8", trySend(c.remoteIP(), text));
        } else if (isGet && strcmp(pathBuf, "/api/m") == 0) {
            char out[SOS_KEEP * (SOS_TEXT_MAX + 30)];
            size_t n = 0;
            uint32_t now = millis();
            // newest first
            for (int k = 1; k <= SOS_KEEP; k++) {
                const SosMsg &m = msgs[(next + SOS_KEEP - k) % SOS_KEEP];
                if (!m.used || now - m.atMs > SOS_EXPIRE_MS)
                    continue;
                unsigned mins = (now - m.atMs) / 60000UL;
                n += snprintf(out + n, sizeof(out) - n, "[%s] %s (%u min fa)\n", m.who, m.text, mins);
                if (n >= sizeof(out) - 1)
                    break;
            }
            out[n] = 0;
            reply(c, "200 OK", "text/plain; charset=utf-8", out);
        } else if (isGet && strcmp(pathBuf, "/capport") == 0) {
            reply(c, "200 OK", "application/captive+json", "{\"captive\":true,\"user-portal-url\":\"http://192.168.4.1/\"}");
        } else if (isGet && (strcmp(pathBuf, "/") == 0) && host && strstr(host, "192.168.4.1")) {
            reply(c, "200 OK", "text/html; charset=utf-8", PAGE);
        } else {
            // any other host or path (the connectivity checks of Android, iOS and Windows): send the phone to the page
            reply(c, "302 Found", "text/plain", "", "Location: http://192.168.4.1/\r\n");
        }
        c.stop();
    }
};

static SosPortalThread *sosPortal;

void startSosPortal()
{
    if (!sosPortal) {
        sosPortal = new SosPortalThread();
        sosPortal->begin();
    }
}

bool isSosApClient(uint8_t a, uint8_t b, uint8_t c)
{
    return a == 192 && b == 168 && c == 4;
}

#endif
