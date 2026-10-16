![GitHub Logo](http://www.heise.de/make/icons/make_logo.png)

Maker Media GmbH

***

# ESP32 VLC Bewegungsfernbedienung

Ein PIR-Bewegungsmelder am ESP32 startet eine in VLC geladene Videodatei auf einem Rechner im selben WLAN. Bei einer Auslösung im Bereitschaftszustand beginnt das Video am Anfang und wird nach einer einstellbaren Zeit pausiert. Eine lokale Webseite bietet manuelle Steuerung und Einstellungen.

## Benötigt

- Ein ESP32-Entwicklungsboard mit WLAN und einem freien digitalen Eingang
- Ein PIR-Sensor mit digitalem Ausgang
- USB-Kabel mit Datenleitungen, Stromversorgung, Verbindungskabel
- Ein Rechner mit VLC 3 und aktivierter HTTP-Weboberfläche
- Arduino IDE mit dem Paket **esp32 by Espressif Systems**

Für den einfachsten Nachbau wird ein klassisches ESP32-Devboard empfohlen. **WROOM ist nur eine Modulbezeichnung:** Auch ein anderes Modul mit klassischem ESP32-Chip kann passen. Entscheidend sind der tatsächliche Chip und die Pins, die das Board herausführt. Wähle in der Arduino IDE das tatsächlich verwendete Board; wenn es nicht gelistet ist, den generischen `ESP32-XX Dev Module` derselben Chipfamilie.

### Welches ESP32-Board?

| Chipfamilie | Anpassung | Stand |
| --- | --- | --- |
| Klassischer ESP32, z. B. WROOM, WROVER oder PICO | GPIO 27 verwenden, sofern am Board frei und herausgeführt | Auf ESP32-WROOM-32 praktisch getestet |
| ESP32-C3 | GPIO 27 existiert nicht. Sensor an einen geeigneten freien GPIO anschließen und `PIR_PIN` entsprechend ändern | Mit GPIO 4 kompiliert; noch nicht am Gerät getestet |
| ESP32-S3 | GPIO 27 ist meist für Flash/PSRAM reserviert. Einen geeigneten freien GPIO verwenden und `PIR_PIN` ändern | Mit GPIO 4 kompiliert; noch nicht am Gerät getestet |
| Weitere ESP32 mit integriertem WLAN, z. B. S2 oder C6 | Chipfamilie in der IDE korrekt wählen und einen geeigneten Sensor-GPIO festlegen | Noch nicht getestet |

Boards mit **ESP32-H2 oder ESP32-P4 ohne zusätzliches WLAN** eignen sich für diese WLAN-Version nicht. Bei unbekannten Boards zuerst die Chipbezeichnung und Pinbelegung des Herstellers nachsehen. Ein Pin mit derselben Zahl ist nicht bei jedem Board frei oder überhaupt vorhanden.

## Sensor anschließen

| PIR-Sensor | ESP32 |
| --- | --- |
| GND | GND |
| OUT | GPIO 27 (Vorgabe für klassischen ESP32) |
| VCC/+ | Versorgung gemäß Datenblatt des Sensors |

**Wichtig:** Am GPIO des ESP32 dürfen höchstens 3,3 V anliegen. Die nötige Versorgungsspannung des PIR-Moduls und sein Ausgangspegel sind vom konkreten Modul abhängig. Bei einem 5-V-Ausgang einen Pegelwandler oder Spannungsteiler verwenden. Bei einem anderen ESP32-Board den angeschlossenen GPIO oben im Sketch anpassen. Boot- und Flash-Pins möglichst meiden.

## VLC vorbereiten

1. Video in VLC öffnen und Vollbild einschalten.
2. Unter **Einstellungen → Alle → Interface → Hauptinterfaces** das **Web**-Interface aktivieren. Unter **Hauptinterfaces → Lua** ein HTTP-Passwort setzen und VLC neu starten.
3. Im Browser auf dem VLC-Rechner `http://127.0.0.1:8080/requests/status.json` öffnen. Bei der Anmeldung den Benutzernamen leer lassen und das VLC-Passwort eingeben.
4. Vom Handy im selben Netz `http://RECHNER-IP:8080/requests/status.json` testen. Das muss funktionieren, bevor der ESP VLC erreichen kann. Firewall und WLAN-Gastnetze können den Zugriff verhindern.

## Auf den ESP32 laden

1. Dieses Repository herunterladen und `ESP32-VLCremote/ESP32-VLCremote.ino` in der Arduino IDE öffnen.
2. Ganz oben im deutlich markierten Abschnitt die Platzhalter für WLAN-Name, WLAN-Passwort, VLC-Passwort und IP-Adresse des VLC-Rechners durch eigene Werte ersetzen. Für andere Boards gegebenenfalls `PIR_PIN` ändern. **Einen so angepassten Sketch mit echten Zugangsdaten nicht auf GitHub hochladen oder weitergeben.**
3. Das ESP32-Boardpaket installieren und das passende Board sowie den USB-Anschluss auswählen.
4. Sketch hochladen. Im seriellen Monitor bei **115200 Baud** erscheint die IP-Adresse der Weboberfläche. Diese Adresse im Browser öffnen.
5. Der Sensor braucht nach dem Start ungefähr 60 Sekunden. Auf der Webseite läuft die Zeit herunter. Danach den Sensor auslösen und VLC prüfen.

Die eingebauten Bibliotheken `WiFi`, `HTTPClient`, `WebServer` und `Preferences` gehören zum ESP32-Boardpaket; weitere Arduino-Bibliotheken sind nicht nötig.

## Bedienung

- **Manuelle Steuerung:** Start, Pause, Fortsetzen, Zum Anfang und Stopp.
- **Automatische Steuerung:** Fernauslösung aktivieren und Sekunden bis zur Pause festlegen.
- **Einstellungen:** IP-Adresse des VLC-Rechners und VLC-Passwort anpassen; ein gemeinsamer Speicherknopf.

Einstellungen auf der Webseite bleiben nach einem Neustart im Speicher dieses ESP32 erhalten. Beim Wechsel auf ein anderes Board müssen sie dort erneut gesetzt werden. Änderungen oben im Sketch werden erst nach erneutem Hochladen wirksam; bereits im Gerät gespeicherte Web-Einstellungen haben Vorrang.

Die ESP-Weboberfläche ist im lokalen Netz ohne Anmeldung erreichbar. VLC und ESP deshalb nur in einem vertrauenswürdigen LAN nutzen und Port 80/8080 nicht ins Internet freigeben. Die Verbindung zu VLC nutzt HTTP; das VLC-Passwort wird im Gerät gespeichert.

## Fehlersuche

- **Kein serieller Anschluss:** Datenfähiges USB-Kabel verwenden; gegebenenfalls USB-Seriell-Treiber für das konkrete Board installieren.
- **Upload startet nicht:** Passendes Board/Port prüfen. Manche Boards benötigen beim Hochladen einen Druck auf **BOOT**.
- **ESP erreicht WLAN nicht:** WLAN-Name und Passwort prüfen; viele ESP32-Boards benötigen ein 2,4-GHz-Netz.
- **Webseite erreichbar, VLC nicht:** IP, VLC-Passwort, VLC-Webinterface und Zugriff vom Handy prüfen.
- **PIR löst nicht aus:** 60 Sekunden Startzeit abwarten, Pinbelegung und Ausgangspegel prüfen. Einige PIR-Module haben einen Jumper für Einzel- oder Wiederholungsauslösung.

## Arduino-ESP32 und GitHub

- [Espressif: Arduino-ESP32 installieren](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [Espressif: Board in der Arduino IDE auswählen](https://docs.espressif.com/projects/arduino-esp32/en/latest/guides/tools_menu.html)

Dieses Paket ist ein Veröffentlichungsentwurf. Vor der Veröffentlichung müssen noch Repository-Name, Eigentümerorganisation und Lizenz festgelegt werden.
