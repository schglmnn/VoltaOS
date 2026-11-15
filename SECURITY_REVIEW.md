# VoltaOS: gezielte Sicherheitsprüfung

Stand: 14.11.2026. Geprüft wurde Commit `2e573ec907dbcd765e69fa93716909166c00caee`
einschließlich unserer lokalen Änderungen. Gemeint ist das hier vorhandene VoltaOS,
nicht ein anderes Produkt namens „Voltera“.

## Einschätzung

**Derzeit keine uneingeschränkte Empfehlung für sensible Konten oder die Nutzung
als allgemeiner Webbrowser auf einem Rechner mit privaten Daten.** Im untersuchten
Quellcode wurde kein offensichtlicher absichtlich eingebauter API-Key-Stealer und
kein dediziertes Analytics-/Crash-Telemetrie-SDK identifiziert. Das ist kein Beweis
für Schadsoftwarefreiheit. Die gefundenen Schutzlücken können Datenverlust auch
ohne böswillige Absicht des Projektentwicklers ermöglichen.

Die wichtigsten Schutzlücken sind bereits im Git-Ausgangsstand vorhanden. Die
Karten-, GPS-, Auto- und Carlinkit-Korrekturen entfernen sie nicht.

## Befunde, nach Priorität

### 1. Hoch / P1: Browser-Schutzmaßnahmen global ausgeschaltet

Stelle: `src/main.cpp:363`, insbesondere Zeilen 368–370.

Alle eingebetteten WebEngine-Ansichten starten mit `--disable-web-security`,
`--allow-running-insecure-content` und `--no-sandbox`. Browser, Theater und
Anmeldeseiten laden externe Inhalte. Die erste Option schwächt die
Ursprungsisolierung; die zweite erlaubt unsichere eingebettete Inhalte; die dritte
entfernt eine wichtige Grenze zwischen einem kompromittierten Renderer und den
Rechten des angemeldeten Benutzers. Ein Renderer-Exploit hätte dadurch erheblich
größere Auswirkungen. Das bedeutet nicht, dass jedes gewöhnliche JavaScript
bereits beliebige Systembefehle ausführen kann.

Nachweis: Quellcode und Abgleich mit dem Git-Ausgangsstand; kein aktiver
Browser-Exploit durchgeführt. Qt dokumentiert ausdrücklich das Sicherheitsrisiko
von `--no-sandbox`:
https://doc.qt.io/qtforpython-6/overviews/qtwebengine-debugging.html

Abhilfe: Diese drei Schalter entfernen. Benötigte Ausnahmen eng auf eigene
interne Inhalte begrenzen; allgemeine Webseiten und Anmeldungen in einer
normal abgesicherten Browserumgebung betreiben.

### 2. Hoch / P1: Lokale Dienste besitzen keine belastbare Zugriffskontrolle

Stelle: `services/local_security.py:22`, insbesondere `protect()` ab Zeile 35.

Die gemeinsame Schutzfunktion prüft Browser-Header, aber weder `Host` noch einen
geheimen Sitzungstoken. Fehlender Origin und `Origin: null` werden akzeptiert,
sofern keine blockierende Fetch-Metadata vorhanden ist. Ein fremder Hostname
mit `Sec-Fetch-Site: same-origin` wird ebenfalls akzeptiert. Das ist eine
Voraussetzung für DNS-Rebinding-Angriffe. Ob eine konkrete Webseite dies gegen
einen bestimmten aktuellen Browser ausnutzen kann, hängt zusätzlich von dessen
Netzwerk-/DNS-Schutzmechanismen ab; ein vollständiger Browserangriff wurde nicht
getestet.

Unabhängig davon können andere lokale Prozesse und Benutzer die Loopback-Dienste
ohne Authentifizierung aufrufen. Die Dienste bieten u.a. WLAN-/Bluetooth-Aktionen,
Kontoinformationen und Wiedergabesteuerung. Einzelne Dienste ändern Zustand sogar
über GET. Loopback-Bindung verhindert direkte Zugriffe aus dem LAN, authentifiziert
aber nicht den Aufrufer auf diesem Rechner.

Isoliert reproduziert: normale fremde Origin → 403; fremder Hostname ohne Origin
mit `same-origin` → 200; `Origin: null` ohne Fetch-Metadata → 200. Auch der echte
Kartenstil-Handler gab mit einem **Dummy**-Stil und fremdem Host HTTP 200 zurück,
einschließlich des Dummy-MapTiler-Schlüssels in den Stil-URLs. Echte Schlüssel
wurden dabei weder ausgegeben noch übertragen. Ein browserseitiger MapTiler-Key
ist nicht mit einem Spotify-Client-Secret gleichzusetzen; unbefugte Nutzung kann
trotzdem Quoten und Kosten betreffen.

Abhilfe: Host strikt auf erwartete Loopback-Adressen begrenzen; zufällige,
prozessgebundene API-Tokens für privilegierte Endpunkte; Änderungen nur per POST
mit geeigneter CSRF-/Origin-Prüfung. Alternativ getrennte lokale IPC verwenden.

### 3. Mittel / P2: Spotify-OAuth ohne State-Bindung

Stelle: `services/spotify_scraper_service.py:194–206`.

`/login` erzeugt keinen erkennbar gespeicherten, zufälligen Sitzungs-State.
`/callback` tauscht jeden übergebenen `code` aus, ohne `state` zu validieren.
Damit fehlt der Schutz gegen Login-CSRF bzw. eine Zuordnung der Anmeldung zum
vom Nutzer begonnenen Vorgang. Das belegt keine direkte Übernahme des
Spotify-Kontos des Opfers.

Nachweis: Die Originalfunktionen wurden isoliert mit einem OAuth-Testobjekt
ausgeführt. `/callback?code=audit-fake-code&state=wrong` rief den Tokenaustausch
auf und meldete Erfolg. Keine echte Spotify-Anfrage.

Abhilfe: Zufälligen State pro Anmeldung speichern, beim Callback vergleichen,
danach verwerfen und ungültige Anfragen vor dem Tokenaustausch ablehnen. Für eine
Desktop-App ist zusätzlich PKCE vorzuziehen. Spotify beschreibt die State-Prüfung:
https://developer.spotify.com/documentation/web-api/tutorials/code-flow

### 4. Mittel / P2: Vorhersehbarer Widevine-Bibliothekspfad in `/tmp`

Stelle: `src/main.cpp:357–370`.

Eine bereits vorhandene `/tmp/libwidevinecdm.so` wird weder auf Eigentümer,
Symlinks noch Inhalt geprüft. Der Pfad wird anschließend Chromium als CDM-Pfad
übergeben. Ein anderer lokaler Benutzer könnte die Datei vorab platzieren. Wenn
der entsprechende CDM-Ladepfad aktiviert wird und die Bibliothek akzeptiert wird,
entsteht ein Risiko für eingeschleusten nativen Code mit den Rechten von VoltaOS.

Nachweis: unsicherer Dateipfad und fehlende Prüfung im Code. Keine manipulierte
Bibliothek platziert und kein Code-Execution-Nachweis durchgeführt.

Abhilfe: Originaldatei aus einem vertrauenswürdigen Installationsverzeichnis
verwenden oder in einem ausschließlich dem Benutzer zugänglichen Verzeichnis
sicher und atomar bereitstellen. Vorhandene fremde Dateien nicht übernehmen.

### 5. Mittel / P2: API-Schlüssel werden an Kindprozesse weitervererbt

Stellen: `src/main.cpp:87–108`, `src/core/SystemHelper.cpp:279`,
`services/ytmusic_scraper_service.py:355–358`.

Alle Einträge aus `keys.env` werden in die globale Prozessumgebung übernommen.
Viele Kindprozesse erben diese Umgebung ungefiltert; der yt-dlp-Pfad kopiert sie
sogar ausdrücklich vollständig. Dadurch erhalten Programme Schlüssel anderer,
für ihre Aufgabe nicht benötigter Dienste. Es gibt zusätzlich einen Mechanismus
zum Nachladen externer yt-dlp-JavaScript-Komponenten (`ejs:github`, Zeile 351).
Das ist eine zusätzliche Lieferketten-Vertrauensgrenze, kein Beweis für Schadcode.

Abhilfe: Pro Dienst nur benötigte Schlüssel übergeben; Browser, Spiele und
Hilfsprogramme ohne fremde Dienstgeheimnisse starten; Abhängigkeiten und
nachgeladene Komponenten nachvollziehbar versionieren.

## Weitere relevante Risiken

- **SSRF / unbeschränkter Icon-Abruf:** `services/map_proxy.py:1827–1832` erstellt
  aus einem frei angegebenen Host HTTPS-Anfragen. Private/Loopback-Adressen und
  Redirect-Ziele werden dort nicht ausgeschlossen; Antworten werden vollständig
  geladen. Das kann interne Netzwerkziele erreichen und Ressourcen verbrauchen.
  Eingaben, DNS-Ergebnis, Redirects, Zeit- und Größenlimits müssen geprüft werden.
  Keine aktive Anfrage an fremde interne Systeme getestet.
- **Native Android-Auto-Wireless-Schnittstelle:** Bei Auswahl dieses Modus hört
  `src/androidauto/AaClient.cpp:102` auf allen IPv4-Schnittstellen, Port 5288.
  `AaTls.cpp:29–30` setzt Sicherheitslevel 0 und deaktiviert Peer-Verifikation.
  Eine zusätzliche nachgewiesene Host-Authentifizierung ist in diesem Einstieg
  nicht erkennbar. Das ist keine generelle Abschaltung der HTTPS-Prüfung aller
  APIs. Dein Carlinkit-USB-Modus benötigt diesen TCP-Listener nicht.
- **Browser-Cookies im Klartext:** `SystemHelper.cpp:1165–1178` speichert auch
  vollständige Cookies in JSON und verlängert ursprünglich sitzungsgebundene
  Cookies auf 60 Tage. Dateimodus 0600 hilft gegen andere Benutzer, aber nicht
  gegen Schadcode im selben Benutzerkonto. Bewusste Logout-/Sitzungsgrenzen werden
  dadurch abgeschwächt. Native Browser-Persistenz und möglichst ein Secret Store
  sind vorzuziehen.
- **Update-/Lieferkette:** Downloads verwenden HTTPS; Größe und, sofern vorhanden,
  GitHub-SHA256-Digest werden geprüft. Eine verpflichtende unabhängige
  Herausgebersignatur ist nicht erkennbar. Fehlt der Digest, wird ohne Hashprüfung
  weitergearbeitet (`UpdateManager.cpp:210` ff.). Abhängigkeiten in
  `requirements.txt` haben Mindestversionen, aber keinen reproduzierbaren Lock mit
  Hashes. Das ist ein Härtungsbedarf, keine Feststellung eines kompromittierten
  Updates. Die unterschiedliche Schreibweise von Git-Remote und hartcodiertem
  Update-Repository wurde nicht als Angriff gewertet; eine mögliche Umbenennung
  bzw. Weiterleitung wurde nicht abschließend verifiziert.

## Tracker, Datenübertragung und API-Key-Stealer

In den untersuchten Übertragungspfaden wurden Schlüssel für ihren jeweiligen
Anbieter verwendet. Es wurde kein offensichtlicher zentraler Upload aller
Schlüssel an einen fremden Sammelserver gefunden. Die Erfassung von
YouTube-Anmelde-Cookies erfolgt für die lokale Musik-Anmeldung und landet laut
geprüftem Pfad beim lokalen Dienst auf Port 8099. Diese technisch sensible
Funktion ist für sich noch kein Nachweis eines Cookie-Stealers.

**Es gibt dennoch viele Übertragungen an Dritte, auch außerhalb der Hauptanbieter:**

| Funktion | Beispiele für Empfänger | Übertragene Daten im Code |
|---|---|---|
| Karten/Navigation | MapTiler, Mapbox, TomTom, Overpass-Instanzen | Kartenausschnitt/Region, Koordinaten oder Suche, anbieterbezogene API-Schlüssel |
| Wetter/Ortsname | Open-Meteo, BigDataCloud, RainViewer | Standort bzw. Region; WeatherPopup rundet teils auf 3–4 Nachkommastellen |
| Liedtexte | LRCLIB, `lyrics-api.boidu.dev`, Kugou, `lrc.red`, mehrere LyricsPlus-Instanzen | Titel, Künstler und teils Dauer/Track-Kennungen |
| Web-App-Icons | Website selbst und Google-Favicon-Dienst | Aufgerufener Hostname, IP-Adresse des Rechners |
| Medien | Spotify, Google/YouTube, Apple/iTunes, Radio-/Podcastanbieter | Authentifizierung sowie funktionsbezogene Such-/Medienanfragen |

Belegbeispiele: `qml/LyricsPanel.qml:441`, `qml/WeatherPopup.qml:110–151`,
`services/map_proxy.py:1829–1830`. Das sind funktionale Drittanbieterabrufe;
ob die Betreiber daraus Profile erstellen, lässt sich nicht aus VoltaOS ableiten.
Webseiten im eingebetteten Browser können zusätzlich ihre eigenen Tracker laden.
Es wurde kein vollständiger Trackingblocker identifiziert.

Der ntfy-Pfad zur Zielübermittlung ist vorhanden, aber `_ph_start()` ist am
Programmeinstieg auskommentiert. Er wurde deshalb nicht als aktive Telemetrie
eingestuft.

## Positive Feststellungen

- Die Python-HTTP-Dienste binden im geprüften Code an Loopback; kein allgemeines
  Flask-Debugging im Produktionsstart.
- Normale fremde Origin-/Fetch-Metadata werden bereits blockiert, wenn auch
  unzureichend als vollständige Authentifizierung.
- HTTPS-Zertifikatsprüfung ist standardmäßig aktiv. Die optionale Umgebungsvariable
  `VOLTA_SSL_INSECURE` würde sie im Qt-Netzwerkpfad deaktivieren; sie ist kein
  sicherer Betriebsmodus.
- `keys.env`, Spotify-Token und Browser-/Theater-Cookie-Dateien haben auf deinem
  Rechner aktuell Modus **0600**. Inhalte wurden nicht im Bericht ausgegeben.
- Dateiauslieferung verwendet an mehreren Stellen Flask `send_from_directory`;
  mehrere Systemaufrufe verwenden Argumentlisten statt zusammengesetzter Shells.

## Umfang und Grenzen

Geprüft: gezielte statische Durchsicht von Start-/WebEngine-Konfiguration,
Schlüssel-/Cookiepfaden, lokalen Diensten, OAuth, ausgewählten Systemaufrufen,
USB-/Wireless-Einstiegen und Updates; Inventar fest codierter URL-Hosts;
isolierte Flask-/OAuth-Tests; Prüfung tatsächlicher Dateirechte.

Die Tests nutzten Dummy-Daten und keine echten externen Konten. Es wurden weder
WLAN/Bluetooth umgestellt noch Cookies oder Tokens zurückgesetzt. Produktivcode
wurde im Rahmen dieses Reviews nicht verändert.

Nicht enthalten: vollständiges Zeile-für-Zeile-Audit aller Drittbibliotheken,
reproduzierbarer Binärvergleich des AppImages, vollständiges Reverse Engineering,
aktuelle CVE-Prüfung aller installierten Abhängigkeiten, Fuzzing von Medien-/USB-
Parsern, Netzwerkmitschnitt oder ein End-to-End-Browserexploit. Ein einfacher
Strings-Scan des AppImage-Executables zeigte keinen offensichtlichen neuen
Sammelserver, kann versteckte oder dynamische Ziele aber nicht ausschließen.
Die Dongle-Firmware selbst wurde nicht auditiert.

## Empfohlene Reihenfolge

1. Browser-Sandbox und Web-Sicherheit wieder aktivieren.
2. Lokale Dienste mit Hostprüfung, API-Authentifizierung und CSRF-Schutz härten.
3. Spotify-State/PKCE und den unsicheren `/tmp`-Bibliothekspfad korrigieren.
4. Schlüsselvererbung begrenzen; Cookies und Lieferkette härten.
5. Danach erneut testen und Abhängigkeiten auf bekannte Sicherheitslücken prüfen.

Bis dahin für Versuche bevorzugt einen separaten Ubuntu-Benutzer ohne weitere
private Konten verwenden. Sensible Anmeldungen im eingebetteten Browser würde ich
vor der Behebung der P1-Befunde vermeiden. Das ist eine Empfehlung für den konkret
geprüften Stand, kein Sicherheitszertifikat.
