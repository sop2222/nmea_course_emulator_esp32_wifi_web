# nmea_course_emulator_esp32_wifi_web

A gyrocompass NMEA sentence emulator running on an **Air-m2m Core ESP32-C3**
(or any ESP32-C3 board).
The device continuously transmits `$HEHDT` true-heading sentences over a
hardware UART at 9600 baud 8N1, exactly as a real RS-422 gyrocompass would.
A built-in Wi-Fi access point serves a browser-based compass-knob interface
that lets you build a custom 125-sentence heading sequence on the fly — no
reflashing, no serial terminal, just a phone or laptop browser.

---

## Features

- **Always-on NMEA output** — `$HEHDT` sentences at 100 ms intervals, 9600 baud 8N1
- **Default sequence** loaded from flash on every boot (128 entries, ~330 ° oscillation)
Warning! The default hardcoded sentences have no checksum ($HEHDT,328.9,T\r\n), while dynamically generated ones now do ($HEHDT,xxx.x,T*XX\r\n). NMEA-0183 parsers are required to accept sentences either way — checksum is optional unless the talker always sends it, in which case the receiver may choose to reject sentences without one. Since you tested both paths and they worked, your receiving device is lenient, so this inconsistency is harmless in practice. Just something to be aware of if you ever switch receivers. Either deal with it, because author needs to tect both checksum and no checksum, or you can just remove `activate_default()` function and its array of sentences alltogeter. This data, by the way is legacy set from back when we coded by hands and I had to generate those numbers from a sine function in Scilab (MatLab alternative) with a script in a peculiar language.
- **Wi-Fi AP** (`NMEA-EMU` / `nmea1234`) active from the first second of boot
- **Web compass knob** at `http://192.168.4.1` — drag the needle, tap *Add*, repeat 125 times; the new sequence is live the moment you hit entry 125
- **Drift** Additional linear function or incremental steps in dicreet sense
Drift unchecked (`default`): every "Add" records exactly where the needle is — nothing changes.
Drift checked: after each "Add", the knob automatically rotates by the drift-adjust value, so the next tap adds (current + delta). The slider remembers its value between taps.
Drift adjust slider goes from −2.0° to +2.0° in 0.1° steps, defaults to +1.0°. The live ±X.X° readout updates as you move it.
Unchecking drift at any point stops the auto-advance immediately; the needle stays wherever it landed.
- **The function page** Additional link at the bottom of the main page with a knob
Centre heading comes from the knob current position, editable at any time
`Sine` — starts at centre, rises to centre + amp, falls back, ends at centre (one complete cycle per period)
`Sawtooth` — ramps linearly from centre − amp to centre + amp per period, then resets
The preview canvas shows the normalised shape (−1 to +1), so the waveform is always clearly visible regardless of amplitude setting
- **Common behavior for both methods** what happens in case:
0° / 360° wrapping is handled correctly in both directions
On success the form hides and a confirmation appears
On failure the button re-enables
- **Sequence wrap LED blink** — onboard LED (GPIO 12) gives a brief 50 ms pulse every time the sentence array cycles back to entry 0, providing a silent visual heartbeat without interrupting transmission

---

## Hardware

| Item | Notes |
|------|-------|
| Air-m2m Core ESP32-C3 | Any ESP32-C3 Arduino board works; adjust pin defines if needed |
| USB-TTL dongle (CH340, CP2102 …) or RS-232 / RS-422 converter | Connects to GPIO 4 on the ESP32 side |

---

## Wiring

The emulator outputs TTL-level serial on **GPIO 4**.
Connect it to the **RX** pin of your USB-TTL dongle or the TTL input of your
level converter.  Only two wires are required.

```
  Air-m2m Core ESP32-C3              USB-TTL dongle / level converter
 ┌─────────────────────┐            ┌──────────────────────────────┐
 │                     │            │                              │
 │   GPIO 4  (NMEA TX) ├────────────┤ RX                           │
 │                     │            │                              │
 │               3V3   │  (no conn) │                              │
 │                     │            │                              │
 │               GND   ├────────────┤ GND                          │
 │                     │            │                              │
 └─────────────────────┘            └──────────┬───────────────────┘
         USB-C                                 │ USB
     (power + flash)                        to PC / MFD
```

> **GPIO 5** is defined as `NMEA_UART_RX_PIN` in the source but is not used —
> the emulator is transmit-only.  You do not need to wire it.

### RS-422 variant

If your receiving device expects RS-422 differential levels (e.g. an actual
chart plotter gyro port), insert a TTL → RS-422 converter between GPIO 4 and
the balanced pair:

```
  GPIO 4 (TX) ──→ TTL-IN of MAX485 / SN75176
  GND         ──→ GND of converter
               converter A/B outputs → RS-422 line
```

---

## LED indicator

| LED state | Meaning |
|-----------|---------|
| 50 ms blink | The active sentence array just wrapped around to entry 0.  At the default 100 ms interval this happens every **12.8 s** (128-sentence default sequence) or every **12.5 s** (125-sentence custom sequence). |
| Off | Normal transmission in progress |

---

## Web interface

1. Power the board.
2. On any Wi-Fi device join the network **`NMEA-EMU`**, password **`nmea1234`**.
3. Open **`http://192.168.4.1`** in a browser.
4. Drag the compass needle to the desired heading → tap **Add to sequence**.
5. Repeat until the counter reaches **125 / 125**.
6. The page sends the full sequence to the ESP32 automatically and confirms
   with *"Array updated — 125 sentences loaded"*.
   The new sequence is active immediately; the default is restored on next reboot.

The web UI works on mobile (touch-drag supported) and desktop browsers.

---

## Building and flashing

Requires [PlatformIO](https://platformio.org/).

```bash
# build
pio run

# flash  (Linux — adjust /dev/ttyACM0 to your port)
pio run -t upload --upload-port /dev/ttyACM0

# flash  (Windows — adjust COMx to your port, e.g. COM3)
pio run -t upload --upload-port COM3

# open serial monitor (optional, 115200 baud)
pio device monitor
```

The serial monitor prints the AP IP, sentence count, and a confirmation line
each time a custom sequence is loaded.

---

## Project structure

```
esp32c3_nmea_emu/
├── platformio.ini        # board: airm2m_core_esp32c3, framework: arduino
├── src/
│   ├── main.cpp          # NMEA transmit loop, Wi-Fi AP, HTTP handlers
│   └── web_page.h        # Self-contained HTML/CSS/JS page (human-readable)
└── input_files/          # Reference sentence logs from the original PC emulator
```

---

## Sentence format

Each sentence follows IEC 61162-1 §4 framing:

```
$HEHDT,<heading>,T\r\n
```

Example: `$HEHDT,285.3,T\r\n` — true heading 285.3 °, no checksum

---

## License

MIT
=======
