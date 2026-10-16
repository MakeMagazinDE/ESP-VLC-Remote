#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>

// =============================================================
// HIER DIE EIGENEN DATEN EINTRAGEN (vor dem Hochladen)
// Keine echten Passwoerter in ein oeffentliches GitHub-Repository laden.
// =============================================================
const char* WIFI_NAME = "HIER_WLAN_NAME_EINTRAGEN";
const char* WIFI_PASS = "HIER_WLAN_PASSWORT_EINTRAGEN";
const char* VLC_PASS  = "HIER_VLC_PASSWORT_EINTRAGEN";
const char* VLC_IP    = "192.168.1.100";  // Beispiel-IP: durch die eigene VLC-Rechner-IP ersetzen

// Standard fuer klassische ESP32-Boards. Bei C3/S3 oder anderer Belegung
// einen freien, herausgefuehrten GPIO des konkreten Boards eintragen.
const int PIR_PIN = 27;

const uint32_t DEFAULT_PAUSE_SECONDS = 47;
const uint32_t MAX_PAUSE_SECONDS = 3600;
const unsigned long PIR_WARMUP_MS = 60000UL;
const unsigned long MIN_TRIGGER_MS = 5000UL;

WebServer server(80);
Preferences preferences;
String vlcHost = VLC_IP;
String vlcPassword = VLC_PASS;
bool preferencesOpen = false;
bool outputEnabled = true;
bool sensorReady = false;
bool lastPirHigh = false;
bool videoRunning = false;
uint32_t pauseSeconds = DEFAULT_PAUSE_SECONDS;
uint32_t activePauseSeconds = DEFAULT_PAUSE_SECONDS;
unsigned long sensorStart = 0;
unsigned long videoStart = 0;
unsigned long lastTrigger = 0;
unsigned long lastPauseAttempt = 0;

bool vlcCommand(const char* command) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WLAN nicht verbunden");
    return false;
  }
  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + vlcHost +
               ":8080/requests/status.json?command=" + command;
  if (!http.begin(client, url)) {
    Serial.println("HTTP-Verbindung konnte nicht gestartet werden");
    return false;
  }
  http.setAuthorization("", vlcPassword.c_str());
  http.setTimeout(3000);
  int status = http.GET();
  http.end();
  Serial.print("VLC ");
  Serial.print(command);
  Serial.print(": HTTP ");
  Serial.println(status);
  return status == 200;
}

bool startVideo() {
  if (!vlcCommand("pl_play")) return false;
  if (!vlcCommand("seek&val=0")) return false;
  return vlcCommand("pl_forceresume");
}

uint32_t warmupRemainingSeconds() {
  if (sensorReady) return 0;
  unsigned long elapsed = millis() - sensorStart;
  if (elapsed >= PIR_WARMUP_MS) return 0;
  return (PIR_WARMUP_MS - elapsed + 999UL) / 1000UL;
}

uint32_t playbackRemainingSeconds() {
  if (!videoRunning) return 0;
  unsigned long elapsed = millis() - videoStart;
  unsigned long duration = activePauseSeconds * 1000UL;
  if (elapsed >= duration) return 0;
  return (duration - elapsed + 999UL) / 1000UL;
}

void showPage() {
  String page = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 VLC-Steuerung</title>
<style>
:root{font-family:-apple-system,BlinkMacSystemFont,"Helvetica Neue",Arial,sans-serif;color:#202020;background:#fff}
*{box-sizing:border-box}
body{margin:0;padding:38px 28px 72px}
main{max-width:680px}
.page-header{display:flex;align-items:baseline;justify-content:space-between;gap:24px;margin:0 0 34px}
h1{font-size:28px;line-height:1.15;letter-spacing:-.025em;font-weight:600;margin:0}
#sensor-status{margin:0;color:#555;font-size:14px;line-height:1.4;text-align:right;white-space:nowrap}
h2{font-size:15px;line-height:1.3;font-weight:600;margin:0 0 20px;padding-top:18px;border-top:1px solid #d7d7d7}
.controls{display:flex;flex-wrap:wrap;gap:8px}
button{font:inherit;font-size:14px;font-weight:500;cursor:pointer;border-radius:0}
.controls button{background:#fff;border:1px solid #aaa;color:#222;padding:10px 15px;min-height:40px}
.controls button:hover{border-color:#222}
.controls button[data-action=start]{background:#222;border-color:#222;color:#fff}
.controls button[data-action=stop]{margin-left:16px}
.controls button:disabled{opacity:.45;cursor:wait}
#control-result{font-size:13px;color:#555;margin:10px 0 34px;min-height:18px}
#control-result:empty{visibility:hidden}
form h2:not(:first-child){margin-top:39px}
label{display:block;font-size:14px;font-weight:500;margin:22px 0 8px}
.switch{display:flex;align-items:center;gap:10px;margin:0 0 27px}
input[type=checkbox]{width:18px;height:18px;accent-color:#222;margin:0}
input[type=number],input[type=text],input[type=password]{display:block;width:min(100%,420px);border:1px solid #aaa;border-radius:0;padding:10px 11px;font:inherit;font-size:15px;background:#fff;color:#222}
input:focus-visible,button:focus-visible{outline:2px solid #222;outline-offset:2px}
button[type=submit]{background:#222;color:#fff;border:1px solid #222;padding:10px 18px;margin-top:30px;min-height:42px}
@media(max-width:600px){.page-header{display:block}#sensor-status{margin-top:10px;text-align:left;white-space:normal}}
@media(max-width:500px){body{padding:24px 18px 48px}h1{font-size:25px}.controls button[data-action=stop]{margin-left:0}}
</style></head><body><main><header class="page-header"><h1>VLC-Steuerung</h1>
)HTML";
  page += "<p id='sensor-status' aria-live='polite'>";
  if (videoRunning) {
    page += "Video läuft – Pause in ";
    page += String(playbackRemainingSeconds());
    page += " s";
  } else if (!sensorReady) {
    page += "Bewegungsmelder startet – ";
    page += String(warmupRemainingSeconds());
    page += " s";
  } else {
    page += "Bewegungsmelder bereit";
  }
  page += "</p></header>";
  page += R"HTML(<h2>Manuelle Steuerung</h2>
<div class="controls" aria-label="VLC bedienen">
<button type="button" data-action="start">Start</button>
<button type="button" data-action="pause">Pause</button>
<button type="button" data-action="resume">Fortsetzen</button>
<button type="button" data-action="beginning">Zum Anfang</button>
<button type="button" data-action="stop">Stopp</button>
</div><p id="control-result" role="status" aria-live="polite"></p>
<form method="post" action="/settings">
<h2>Automatische Steuerung</h2>
<label class="switch"><input type="checkbox" name="enabled" value="1" )HTML";
  if (outputEnabled) page += "checked";
  page += R"HTML(> Fernauslösung aktiv</label>
<label for="seconds">Zeit bis zur Pause (Sekunden)</label>
<input id="seconds" name="seconds" type="number" min="1" max="3600" step="1" required value=")HTML";
  page += String(pauseSeconds);
  page += R"HTML(">
<h2>Einstellungen</h2>
<label for="vlc-ip">IP-Adresse des VLC-Rechners</label>
<input id="vlc-ip" name="ip" type="text" inputmode="decimal" required value=")HTML";
  page += vlcHost;
  page += R"HTML(">
<label for="new-password">VLC-Passwort (leer lassen, wenn unverändert)</label>
<input id="new-password" name="new_password" type="password" autocomplete="off" maxlength="64">
<button type="submit">Speichern</button></form>
</main><script>
async function updateSensorStatus() {
  try {
    const response = await fetch("/status", {cache: "no-store"});
    if (!response.ok) throw new Error("Status nicht verfügbar");
    const state = await response.json();
    const status = document.getElementById("sensor-status");
    if (state.videoRunning) {
      status.textContent = "Video läuft – Pause in " + state.pauseRemainingSeconds + " s";
    } else if (!state.sensorReady) {
      status.textContent = "Bewegungsmelder startet – " + state.remainingSeconds + " s";
    } else {
      status.textContent = "Bewegungsmelder bereit";
    }
  } catch (error) {
    document.getElementById("sensor-status").textContent = "ESP nicht erreichbar";
  }
}
for (const button of document.querySelectorAll(".controls button")) {
  button.addEventListener("click", async () => {
    const buttons = document.querySelectorAll(".controls button");
    const result = document.getElementById("control-result");
    buttons.forEach(item => item.disabled = true);
    result.textContent = "Befehl wird gesendet …";
    try {
      const response = await fetch("/control", {
        method: "POST",
        headers: {"Content-Type": "application/x-www-form-urlencoded"},
        body: new URLSearchParams({action: button.dataset.action})
      });
      result.textContent = response.ok ? button.textContent + " ausgeführt" : await response.text();
      await updateSensorStatus();
    } catch (error) {
      result.textContent = "ESP nicht erreichbar";
    } finally {
      buttons.forEach(item => item.disabled = false);
    }
  });
}
setInterval(updateSensorStatus, 1000);
updateSensorStatus();
</script></body></html>)HTML";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", page);
}

void showStatus() {
  server.sendHeader("Cache-Control", "no-store");
  String json = "{\"sensorReady\":";
  json += sensorReady ? "true" : "false";
  json += ",\"remainingSeconds\":";
  json += String(warmupRemainingSeconds());
  json += ",\"videoRunning\":";
  json += videoRunning ? "true" : "false";
  json += ",\"pauseRemainingSeconds\":";
  json += String(playbackRemainingSeconds());
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

void controlVlc() {
  if (!server.hasArg("action")) {
    server.send(400, "text/plain; charset=utf-8", "Befehl fehlt.");
    return;
  }
  String action = server.arg("action");
  bool success = false;
  if (action == "start") {
    success = startVideo();
    if (success) {
      videoStart = millis();
      activePauseSeconds = pauseSeconds;
      videoRunning = true;
      lastTrigger = videoStart;
    }
  } else if (action == "pause") {
    success = vlcCommand("pl_forcepause");
    if (success) videoRunning = false;
  } else if (action == "resume") {
    success = vlcCommand("pl_forceresume");
    if (success) {
      videoStart = millis();
      activePauseSeconds = pauseSeconds;
      videoRunning = true;
      lastTrigger = videoStart;
    }
  } else if (action == "beginning") {
    success = vlcCommand("seek&val=0");
    if (success && videoRunning) videoStart = millis();
  } else if (action == "stop") {
    success = vlcCommand("pl_stop");
    if (success) videoRunning = false;
  } else {
    server.send(400, "text/plain; charset=utf-8", "Unbekannter Befehl.");
    return;
  }
  server.send(success ? 200 : 502, "text/plain; charset=utf-8",
              success ? "OK" : "VLC nicht erreichbar oder Befehl fehlgeschlagen.");
}

void saveSettings() {
  if (!server.hasArg("seconds") || !server.hasArg("ip")) {
    server.send(400, "text/plain; charset=utf-8", "Sekunden oder IP-Adresse fehlen.");
    return;
  }
  String raw = server.arg("seconds");
  if (raw.length() < 1 || raw.length() > 4) {
    server.send(400, "text/plain; charset=utf-8", "Bitte 1 bis 3600 Sekunden eingeben.");
    return;
  }
  for (size_t i = 0; i < raw.length(); ++i) {
    if (raw[i] < '0' || raw[i] > '9') {
      server.send(400, "text/plain; charset=utf-8", "Bitte nur ganze Sekunden eingeben.");
      return;
    }
  }
  uint32_t requestedSeconds = (uint32_t)raw.toInt();
  if (requestedSeconds < 1 || requestedSeconds > MAX_PAUSE_SECONDS) {
    server.send(400, "text/plain; charset=utf-8", "Bitte 1 bis 3600 Sekunden eingeben.");
    return;
  }
  String requestedHost = server.arg("ip");
  requestedHost.trim();
  IPAddress parsed;
  if (!parsed.fromString(requestedHost) || parsed == IPAddress(0, 0, 0, 0)) {
    server.send(400, "text/plain; charset=utf-8", "Bitte eine gültige IPv4-Adresse eingeben.");
    return;
  }
  String requestedPassword = server.arg("new_password");
  if (requestedHost != vlcHost && !requestedPassword.length()) {
    server.send(400, "text/plain; charset=utf-8", "Bei geänderter IP-Adresse bitte das VLC-Passwort eingeben.");
    return;
  }
  if (requestedPassword.length() > 64) {
    server.send(400, "text/plain; charset=utf-8", "Das VLC-Passwort ist zu lang.");
    return;
  }
  for (size_t i = 0; i < requestedPassword.length(); ++i) {
    if ((uint8_t)requestedPassword[i] < 32 || (uint8_t)requestedPassword[i] == 127) {
      server.send(400, "text/plain; charset=utf-8", "Das VLC-Passwort enthält ungültige Zeichen.");
      return;
    }
  }
  if (!preferencesOpen) {
    server.send(500, "text/plain; charset=utf-8", "Einstellungen können nicht gespeichert werden.");
    return;
  }
  bool requestedEnabled = server.hasArg("enabled");
  bool saved = true;
  if (requestedEnabled != outputEnabled) {
    if (preferences.putBool("enabled", requestedEnabled)) outputEnabled = requestedEnabled;
    else saved = false;
  }
  if (requestedSeconds != pauseSeconds) {
    if (preferences.putUInt("seconds", requestedSeconds)) pauseSeconds = requestedSeconds;
    else saved = false;
  }
  if (requestedHost != vlcHost) {
    if (preferences.putString("host", requestedHost)) vlcHost = requestedHost;
    else saved = false;
  }
  if (requestedPassword.length() && requestedPassword != vlcPassword) {
    if (preferences.putString("vlcpw", requestedPassword)) vlcPassword = requestedPassword;
    else saved = false;
  }
  if (!saved) {
    server.send(500, "text/plain; charset=utf-8", "Speichern fehlgeschlagen.");
    return;
  }
  Serial.print("PIR-Auslösung: ");
  Serial.println(outputEnabled ? "ein" : "aus");
  Serial.print("Pause nach Sekunden: ");
  Serial.println(pauseSeconds);
  Serial.print("VLC-Rechner: ");
  Serial.println(vlcHost);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain; charset=utf-8", "Gespeichert");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  preferencesOpen = preferences.begin("vlc-remote", false);
  if (preferencesOpen) {
    outputEnabled = preferences.getBool("enabled", true);
    pauseSeconds = preferences.getUInt("seconds", DEFAULT_PAUSE_SECONDS);
    vlcHost = preferences.getString("host", String(VLC_IP));
    vlcPassword = preferences.getString("vlcpw", String(VLC_PASS));
    IPAddress storedAddress;
    if (!storedAddress.fromString(vlcHost)) vlcHost = VLC_IP;
    if (!vlcPassword.length()) vlcPassword = VLC_PASS;
    if (pauseSeconds < 1 || pauseSeconds > MAX_PAUSE_SECONDS)
      pauseSeconds = DEFAULT_PAUSE_SECONDS;
  } else {
    Serial.println("Einstellungen konnten nicht geladen werden");
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_NAME, WIFI_PASS);
  Serial.print("Verbinde WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WLAN verbunden");
  Serial.print("Weboberfläche: http://");
  Serial.println(WiFi.localIP());
  server.on("/", HTTP_GET, showPage);
  server.on("/status", HTTP_GET, showStatus);
  server.on("/control", HTTP_POST, controlVlc);
  server.on("/settings", HTTP_POST, saveSettings);
  server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
  sensorStart = millis();
  Serial.println("Bewegungsmelder startet");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  bool pirHigh = digitalRead(PIR_PIN) == HIGH;
  if (videoRunning &&
      now - videoStart >= activePauseSeconds * 1000UL &&
      now - lastPauseAttempt >= 2000UL) {
    lastPauseAttempt = now;
    if (vlcCommand("pl_forcepause")) {
      videoRunning = false;
      Serial.println("Video pausiert");
    }
  }
  if (!sensorReady) {
    if (now - sensorStart >= PIR_WARMUP_MS) {
      sensorReady = true;
      lastPirHigh = pirHigh;
      Serial.println("Bewegungsmelder bereit");
    }
    delay(10);
    return;
  }
  if (outputEnabled && !videoRunning && pirHigh && !lastPirHigh &&
      now - lastTrigger >= MIN_TRIGGER_MS) {
    lastTrigger = now;
    Serial.println("Bewegung erkannt");
    if (startVideo()) {
      videoStart = millis();
      activePauseSeconds = pauseSeconds;
      videoRunning = true;
    }
  }
  lastPirHigh = pirHigh;
  delay(10);
}
