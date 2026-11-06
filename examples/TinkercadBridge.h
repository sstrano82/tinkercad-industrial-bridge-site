// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Salvatore Strano
// Tinkercad Industrial Bridge - Header-Only Arduino Helper Library
// Progettato appositamente per Autodesk Tinkercad Circuits

#ifndef TINKERCAD_INDUSTRIAL_BRIDGE_H
#define TINKERCAD_INDUSTRIAL_BRIDGE_H

#include <Arduino.h>

/**
 * ============================================================================
 * CLASSE: TinkercadBridge
 * ============================================================================
 * Gestisce in modo trasparente la comunicazione bidirezionale tra lo sketch
 * Arduino simulato su Tinkercad e il companion server industriale (OPC UA / Modbus).
 *
 * CARATTERISTICHE:
 * - Creazione e registrazione automatica dei nodi (Input & Output).
 * - Lettura non bloccante via Seriale con parser integrato (terminatore '|').
 * - Sincronizzazione ciclica automatica con invio su variazione (isChanged).
 * - Force sync periodico ogni 5s per garantire l'allineamento perfetto con il server.
 * - Supporto completo ai tipi con overload dedicati (const char*, bool, int, float, double).
 */

#define MAX_BRIDGE_TAGS 16

class TinkercadBridge {
public:
  enum TagType { TAG_OUTPUT, TAG_INPUT };

  struct Tag {
    const char* name;
    String value;
    TagType type;
    bool isChanged;
  };

private:
  Tag tags[MAX_BRIDGE_TAGS];
  int tagCount;
  unsigned long lastSyncTime;
  unsigned long syncIntervalMs;
  unsigned long lastForceSyncTime;
  unsigned long forceSyncIntervalMs;
  int currentOutIndex;
  String rxBuffer;

public:
  TinkercadBridge() {
    tagCount = 0;
    lastSyncTime = 0;
    syncIntervalMs = 25;        // 25ms tra un tag e l'altro (frequenza ottimale)
    lastForceSyncTime = 0;
    forceSyncIntervalMs = 5000; // Ogni 5s rinvia forzatamente tutti gli output (safety net)
    currentOutIndex = 0;
    rxBuffer = "";
  }

  /**
   * Inizializza la comunicazione seriale e registra i nodi iniziali
   */
  void begin(unsigned long baudRate = 115200, unsigned long warmupMs = 1500) {
    Serial.begin(baudRate);
    
    // Attesa iniziale per consentire all'estensione browser di agganciare la console seriale
    delay(warmupMs);

    // Annuncio iniziale di tutti i tag registrati (creazione nodi sul server OPC UA / Modbus)
    for (int i = 0; i < tagCount; i++) {
      Serial.print(tags[i].name);
      Serial.print(":");
      Serial.println(tags[i].value);
      delay(20);
    }
  }

  /**
   * Registra un tag di OUTPUT (Arduino -> Factory I/O / CODESYS / SCADA)
   */
  void addOutput(const char* name, const char* val = "false") {
    if (tagCount >= MAX_BRIDGE_TAGS) return;
    tags[tagCount++] = { name, String(val), TAG_OUTPUT, false };
  }
  void addOutput(const char* name, bool val) { addOutput(name, val ? "true" : "false"); }
  void addOutput(const char* name, int val) { addOutput(name, String(val).c_str()); }
  void addOutput(const char* name, float val, int dec = 2) { addOutput(name, String(val, dec).c_str()); }
  void addOutput(const char* name, double val, int dec = 2) { addOutput(name, String(val, dec).c_str()); }

  /**
   * Registra un tag di INPUT (Factory I/O / CODESYS / SCADA -> Arduino)
   */
  void addInput(const char* name, const char* val = "false") {
    if (tagCount >= MAX_BRIDGE_TAGS) return;
    tags[tagCount++] = { name, String(val), TAG_INPUT, false };
  }
  void addInput(const char* name, bool val) { addInput(name, val ? "true" : "false"); }
  void addInput(const char* name, int val) { addInput(name, String(val).c_str()); }
  void addInput(const char* name, float val, int dec = 2) { addInput(name, String(val, dec).c_str()); }
  void addInput(const char* name, double val, int dec = 2) { addInput(name, String(val, dec).c_str()); }

  /**
   * SCRITTURA VALORI (Arduino invia al Server industriale)
   * Supporta costanti stringa, bool, int, float, double e oggetti String
   */
  void write(const char* name, const char* val) {
    int idx = findTagIndex(name);
    if (idx >= 0 && tags[idx].value != val) {
      tags[idx].value = val;
      tags[idx].isChanged = true;
    }
  }
  void write(const char* name, const String& val) { write(name, val.c_str()); }
  void write(const char* name, bool val) { write(name, val ? "true" : "false"); }
  void write(const char* name, int val) { write(name, String(val).c_str()); }
  void write(const char* name, float val, int dec = 2) { write(name, String(val, dec).c_str()); }
  void write(const char* name, double val, int dec = 2) { write(name, String(val, dec).c_str()); }

  /**
   * LETTURA VALORI (Arduino riceve da Factory I/O o PLC)
   */
  String readString(const char* name) {
    int idx = findTagIndex(name);
    return (idx >= 0) ? tags[idx].value : "";
  }

  bool readBool(const char* name) {
    String val = readString(name);
    val.toLowerCase();
    return (val == "true" || val == "high" || val == "1");
  }

  int readInt(const char* name) {
    return readString(name).toInt();
  }

  float readFloat(const char* name) {
    return readString(name).toFloat();
  }

  /**
   * AGGIORNAMENTO CICLICO (Da inserire nel loop principale)
   * Gestisce sia l'invio rotativo non-bloccante che la ricezione immediata di comandi.
   */
  void update() {
    // 1. RICEZIONE COMANDI IN INGRESSO (terminati da '|')
    while (Serial.available() > 0) {
      char ch = (char)Serial.read();
      if (ch != '|') {
        rxBuffer += ch;
      } else {
        processIncomingMessage(rxBuffer);
        rxBuffer = "";
      }
    }

    // 2. FORCE SYNC PERIODICO (safety net: rimarca tutti gli output come "da inviare")
    if (millis() - lastForceSyncTime >= forceSyncIntervalMs) {
      lastForceSyncTime = millis();
      markAllOutputChanged();
    }

    // 3. TRASMISSIONE OUTPUT (Round-Robin non bloccante, solo tag con isChanged == true)
    if (millis() - lastSyncTime >= syncIntervalMs) {
      lastSyncTime = millis();

      if (tagCount > 0) {
        int attempts = 0;
        while (attempts < tagCount) {
          if (tags[currentOutIndex].type == TAG_OUTPUT && tags[currentOutIndex].isChanged) {
            Serial.print(tags[currentOutIndex].name);
            Serial.print(":");
            Serial.println(tags[currentOutIndex].value);
            tags[currentOutIndex].isChanged = false;
            currentOutIndex = (currentOutIndex + 1) % tagCount;
            break;
          }
          currentOutIndex = (currentOutIndex + 1) % tagCount;
          attempts++;
        }
      }
    }
  }

private:
  int findTagIndex(const char* name) {
    for (int i = 0; i < tagCount; i++) {
      if (strcmp(tags[i].name, name) == 0) return i;
    }
    return -1;
  }

  void processIncomingMessage(const String& msg) {
    int colonIdx = msg.indexOf(':');
    if (colonIdx > 0) {
      String name = msg.substring(0, colonIdx);
      String val = msg.substring(colonIdx + 1);
      name.trim();
      val.trim();

      int idx = findTagIndex(name.c_str());
      if (idx >= 0) {
        tags[idx].value = val;
      }
    }
  }

  void markAllOutputChanged() {
    for (int i = 0; i < tagCount; i++) {
      if (tags[i].type == TAG_OUTPUT) {
        tags[i].isChanged = true;
      }
    }
  }
};

#endif // TINKERCAD_INDUSTRIAL_BRIDGE_H
