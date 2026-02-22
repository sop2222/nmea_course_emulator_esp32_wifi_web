#pragma once

/*
 * func_page.h
 *
 * HTML served at http://192.168.4.1/addfunction
 *
 * The page receives the current compass heading as the URL query parameter
 * "h" (e.g. /addfunction?h=285.3) and uses it as the centre heading for
 * the oscillation.
 *
 * Controls:
 *   - Centre heading  : editable number field, pre-filled from the URL param
 *   - Waveform        : sine or sawtooth drop-down
 *   - Periods         : 1 – 10 (how many complete cycles across 125 entries)
 *   - Amplitude       : 0.1 – 5.0 degrees peak deviation from centre
 *
 * A small canvas below the controls shows the shape of the selected waveform
 * and updates live as sliders are moved.
 *
 * The Generate button computes 125 heading values and POSTs them to /update
 * in the same format as the main page, then shows a confirmation.
 */

static const char FUNC_PAGE[] = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>NMEA Function Generator</title>
  <style>
    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      padding: 24px 16px;
      font-family: sans-serif;
      background: #0d1117;
      color: #c9d1d9;
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    h2 {
      margin: 0 0 4px;
      color: #58a6ff;
      font-size: 1.3em;
    }

    #subtitle {
      color: #8b949e;
      font-size: 0.85em;
      margin: 0 0 18px;
      text-align: center;
    }

    /* ---- shared field container ---- */
    .field {
      width: 240px;
      margin-bottom: 16px;
    }

    /* plain label above a control */
    .field > label {
      display: block;
      font-size: 0.8em;
      color: #8b949e;
      margin-bottom: 5px;
    }

    /* label + live value side by side (for sliders) */
    .slider-header {
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      margin-bottom: 5px;
    }

    .slider-header label {
      font-size: 0.8em;
      color: #8b949e;
    }

    .slider-val {
      font-size: 0.88em;
      color: #58a6ff;
      font-family: monospace;
    }

    /* ---- centre heading number input ---- */
    .field-row {
      display: flex;
      align-items: center;
      gap: 6px;
    }

    input[type="number"] {
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 4px;
      color: #e6edf3;
      padding: 7px 10px;
      font-size: 1.1em;
      width: 110px;
      text-align: right;
    }

    .unit {
      color: #8b949e;
      font-size: 1.1em;
    }

    /* ---- waveform drop-down ---- */
    select {
      width: 100%;
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 4px;
      color: #e6edf3;
      padding: 8px 10px;
      font-size: 0.95em;
      cursor: pointer;
    }

    /* ---- range sliders ---- */
    input[type="range"] {
      width: 100%;
      accent-color: #58a6ff;
      cursor: pointer;
    }

    /* ---- waveform preview canvas ---- */
    #preview {
      display: block;
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 6px;
      margin-bottom: 18px;
    }

    /* ---- generate button ---- */
    #gen-btn {
      padding: 12px 40px;
      font-size: 1.05em;
      background: #238636;
      color: #fff;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      margin-bottom: 10px;
    }

    #gen-btn:hover:not(:disabled) {
      background: #2ea043;
    }

    #gen-btn:disabled {
      background: #21262d;
      color: #8b949e;
      cursor: default;
    }

    /* ---- navigation links ---- */
    .back-link {
      margin: 6px 0 0;
      font-size: 0.85em;
      text-align: center;
    }

    .back-link a {
      color: #58a6ff;
      text-decoration: none;
    }

    /* ---- confirmation screen ---- */
    #done {
      display: none;
      text-align: center;
      margin-top: 32px;
    }

    #done .checkmark {
      font-size: 3em;
      color: #3fb950;
    }

    #done .done-title {
      font-size: 1.4em;
      color: #3fb950;
      margin: 8px 0 4px;
    }

    #done .done-sub {
      color: #8b949e;
      font-size: 0.85em;
    }
  </style>
</head>
<body>

  <h2>Function Generator</h2>
  <p id="subtitle">125 headings oscillating around a centre course</p>

  <!-- ------------------------------------------------------------------ -->
  <!-- Generator form — hidden after a successful POST                     -->
  <!-- ------------------------------------------------------------------ -->
  <div id="generator">

    <!-- Centre heading: pre-filled by the ?h= query parameter from main page -->
    <div class="field">
      <label for="centre">Centre heading</label>
      <div class="field-row">
        <input type="number" id="centre"
               min="0.0" max="359.9" step="0.1" value="0.0"
               oninput="updatePreview()">
        <span class="unit">&deg;</span>
      </div>
    </div>

    <!-- Waveform selection -->
    <div class="field">
      <label for="func-select">Waveform</label>
      <select id="func-select" onchange="updatePreview()">
        <option value="sine">Sine</option>
        <option value="sawtooth">Sawtooth</option>
      </select>
    </div>

    <!-- Periods: 1 – 10 complete cycles across the 125 entries -->
    <div class="field">
      <div class="slider-header">
        <label for="periods">Periods</label>
        <span id="periods-val" class="slider-val">1</span>
      </div>
      <input type="range" id="periods" min="1" max="10" step="1" value="1"
             oninput="document.getElementById('periods-val').textContent = this.value;
                      updatePreview();">
    </div>

    <!-- Amplitude: 0.1 – 5.0 degrees peak deviation from centre -->
    <div class="field">
      <div class="slider-header">
        <label for="amplitude">Amplitude</label>
        <span id="amp-val" class="slider-val">1.0&deg;</span>
      </div>
      <input type="range" id="amplitude" min="0.1" max="5.0" step="0.1" value="1.0"
             oninput="updateAmpVal(this.value); updatePreview();">
    </div>

    <!-- Waveform preview: shows shape only, no axes or labels -->
    <canvas id="preview" width="240" height="80"></canvas>

    <!-- Generate button -->
    <button id="gen-btn" onclick="generate()">Generate 125 sentences</button>

    <!-- Return link -->
    <p class="back-link"><a href="/">&larr; Back to compass</a></p>

  </div>

  <!-- ------------------------------------------------------------------ -->
  <!-- Confirmation — shown after the POST succeeds                        -->
  <!-- ------------------------------------------------------------------ -->
  <div id="done">
    <div class="checkmark">&#10003;</div>
    <div class="done-title">Array updated</div>
    <div class="done-sub">125 sentences loaded to NMEA emulator</div>
    <p class="back-link" style="margin-top: 18px;"><a href="/">&larr; Back to compass</a></p>
  </div>

  <script>
    // --- Pre-fill centre heading from URL query parameter ---
    const params = new URLSearchParams(window.location.search);
    const hParam = params.get('h');
    if (hParam !== null) {
      const h = parseFloat(hParam);
      if (!isNaN(h)) {
        document.getElementById('centre').value = h.toFixed(1);
      }
    }

    // --- Slider label helpers ---
    function updateAmpVal(v) {
      document.getElementById('amp-val').textContent =
        parseFloat(v).toFixed(1) + '\u00b0';
    }

    // --- Waveform functions (both normalised to -1 .. +1) ---
    function sineAt(t) {
      return Math.sin(2 * Math.PI * t);
    }

    function sawtoothAt(t) {
      // Ramps linearly from -1 to +1 per period, then resets.
      return 2 * (t - Math.floor(t)) - 1;
    }

    function waveAt(func, t) {
      return func === 'sine' ? sineAt(t) : sawtoothAt(t);
    }

    // --- Compute 125 heading values ---
    // t spans [0, periods] across indices 0..124 (inclusive on both ends).
    function computeValues() {
      const centre  = parseFloat(document.getElementById('centre').value) || 0;
      const func    = document.getElementById('func-select').value;
      const periods = parseInt(document.getElementById('periods').value);
      const amp     = parseFloat(document.getElementById('amplitude').value);
      const values  = [];

      for (let i = 0; i < 125; i++) {
        const t = (i / 124) * periods;
        const f = waveAt(func, t);

        // Centre + amplitude * normalised wave, wrapped to [0, 360)
        let h = centre + amp * f;
        h = ((h % 360) + 360) % 360;
        h = Math.round(h * 10) / 10;
        values.push(h);
      }
      return values;
    }

    // --- Draw waveform preview ---
    // Plots the normalised shape (-1..+1) regardless of amplitude,
    // so the line always fills the canvas height clearly.
    function updatePreview() {
      const canvas  = document.getElementById('preview');
      const ctx     = canvas.getContext('2d');
      const W       = canvas.width;
      const H       = canvas.height;
      const func    = document.getElementById('func-select').value;
      const periods = parseInt(document.getElementById('periods').value);
      const PAD     = 6;   // vertical padding in pixels

      ctx.clearRect(0, 0, W, H);
      ctx.beginPath();
      ctx.strokeStyle = '#58a6ff';
      ctx.lineWidth   = 2;
      ctx.lineJoin    = 'round';

      for (let i = 0; i < 125; i++) {
        const t = (i / 124) * periods;
        const f = waveAt(func, t);   // -1 .. +1

        const x = (i / 124) * (W - 1);
        // Map f: +1 → top edge (PAD), -1 → bottom edge (H - PAD)
        const y = PAD + (1 - (f + 1) / 2) * (H - 2 * PAD);

        if (i === 0) ctx.moveTo(x, y);
        else         ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    // --- Generate and POST to /update ---
    function generate() {
      const values  = computeValues();
      const payload = values.map(v => v.toFixed(1)).join(',');
      const btn     = document.getElementById('gen-btn');

      btn.disabled    = true;
      btn.textContent = 'Sending\u2026';

      fetch('/update', {
        method:  'POST',
        headers: { 'Content-Type': 'text/plain' },
        body:    payload
      })
      .then(response => response.text())
      .then(() => {
        document.getElementById('generator').style.display = 'none';
        document.getElementById('done').style.display      = 'block';
      })
      .catch(() => {
        alert('Failed to send to ESP32. Are you still connected to NMEA-EMU?');
        btn.disabled    = false;
        btn.textContent = 'Generate 125 sentences';
      });
    }

    // Draw preview on page load
    updatePreview();
  </script>

</body>
</html>
)html";
