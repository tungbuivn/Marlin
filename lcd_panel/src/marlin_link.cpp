/**
 * lcd_panel -- link to Marlin (implementation). See marlin_link.h for the protocol.
 */
#include "marlin_link.h"

// ---------------------------------------------------------------------------
// tiny float parser: "210.34" -> 210.34. Avoids pulling in the libc float scanner
// (matters on the 32 KB F103C6 build).
// ---------------------------------------------------------------------------
float MarlinLink::parseFloat(const char *s) {
  while (*s == ' ') s++;
  bool neg = false;
  if (*s == '-' || *s == '+') { neg = (*s == '-'); s++; }
  float v = 0;
  while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
  if (*s == '.') {
    s++;
    float scale = 0.1f;
    while (*s >= '0' && *s <= '9') { v += (*s++ - '0') * scale; scale *= 0.1f; }
  }
  return neg ? -v : v;
}

bool MarlinLink::tokenFloat(const char *after, float &out) {
  if (!after) return false;
  if (*after != ':' && *after != ' ') return false;
  out = parseFloat(after + 1);
  return true;
}

// Look for "<key>:" anywhere in the line (e.g. 'T' in "ok T:210.3 /210.0 B:60.1")
bool MarlinLink::findValue(const char *line, const char *key, float &out) {
  const char *p = line;
  while ((p = strstr(p, key)) != nullptr) {
    // the character before the key must not be alphanumeric ("B:" must not match "B@:")
    if ((p == line || p[-1] == ' ' || p[-1] == ':') && p[1] == ':') {
      // ...and the value must be a number, not a range like "T:210.3 /210.0"
      out = parseFloat(p + 2);
      return true;
    }
    p++;
  }
  return false;
}

// ---------------------------------------------------------------------------
void MarlinLink::begin() {
  PANEL_MARLIN_SERIAL.begin(PANEL_MARLIN_BAUD);
  _last_ok_ms = millis();
}

bool MarlinLink::send(const char *cmd) {
  if (_tx_count >= 4) return false;
  strncpy(_tx[_tx_head], cmd, sizeof(_tx[0]) - 1);
  _tx[_tx_head][sizeof(_tx[0]) - 1] = '\0';
  _tx_head = (_tx_head + 1) & 3;
  _tx_count++;
  return true;
}

bool MarlinLink::send(const __FlashStringHelper *cmd) {
  char buf[40];
  strncpy_P(buf, (PGM_P)cmd, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  return send(buf);
}

void MarlinLink::push(const char *cmd) {
  DBGLN(cmd);
  PANEL_MARLIN_SERIAL.print(cmd);
  PANEL_MARLIN_SERIAL.print('\n');
}

// ---------------------------------------------------------------------------
// Parse one complete line from Marlin
// ---------------------------------------------------------------------------
void MarlinLink::handleLine(char *line) {
  const uint32_t now = millis();
  DBG("<- "); DBGLN(line);

  if (!strncmp(line, "ok", 2)) {
    _st.link_ok = true;
    _last_ok_ms = now;
    if (line[2] == '\0') return;              // plain "ok" -- nothing else to parse
  }

  if (!strncmp(line, "echo:", 5)) {
    strncpy(_st.message, line + 5, PANEL_MSG_LEN - 1);
    _st.message[PANEL_MSG_LEN - 1] = '\0';
    return;
  }
  if (!strncmp(line, "Error:", 6) || !strncmp(line, "!!", 2)) {
    strncpy(_st.message, line + (line[0] == '!' ? 2 : 6), PANEL_MSG_LEN - 1);
    _st.message[PANEL_MSG_LEN - 1] = '\0';
    return;
  }
  if (!strncmp(line, "FIRMWARE_NAME:", 14)) {
    strncpy(_st.firmware, line + 14, sizeof(_st.firmware) - 1);
    _st.firmware[sizeof(_st.firmware) - 1] = '\0';
    return;
  }

  // Temperatures: "T:210.34 /210.00 B:60.12 /60.00"
  float v;
  if (findValue(line, "T", v)) {
    _st.nozzle = v;
    const char *slash = strchr(line, '/');
    if (slash) _st.nozzle_target = parseFloat(slash + 1);
  }
  if (findValue(line, "B", v)) {
    _st.bed = v;
    const char *slash = strchr(line, '/');
    // the bed target is the second "/" of the line
    if (slash && (slash = strchr(slash + 1, '/')) != nullptr) _st.bed_target = parseFloat(slash + 1);
  }

  // Position: "X:152.30 Y:152.30 Z:10.00 E:0.00"
  if (findValue(line, "X", v)) _st.x = v;
  if (findValue(line, "Y", v)) _st.y = v;
  if (findValue(line, "Z", v)) _st.z = v;

  // SD progress: "SD printing byte 12345/678900"
  const char *sd = strstr(line, "SD printing byte ");
  if (sd) {
    const char *slash = strchr(sd, '/');
    if (slash) {
      const uint32_t done = (uint32_t)parseFloat(sd + 17);
      const uint32_t total = (uint32_t)parseFloat(slash + 1);
      _st.progress = total ? (uint8_t)((uint64_t)done * 100 / total) : 0;
      _st.printing = true;
    }
  }
  else if (!strncmp(line, "Not SD printing", 15)) {
    _st.printing = false;
    _st.paused = false;
    _st.progress = 0;
  }
  else if (!strncmp(line, "Done printing file", 18)) {
    _st.printing = false;
    _st.paused = false;
    _st.progress = 100;
    strncpy(_st.message, "in xong", PANEL_MSG_LEN - 1);
  }
  else if (!strncmp(line, "echo:busy: paused", 17)) {
    _st.paused = true;
  }
}

// ---------------------------------------------------------------------------
void MarlinLink::loop() {
  // --- queue -> Marlin ---
  while (_tx_count) {
    push(_tx[_tx_tail]);
    _tx_tail = (_tx_tail + 1) & 3;
    _tx_count--;
  }

  // --- Marlin -> us ---
  while (PANEL_MARLIN_SERIAL.available()) {
    const char c = (char)PANEL_MARLIN_SERIAL.read();
    if (c == '\r') continue;
    if (c == '\n') {
      _rx[_rx_len] = '\0';
      if (_rx_len) handleLine(_rx);
      _rx_len = 0;
      continue;
    }
    if (_rx_len < sizeof(_rx) - 1) _rx[_rx_len++] = c;
  }

  // --- periodic polls ---
  const uint32_t now = millis();
  if (!_m115_sent) { send(F("M115")); _m115_sent = true; }
  if (now - _last_temp >= POLL_TEMP_MS) { send(F("M105")); _last_temp = now; }
  if (now - _last_pos  >= POLL_POS_MS)  { send(F("M114")); _last_pos  = now; }
  if (_st.printing && now - _last_prog >= POLL_PROGRESS_MS) { send(F("M27")); _last_prog = now; }

  if (now - _last_ok_ms > LINK_TIMEOUT_MS) _st.link_ok = false;
}
