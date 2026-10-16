/**
 * cydlcd -- encoder decoder (see encoder.h for why it is interrupt-driven).
 */
#include "encoder.h"
#include "cydlcd_config.h"

static constexpr uint8_t ENC_RING = 32;                 // 2-bit samples, power of two

static volatile uint8_t enc_ring[ENC_RING];
static volatile uint8_t enc_head = 0, enc_tail = 0;

static uint8_t enc_last = 0xFF;                         // 0xFF = no reference sample yet
static int16_t enc_pulses = 0;                          // residual quadrature pulses
static int16_t enc_residual = 0;                        // pulses not yet a full detent

// Index = (previous_state << 2) | new_state, with state = (A << 1) | B.
//   +1 / -1 : one bit changed  -> a real edge, count it
//    0      : no change, or BOTH bits changed at once (bounce / unbuffered edge)
//             -> NOT counted, just resynchronise
// Polarity is the same as the old polling table this file replaces (and as Marlin's), so
// ENC_REVERSE keeps exactly the meaning it had before.
static const int8_t enc_table[16] = {
   0, -1, +1,  0,
  +1,  0,  0, -1,
  -1,  0,  0, +1,
   0, +1, -1,  0
};

// Interrupt context: read the pins, push the state, nothing else.
static void enc_isr() {
  const uint8_t s = (uint8_t)((digitalRead(ENC_PIN_A) << 1) | digitalRead(ENC_PIN_B));
  const uint8_t next = (uint8_t)((enc_head + 1) & (ENC_RING - 1));
  if (next != enc_tail) {                               // ring full -> drop the sample
    enc_ring[enc_head] = s;
    enc_head = next;
  }
}

void enc_begin() {
  pinMode(ENC_PIN_A, INPUT_PULLUP);
  pinMode(ENC_PIN_B, INPUT_PULLUP);

  enc_head = enc_tail = 0;
  enc_pulses = enc_residual = 0;
  enc_last = (uint8_t)((digitalRead(ENC_PIN_A) << 1) | digitalRead(ENC_PIN_B));

  attachInterrupt(digitalPinToInterrupt(ENC_PIN_A), enc_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_PIN_B), enc_isr, CHANGE);
}

int8_t enc_detents() {
  // Snapshot the head so a burst arriving during decoding is handled next time.
  const uint8_t head = enc_head;
  while (enc_tail != head) {
    const uint8_t s = enc_ring[enc_tail];
    enc_tail = (uint8_t)((enc_tail + 1) & (ENC_RING - 1));
    if (enc_last != 0xFF)
      enc_pulses += enc_table[((enc_last & 3) << 2) | (s & 3)];
    enc_last = s & 3;
  }

  enc_residual += enc_pulses;
  enc_pulses = 0;

  int8_t detents = 0;
  while (enc_residual >= ENC_PULSES_PER_DETENT)  { enc_residual -= ENC_PULSES_PER_DETENT; detents++; }
  while (enc_residual <= -ENC_PULSES_PER_DETENT) { enc_residual += ENC_PULSES_PER_DETENT; detents--; }

  // Coalesce a fast spin (like summer-fan's "skip fast encoder"): the UI cannot use more
  //than a couple of detents per frame, and dropping the rest stops the value running away.
  if (detents > ENC_MAX_DETENTS || detents < -ENC_MAX_DETENTS) {
    detents = detents > 0 ? ENC_MAX_DETENTS : -ENC_MAX_DETENTS;
    enc_residual = 0;
  }

  return ENC_REVERSE ? -detents : detents;
}
