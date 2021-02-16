# BeltWinder
GW60 Superrollo Gurtwickler ESP8266 Programm mit WiFi/MQTT
Version: 0.1

Inspiriert und basierend auf dem Programm von ManuA:
https://gitlab.com/ManuA/GW60-Superrollo

Ausgelegt für die Platine von Papa Romeo (https://www.hempel-online.de/umbau-superrollo-gw60-mit-esp8266/articles/umbau-superrollo-gw60-mit-esp8266.html)

Wann, und ob, ich etwas von der ToDo Liste abarbeite entscheidet Lust und Laune.
Hilfe ist natürlich gern gesehen. Da ich kein Informatiker bin, sondern das ganze nur als Hobby betreibe, lerne ich auf diese Weise gerne dazu.



# Features:

**Verhalten:**
* Es reicht die .bin Datei auf den ESP zu flashen.
* Nach erstmaliger Gerätekonfiguration und WLAN Verbindung:
    * Minimal notwendige Topics werden erstellt.
    * Kalibrierungsfahrt muss gestartet werden: einmalig notwendig
    * Position Topic wird erstellt und aktiviert
* Nach reboot:
    * letzte bekannte Position wird übermittelt. Postion ist unsicher
    * Falls zuerst ein Prozentwert ungleich 0/100 angefahren werden soll, findet eine Positionierungsfahrt statt [kürzeste Strecke zwischen vermuteter Position und gewünschter Position, mit Halt an einem Anschlag]. Postion ist bis zum nächsten reboot sicher.

**MQTT Anbindung:**
* /alive: Status des Gerätes durch lastWill. [true/false -read only]
* /calibration: Führt eine Kalibrierungsfahrt durch und speichert Wert in EEPROM [true/false(button)]
* /state: Richtung [Inaktiv/Up/Down -read only ]
* /info: wichtige Meldungen werden hier augegeben [read only]
* /open: Fahre nach oben [true/false(button)]
* /close: Fahre nach unten [true/false(button)]
* /pause: Pausiere normale Fahrt [true/false(button)]
* /position: aktuelle/gewünschte Position in %. Aktiv nach Kalibrierung [0-100]
* /count: letzter bekannter Impulszählerwert. Wird beim Start abgerufen und als Ausgangswert genutzt (EEPROM Schonung)[read only]

**IotWebserver:**
* Start, ohne Konfiguration: Baut Access Point [name:"GW60-ESP", pw:"GW60-ESP"] auf. Gerätekonfiguration [192.168.4.1].
* Start, mit Konfiguration, D0 high, mit Wartezeit: Access Point für konfigurierte Zeit, dann Verbindung zu WLAN.
* Start, mit Konfiguration, D0 high, Wartezeit=0. Mqtt Verbindung in ~10s. Gerätekonfiguration [ip:admin:gesetztesPW]
* Start, mit Konfiguration, D0 low: Acces Point mit Standartpasswort bis Verbindung.

# To-Do's
**Todo für V1.0:**
* reedme verschönern...
* Zusatzinfo für Betrieb mit geschalteter Stromversorgung
* MQTT Port im IotWebServer konfigurierbar
* Code cleanup - Aufteilung in main.cpp, webServer.cpp, mqttStuff.cpp, programMagic.cpp, otherMagic.cpp
* Code kommentieren
* Englisch Übersetzung: Kommentare + reedme
* Code anpassen für ESP32 (hauptsächlich iotWebServer)

**Ideen nach V1.0:**
* Programm universeller gestalten. Unabhängig von der Platine.
* Unterstützung anderer Sensoren anstelle des Impulszählers. Bsp.: Entfernungsmesser
* Verschiene Modi: Gurtwickler/Rollo/Jalousie(Schrägstellung)

**Persönliche Ziele nach V1.0:**
* Nutzung des ESP32 für BLE(RSSI) Entfernungsmessung. Bsp: Handy, Smartwatch, Beacon
* Da die Gurtwickler günstig verteilt sind: Indoor BLE-Ortung durch Vergleich der einzelnen RSSI Werte. Zumindest Raumgenau...

