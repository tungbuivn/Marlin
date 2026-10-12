/**
 * encoder_quad.cpp -- quadrature decoder for the LCD encoder wheel (see encoder_quad.h).
 */
#include "encoder_quad.h"

#if ENABLED(ENCODER_QUAD_ISR) && HAS_ENCODER_WHEEL && BUTTONS_EXIST(EN1, EN2)

  // ---------------------------------------------------------------------------
  // Transition table, indexed by (previous_state << 2) | new_state, where the state is
  // (A << 1) | B. Polarity is IDENTICAL to Marlin's old polling table (with
  // encoderDirection = +1, i.e. REVERSE_ENCODER_DIRECTION off), so turning the knob feels
  // exactly the same as before this change.
  //
  //   +1 / -1 : a single bit changed -> a REAL quadrature edge, count it
  //    0      : no change, or BOTH bits changed at once (bounce, or an edge that arrived
  //             while the ring was full) -> NOT counted, the decoder just resynchronises.
  //
  // (Marlin's old table also ignored 2-bit jumps -- it only spun on the two valid
  // neighbours of each new state. What it could NOT do is avoid LOSING edges: it sampled
  // the pins by polling, so a long blocking operation ate steps.)
  // ---------------------------------------------------------------------------
  static const int8_t quad_table[16] = {
     0, -1, +1,  0,     // prev = 00
    +1,  0,  0, -1,     // prev = 01
    -1,  0,  0, +1,     // prev = 10
     0, +1, -1,  0      // prev = 11
  };

  static volatile uint8_t quad_ring[QUAD_RING_SIZE];
  static volatile uint8_t quad_head = 0, quad_tail = 0;
  static uint8_t quad_last = 0xFF;          // 0xFF = "no reference sample yet"
  static int16_t quad_pulses = 0;           // accumulated since the last read-out

  // Runs in interrupt context: read the two pins, push the 2-bit state, nothing else.
  static void quad_isr() {
    const uint8_t s = (uint8_t)((READ(BTN_EN1) << 1) | READ(BTN_EN2));
    const uint8_t next = (uint8_t)((quad_head + 1) & (QUAD_RING_SIZE - 1));
    if (next != quad_tail) {                // drop the sample if the ring is full
      quad_ring[quad_head] = s;
      quad_head = next;
    }
  }

  void quad_encoder_init() {
    SET_INPUT_PULLUP(BTN_EN1);
    SET_INPUT_PULLUP(BTN_EN2);

    quad_head = quad_tail = 0;
    quad_pulses = 0;
    quad_last = (uint8_t)((READ(BTN_EN1) << 1) | READ(BTN_EN2));  // seed the reference

    attachInterrupt(BTN_EN1, quad_isr, CHANGE);
    attachInterrupt(BTN_EN2, quad_isr, CHANGE);
  }

  int8_t quad_encoder_pulses() {
    // Snapshot the head: the ISR may keep appending, the next call picks those up.
    const uint8_t head = quad_head;
    while (quad_tail != head) {
      const uint8_t s = quad_ring[quad_tail];
      quad_tail = (uint8_t)((quad_tail + 1) & (QUAD_RING_SIZE - 1));
      if (quad_last != 0xFF)
        quad_pulses += quad_table[((quad_last & 3) << 2) | (s & 3)];
      quad_last = s & 3;
    }

    // Clamp: one UI update can never need more than this, and clamping stops a runaway
    // (e.g. after the ring overflowed during a long blocking operation).
    if (quad_pulses >  64) quad_pulses =  64;
    if (quad_pulses < -64) quad_pulses = -64;

    const int8_t out = (int8_t)quad_pulses;
    quad_pulses = 0;
    return out;
  }

#endif // ENCODER_QUAD_ISR && HAS_ENCODER_WHEEL && BUTTONS_EXIST(EN1, EN2)
