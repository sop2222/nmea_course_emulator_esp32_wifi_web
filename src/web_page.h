#pragma once

/*
 * web_page.h
 *
 * HTML served at http://192.168.4.1 by the ESP32 access point.
 *
 * Usage:
 *   - Drag (or touch-drag) the compass needle to a heading value.
 *   - Tap "Add to sequence" to append that heading to the list.
 *   - Repeat until 125 headings have been collected.
 *   - The page then POSTs the full comma-separated list to /update
 *     and shows a confirmation message.
 */

static const char WEB_PAGE[] = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>NMEA Sequence Builder</title>
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
      margin: 0 0 14px;
    }

    canvas {
      cursor: crosshair;
      touch-action: none;
      display: block;
    }

    #heading-display {
      font-size: 2.6em;
      font-weight: 700;
      color: #e6edf3;
      letter-spacing: 0.04em;
      margin: 8px 0;
    }

    #add-btn {
      padding: 12px 40px;
      font-size: 1.05em;
      background: #238636;
      color: #fff;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      margin-bottom: 14px;
    }

    #add-btn:hover:not(:disabled) {
      background: #2ea043;
    }

    #add-btn:disabled {
      background: #21262d;
      color: #8b949e;
      cursor: default;
    }

    #progress-bar-track {
      width: 240px;
      height: 6px;
      background: #21262d;
      border-radius: 3px;
      margin-bottom: 6px;
      overflow: hidden;
    }

    #progress-bar-fill {
      height: 100%;
      width: 0%;
      background: #238636;
      border-radius: 3px;
      transition: width 0.15s ease;
    }

    #counter {
      font-size: 0.9em;
      color: #8b949e;
      margin-bottom: 10px;
    }

    #counter strong {
      color: #58a6ff;
    }

    #log {
      width: 240px;
      max-height: 130px;
      overflow-y: auto;
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 6px;
      padding: 6px 10px;
      font-size: 0.82em;
      color: #8b949e;
      font-family: monospace;
    }

    #log .log-entry {
      padding: 1px 0;
    }

    #log .log-entry.last {
      color: #e6edf3;
    }

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

  <h2>NMEA Sequence Builder</h2>
  <p id="subtitle">Drag the needle &rarr; tap Add &rarr; repeat 125 times</p>

  <!-- Builder UI (hidden after submission) -->
  <div id="builder">
    <canvas id="knob" width="240" height="240"></canvas>
    <div id="heading-display">000.0&deg;</div>
    <button id="add-btn" onclick="addHeading()">Add to sequence</button>

    <div id="progress-bar-track">
      <div id="progress-bar-fill"></div>
    </div>
    <div id="counter">Added: <strong id="cnt">0</strong> / 125</div>

    <div id="log"></div>
  </div>

  <!-- Confirmation (shown after 125 entries are sent) -->
  <div id="done">
    <div class="checkmark">&#10003;</div>
    <div class="done-title">Array updated</div>
    <div class="done-sub">125 sentences loaded to NMEA emulator</div>
  </div>

  <script>
    // --- Canvas knob setup ---
    const canvas  = document.getElementById('knob');
    const ctx     = canvas.getContext('2d');
    const CX = 120, CY = 120, R = 108;

    let heading   = 0.0;
    let dragging  = false;
    const collected = [];

    function formatHeading(v) {
      return v.toFixed(1).padStart(5, '0') + '\u00b0';
    }

    function drawKnob() {
      ctx.clearRect(0, 0, 240, 240);

      // Background disc
      ctx.beginPath();
      ctx.arc(CX, CY, R, 0, 2 * Math.PI);
      ctx.fillStyle   = '#161b22';
      ctx.fill();
      ctx.strokeStyle = '#30363d';
      ctx.lineWidth   = 2;
      ctx.stroke();

      // Tick marks: every 5 degrees, major every 30
      for (let i = 0; i < 72; i++) {
        const angleDeg = i * 5;
        const angleRad = angleDeg * Math.PI / 180 - Math.PI / 2;
        const major    = (angleDeg % 30 === 0);
        const medium   = (angleDeg % 15 === 0);
        const innerR   = major ? R - 22 : medium ? R - 14 : R - 8;

        ctx.beginPath();
        ctx.moveTo(CX + (R - 1) * Math.cos(angleRad), CY + (R - 1) * Math.sin(angleRad));
        ctx.lineTo(CX + innerR  * Math.cos(angleRad), CY + innerR  * Math.sin(angleRad));
        ctx.strokeStyle = major ? '#58a6ff' : medium ? '#484f58' : '#2d333b';
        ctx.lineWidth   = major ? 2 : 1;
        ctx.stroke();
      }

      // Degree labels at 0 / 90 / 180 / 270
      [[0, 'N'], [90, 'E'], [180, 'S'], [270, 'W']].forEach(([deg, label]) => {
        const a = deg * Math.PI / 180 - Math.PI / 2;
        ctx.fillStyle       = (label === 'N') ? '#f85149' : '#58a6ff';
        ctx.font            = 'bold 14px sans-serif';
        ctx.textAlign       = 'center';
        ctx.textBaseline    = 'middle';
        ctx.fillText(label, CX + (R - 36) * Math.cos(a), CY + (R - 36) * Math.sin(a));
      });

      // Needle
      const needleAngle = heading * Math.PI / 180 - Math.PI / 2;
      ctx.save();
      ctx.lineCap = 'round';

      // Red tip (points to selected heading)
      ctx.beginPath();
      ctx.moveTo(CX - 18 * Math.cos(needleAngle), CY - 18 * Math.sin(needleAngle));
      ctx.lineTo(CX + (R - 26) * Math.cos(needleAngle), CY + (R - 26) * Math.sin(needleAngle));
      ctx.strokeStyle = '#f85149';
      ctx.lineWidth   = 3;
      ctx.stroke();

      // Grey tail
      ctx.beginPath();
      ctx.moveTo(CX, CY);
      ctx.lineTo(CX - 26 * Math.cos(needleAngle), CY - 26 * Math.sin(needleAngle));
      ctx.strokeStyle = '#8b949e';
      ctx.lineWidth   = 2;
      ctx.stroke();

      ctx.restore();

      // Centre pivot dot
      ctx.beginPath();
      ctx.arc(CX, CY, 6, 0, 2 * Math.PI);
      ctx.fillStyle = '#c9d1d9';
      ctx.fill();

      document.getElementById('heading-display').textContent = formatHeading(heading);
    }

    // --- Pointer input ---
    function headingFromEvent(e) {
      const rect  = canvas.getBoundingClientRect();
      const touch = e.touches ? e.touches[0] : e;
      const dx    = touch.clientX - rect.left - CX;
      const dy    = touch.clientY - rect.top  - CY;
      let angle   = Math.atan2(dy, dx) * 180 / Math.PI + 90;
      if (angle < 0)    angle += 360;
      return (Math.round(angle * 10) / 10) % 360;
    }

    canvas.addEventListener('mousedown', e => {
      dragging = true;
      heading  = headingFromEvent(e);
      drawKnob();
    });
    canvas.addEventListener('mousemove', e => {
      if (!dragging) return;
      heading = headingFromEvent(e);
      drawKnob();
    });
    document.addEventListener('mouseup', () => { dragging = false; });

    canvas.addEventListener('touchstart', e => {
      dragging = true;
      heading  = headingFromEvent(e);
      drawKnob();
      e.preventDefault();
    }, { passive: false });
    canvas.addEventListener('touchmove', e => {
      if (!dragging) return;
      heading = headingFromEvent(e);
      drawKnob();
      e.preventDefault();
    }, { passive: false });
    document.addEventListener('touchend', () => { dragging = false; });

    // --- Add heading to sequence ---
    function addHeading() {
      if (collected.length >= 125) return;

      collected.push(heading);
      const count = collected.length;

      // Update counter and progress bar
      document.getElementById('cnt').textContent = count;
      document.getElementById('progress-bar-fill').style.width = (count / 125 * 100) + '%';

      // Append to log; highlight the newest entry
      const log     = document.getElementById('log');
      const prev    = log.querySelector('.last');
      if (prev) prev.classList.remove('last');

      const entry         = document.createElement('div');
      entry.className     = 'log-entry last';
      entry.textContent   = count + '. ' + formatHeading(heading);
      log.appendChild(entry);
      log.scrollTop = log.scrollHeight;

      if (count >= 125) {
        document.getElementById('add-btn').disabled = true;
        sendSequence();
      }
    }

    // --- POST sequence to ESP32 ---
    function sendSequence() {
      const payload = collected.map(v => v.toFixed(1)).join(',');

      fetch('/update', {
        method:  'POST',
        headers: { 'Content-Type': 'text/plain' },
        body:    payload
      })
      .then(response => response.text())
      .then(() => {
        document.getElementById('builder').style.display = 'none';
        document.getElementById('done').style.display    = 'block';
      })
      .catch(() => {
        alert('Failed to send to ESP32. Are you still connected to NMEA-EMU?');
        document.getElementById('add-btn').disabled = false;
      });
    }

    // Initial draw
    drawKnob();
  </script>

</body>
</html>
)html";
