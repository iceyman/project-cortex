// ════════════════════════════════════════════════════════════
//  dance_music.h  —  drop into src/ alongside main.cpp
//  Chiptune dance party music via M5Stack speaker
//  Plays a fun 8-bit melody loop during dance party mode
// ════════════════════════════════════════════════════════════
#pragma once
#include <M5Unified.h>

// Note frequencies (Hz)
#define N_C4  262
#define N_D4  294
#define N_E4  330
#define N_F4  349
#define N_G4  392
#define N_A4  440
#define N_B4  494
#define N_C5  523
#define N_D5  587
#define N_E5  659
#define N_G5  784
#define N_A5  880
#define N_REST  0

// Note durations (ms at ~128 BPM)
#define Q   468   // quarter note
#define E   234   // eighth note
#define S   117   // sixteenth note
#define QD  702   // dotted quarter

struct Note { uint16_t freq; uint16_t ms; };

// Fun upbeat 8-bit dance tune — plays on loop
// Sounds like a happy video game level start
const Note DANCE_TUNE[] = {
  // Bar 1 — main hook
  {N_E5, E}, {N_E5, E}, {N_G5, E}, {N_E5, E},
  {N_D5, E}, {N_C5, Q}, {N_REST, E},
  // Bar 2
  {N_E5, E}, {N_E5, E}, {N_G5, E}, {N_A5, E},
  {N_G5, QD}, {N_REST, S},
  // Bar 3 — build
  {N_C5, E}, {N_D5, E}, {N_E5, E}, {N_G5, E},
  {N_A5, E}, {N_G5, E}, {N_E5, Q},
  // Bar 4 — answer
  {N_D5, E}, {N_C5, E}, {N_D5, E}, {N_E5, E},
  {N_C5, QD}, {N_REST, S},
  // Bar 5 — variation
  {N_G5, E}, {N_G5, E}, {N_A5, E}, {N_G5, E},
  {N_E5, E}, {N_D5, Q}, {N_REST, E},
  // Bar 6
  {N_G5, E}, {N_E5, E}, {N_D5, E}, {N_C5, E},
  {N_E5, QD}, {N_REST, S},
  // Bar 7 — run up
  {N_C5, S}, {N_D5, S}, {N_E5, S}, {N_G5, S},
  {N_A5, S}, {N_G5, S}, {N_E5, S}, {N_D5, S},
  {N_C5, Q}, {N_REST, Q},
  // Bar 8 — big finish before loop
  {N_C5, E}, {N_E5, E}, {N_G5, E}, {N_A5, E},
  {N_G5, Q}, {N_E5, E}, {N_REST, E},
};
const int DANCE_NOTE_COUNT = sizeof(DANCE_TUNE) / sizeof(Note);

// ─── Task vars ────────────────────────────────────────────
TaskHandle_t musicTask = nullptr;
volatile bool musicRunning = false;

void musicTaskFn(void*) {
  int idx = 0;
  while (musicRunning) {
    const Note& n = DANCE_TUNE[idx];
    if (n.freq > 0) {
      M5.Speaker.tone(n.freq, n.ms);
    }
    vTaskDelay(pdMS_TO_TICKS(n.ms + 20));  // tiny gap between notes
    idx = (idx + 1) % DANCE_NOTE_COUNT;    // loop forever
  }
  M5.Speaker.stop();
  musicTask = nullptr;
  vTaskDelete(nullptr);
}

void startDanceMusic() {
  if (musicTask) return;
  M5.Speaker.setVolume(220);  // loud and proud for dance party
  musicRunning = true;
  xTaskCreatePinnedToCore(musicTaskFn, "music", 2048, nullptr, 2, &musicTask, 0);
}

void stopDanceMusic() {
  musicRunning = false;
  vTaskDelay(pdMS_TO_TICKS(100));
  M5.Speaker.stop();
}
