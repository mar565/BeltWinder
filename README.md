# BeltWinder
GW60 Superrollo Gurtwickler ESP8266 Firmware mit WiFi/MQTT
Version: 0.1

Inspiriert und basierend auf dem Programm von ManuA:
https://gitlab.com/ManuA/GW60-Superrollo

Ausgelegt für die Platine von Papa Romeo (https://www.hempel-online.de/umbau-superrollo-gw60-mit-esp8266/articles/umbau-superrollo-gw60-mit-esp8266.html)


  Features:
  -MQTT Anbindung:
    -/alive: Status des Gerätes durch lastWill. [true/false -read only]
    -/state: Richtung [Inaktiv/Up/Down -read only ]
    -/info: wichtige Meldungen werden hier angegeben [read only]
    -/up: Fahre nach oben [true/false(button)]
    -/down: Fahre nach unten [true/false(button)]
    -/pause: Pausiere normale Fahrt [true/false(button)]
    -/calibration: Führt eine Kalibrierungsfahrt durch und speichert Wert in EEPROM [true/false(button)]
    -/position: aktuelle/gewünschte Position in %. Aktiv nach Kalibrierung [0-100]
    -/count: letzter bekannter Impulszählerwert. Wird bei Start abgerufen und als Ausgangswert genutzt (EEPROM Schonung)[read only]

  -IotWebserver:
    -Start, ohne Konfiguration: Baut Access Point [name:"GW-60", pw:"GW-60"] auf. Gerätekonfiguration [192.168.4.1].
    -Start, mit Konfiguration, D0 high, mit Wartezeit: Access Point für konfigurierte Zeit, dann Verbindung zu WLAN.
    -Start, mit Konfiguration, D0 high, Wartezeit=0. Mqtt Verbindung in ~10s
    -Start, mit Konfiguration, D0 low: Acces Point mit Standartpasswort bis Verbindung.


  -Verhalten.
    -Nach erstmaliger Gerätekonfiguration und WLAN Verbindung:
      -Minimal notwendige Topics werden erstellt.
      -Kalibrierungsfahrt muss gestartet werden: einmalig notwendig
      -Position Topic wird erstellt und aktiviert
    -Nach reboot:
      -letzte bekannte Position wird übermittelt. Postion ist unsicher
      -Falls zuerst ein Prozentwert ungleich 0/100 angefahren werden soll, findet eine Positionierungsfahrt statt [kürzeste Strecke zwischen vermuteter Position und gewünschter Position, mit Halt an einem Anschlag]. Postion ist bis zum nächsten reboot sicher.
    
    
    Todo für V1.0:
    -reedme verschönern...
    -Zusatzinfo für Betrieb mit geschalteter Stromversorgung
    -MQTT Port im IotWebServer konfigurierbar
    -Code cleanup - Aufteilung in main.cpp, webServer.cpp, mqttStuff.cpp, programMagic.cpp, otherMagic.cpp
    -Code kommentieren
    -Englisch Übersetzung: Kommentare + reedme
    -Code anpassen für ESP32 (hauptsächlich iotWebServer)
    
    Ideen nach V1.0:
    -Programm universeller gestalten. Unabhängig von der Platine.
    -Unterstützung anderer Sensoren anstelle des Impulszählers. Bsp.: Entfernungsmesser
    -Erschiene Modi: Gurtwickler/Rollo/Jalousie(Schrägstellung)
    
    Persönliche Ziele nach V1.0:
    -Nutzung des ESP32 für BLE(RSSI) Entfernungsmessung. Bsp: Handy,Smartwatch,Beacon
    -Da die Gurtwickler günstig verteilt sind: Indoor BLE-Ortung durch Vergleich der einzelnen RSSI Werte. Zumindest Raumgenau...

