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
 * - Sincronizzazione ciclica automatica senza delay bloccanti.
 * - API moderne: bridge.readBool(), bridge.write(), bridge.readInt(), ecc.
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
  int currentOutIndex;
  String rxBuffer;

public:
  TinkercadBridge() {
    tagCount = 0;
    lastSyncTime = 0;
    syncIntervalMs = 25; // 25ms tra un tag e l'altro (frequenza ottimale)
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
  void addOutput(const char* tagName, const char* initialValue = "false") {
    if (tagCount >= MAX_BRIDGE_TAGS) return;
    tags[tagCount].name = tagName;
    tags[tagCount].value = String(initialValue);
    tags[tagCount].type = TAG_OUTPUT;
    tags[tagCount].isChanged = false;
    tagCount++;
  }

  void addOutput(const char* tagName, bool initialValue) {
    addOutput(tagName, initialValue ? "true" : "false");
  }

  void addOutput(const char* tagName, int initialValue) {
    addOutput(tagName, String(initialValue).c_str());
  }

  void addOutput(const char* tagName, float initialValue, int decimals = 2) {
    addOutput(tagName, String(initialValue, decimals).c_str());
  }

  void addOutput(const char* tagName, double initialValue, int decimals = 2) {
    addOutput(tagName, String(initialValue, decimals).c_str());
  }

  /**
   * Registra un tag di INPUT (Factory I/O / CODESYS / SCADA -> Arduino)
   */
  void addInput(const char* tagName, const char* initialValue = "false") {
    if (tagCount >= MAX_BRIDGE_TAGS) return;
    tags[tagCount].name = tagName;
    tags[tagCount].value = String(initialValue);
    tags[tagCount].type = TAG_INPUT;
    tags[tagCount].isChanged = false;
    tagCount++;
  }

  void addInput(const char* tagName, bool initialValue) {
    addInput(tagName, initialValue ? "true" : "false");
  }

  void addInput(const char* tagName, int initialValue) {
    addInput(tagName, String(initialValue).c_str());
  }

  void addInput(const char* tagName, float initialValue, int decimals = 2) {
    addInput(tagName, String(initialValue, decimals).c_str());
  }

  /**
   * SCRITTURA VALORI (Arduino invia al Server industriale)
   */
  void write(const char* tagName, const String& val) {
    int idx = findTagIndex(tagName);
    if (idx >= 0) {
      if (tags[idx].value != val) {
        tags[idx].value = val;
        tags[idx].isChanged = true;
      }
    }
  }

  void write(const char* tagName, bool val) {
    write(tagName, val ? "true" : "false");
  }

  void write(const char* tagName, int val) {
    write(tagName, String(val));
  }

  void write(const char* tagName, float val, int decimals = 2) {
    write(tagName, String(val, decimals));
  }

  void write(const char* tagName, double val, int decimals = 2) {
    write(tagName, String(val, decimals));
  }

  /**
   * LETTURA VALORI (Arduino riceve da Factory I/O o PLC)
   */
  String readString(const char* tagName) {
    int idx = findTagIndex(tagName);
    if (idx >= 0) {
      return tags[idx].value;
    }
    return "";
  }

  bool readBool(const char* tagName) {
    String val = readString(tagName);
    val.toLowerCase();
    return (val == "true" || val == "high" || val == "1");
  }

  int readInt(const char* tagName) {
    return readString(tagName).toInt();
  }

  float readFloat(const char* tagName) {
    return readString(tagName).toFloat();
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

    // 2. TRASMISSIONE OUTPUT (Round-Robin non bloccante)
    if (millis() - lastSyncTime >= syncIntervalMs) {
      lastSyncTime = millis();

      if (tagCount > 0) {
        // Cerca il prossimo tag di OUTPUT da sincronizzare
        int attempts = 0;
        while (attempts < tagCount) {
          if (tags[currentOutIndex].type == TAG_OUTPUT) {
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
  int findTagIndex(const char* tagName) {
    for (int i = 0; i < tagCount; i++) {
      if (strcmp(tags[i].name, tagName) == 0) {
        return i;
      }
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
};

#endif // TINKERCAD_INDUSTRIAL_BRIDGE_H

