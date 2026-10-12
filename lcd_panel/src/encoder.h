/**
 * lcd_panel -- encoder decoder: pin-change ISR + fixed ring + validated state machine.
 *
 * The panel draws over SPI in long bursts (a full 240x320 fill is ~150 KB, tens of ms), so
 * POLLING the encoder in the main loop loses detents while the screen is being painted.
 * Here the two encoder pins interrupt on change, the ISR only pushes the 2-bit state into
 * a fixed ring (no heap, no STL -- unlike D:\summer-fan\src\encode2.cpp which allocates in
 * its ISR), and the main loop decodes with a transition table that COUNTS a real single-bit
 * step (±1) but IGNORES a 2-bit jump (contact bounce, or an edge we could not buffer),
 * resynchronising instead.
 */
#pragma once

#include <Arduino.h>

void enc_begin();          // configure the pins and attach the two interrupts
int8_t enc_detents();      // detents since the last call, clamped; 0 when nothing turned
