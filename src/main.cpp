/*
 * main.cpp
 *
 * Gyrocompass NMEA sentence emulator for Air-m2m Core ESP32-C3
 *
 * Replays $HEHDT true heading sentences over UART1 at 9600 baud, 8N1,
 * no parity — matching the original RS-422 gyrocompass interface parameters.
 *
 * Defaults to the hardcoded table below (input_files/in-o.txt).
 * A Wi-Fi access point (SSID: NMEA-EMU  pass: nmea1234) is always active.
 * Connect any browser to http://192.168.4.1 to build a custom 125-sentence
 * sequence interactively; the ESP32 switches to it immediately on receipt.
 *
 * Wiring:
 *   NMEA_UART_TX_PIN -> RS-232/RS-422 level converter TX input
 *   GND              -> level converter GND
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "web_page.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Transmission interval — matches sleep(0.1) in db9.py.
const uint32_t TX_INTERVAL_MS = 100;

// UART1 pins on the Air-m2m Core ESP32-C3.
#define NMEA_UART_TX_PIN  4
#define NMEA_UART_RX_PIN  5   // not used — output is TX-only

// Wi-Fi access point credentials.
#define AP_SSID  "NMEA-EMU"
#define AP_PASS  "nmea1234"

// ---------------------------------------------------------------------------
// Default sentence table  (input_files/in-o.txt, 128 entries)
// Stored in flash; active until the user uploads a custom sequence.
// ---------------------------------------------------------------------------

static const char* const nmea_default[] = {
    "$HEHDT,328.9,T\r\n",
    "$HEHDT,329.0,T\r\n",
    "$HEHDT,329.2,T\r\n",
    "$HEHDT,329.3,T\r\n",
    "$HEHDT,329.5,T\r\n",
    "$HEHDT,329.6,T\r\n",
    "$HEHDT,329.8,T\r\n",
    "$HEHDT,329.9,T\r\n",
    "$HEHDT,330.1,T\r\n",
    "$HEHDT,330.3,T\r\n",
    "$HEHDT,330.4,T\r\n",
    "$HEHDT,330.6,T\r\n",
    "$HEHDT,330.7,T\r\n",
    "$HEHDT,330.9,T\r\n",
    "$HEHDT,331.0,T\r\n",
    "$HEHDT,331.1,T\r\n",
    "$HEHDT,331.3,T\r\n",
    "$HEHDT,331.4,T\r\n",
    "$HEHDT,331.5,T\r\n",
    "$HEHDT,331.6,T\r\n",
    "$HEHDT,331.7,T\r\n",
    "$HEHDT,331.7,T\r\n",
    "$HEHDT,331.8,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.8,T\r\n",
    "$HEHDT,331.8,T\r\n",
    "$HEHDT,331.7,T\r\n",
    "$HEHDT,331.6,T\r\n",
    "$HEHDT,331.5,T\r\n",
    "$HEHDT,331.4,T\r\n",
    "$HEHDT,331.3,T\r\n",
    "$HEHDT,331.2,T\r\n",
    "$HEHDT,331.1,T\r\n",
    "$HEHDT,330.9,T\r\n",
    "$HEHDT,330.8,T\r\n",
    "$HEHDT,330.6,T\r\n",
    "$HEHDT,330.5,T\r\n",
    "$HEHDT,330.3,T\r\n",
    "$HEHDT,330.2,T\r\n",
    "$HEHDT,330.0,T\r\n",
    "$HEHDT,329.8,T\r\n",
    "$HEHDT,329.7,T\r\n",
    "$HEHDT,329.5,T\r\n",
    "$HEHDT,329.4,T\r\n",
    "$HEHDT,329.2,T\r\n",
    "$HEHDT,329.1,T\r\n",
    "$HEHDT,328.9,T\r\n",
    "$HEHDT,328.8,T\r\n",
    "$HEHDT,328.7,T\r\n",
    "$HEHDT,328.6,T\r\n",
    "$HEHDT,328.4,T\r\n",
    "$HEHDT,328.3,T\r\n",
    "$HEHDT,328.3,T\r\n",
    "$HEHDT,328.2,T\r\n",
    "$HEHDT,328.1,T\r\n",
    "$HEHDT,328.1,T\r\n",
    "$HEHDT,328.0,T\r\n",
    "$HEHDT,328.0,T\r\n",
    "$HEHDT,328.0,T\r\n",
    "$HEHDT,328.0,T\r\n",
    "$HEHDT,328.0,T\r\n",
    "$HEHDT,328.0,T\r\n",
    "$HEHDT,328.0,T\r\n",
    "$HEHDT,328.1,T\r\n",
    "$HEHDT,328.1,T\r\n",
    "$HEHDT,328.2,T\r\n",
    "$HEHDT,328.3,T\r\n",
    "$HEHDT,328.3,T\r\n",
    "$HEHDT,328.4,T\r\n",
    "$HEHDT,328.6,T\r\n",
    "$HEHDT,328.7,T\r\n",
    "$HEHDT,328.8,T\r\n",
    "$HEHDT,328.9,T\r\n",
    "$HEHDT,329.1,T\r\n",
    "$HEHDT,329.2,T\r\n",
    "$HEHDT,329.4,T\r\n",
    "$HEHDT,329.5,T\r\n",
    "$HEHDT,329.7,T\r\n",
    "$HEHDT,329.9,T\r\n",
    "$HEHDT,330.0,T\r\n",
    "$HEHDT,330.2,T\r\n",
    "$HEHDT,330.3,T\r\n",
    "$HEHDT,330.5,T\r\n",
    "$HEHDT,330.6,T\r\n",
    "$HEHDT,330.8,T\r\n",
    "$HEHDT,330.9,T\r\n",
    "$HEHDT,331.1,T\r\n",
    "$HEHDT,331.2,T\r\n",
    "$HEHDT,331.3,T\r\n",
    "$HEHDT,331.4,T\r\n",
    "$HEHDT,331.5,T\r\n",
    "$HEHDT,331.6,T\r\n",
    "$HEHDT,331.7,T\r\n",
    "$HEHDT,331.8,T\r\n",
    "$HEHDT,331.8,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.9,T\r\n",
    "$HEHDT,331.8,T\r\n",
    "$HEHDT,331.7,T\r\n",
    "$HEHDT,331.7,T\r\n",
    "$HEHDT,331.6,T\r\n",
    "$HEHDT,331.5,T\r\n",
    "$HEHDT,331.4,T\r\n",
    "$HEHDT,331.3,T\r\n",
    "$HEHDT,331.1,T\r\n",
    "$HEHDT,331.0,T\r\n",
    "$HEHDT,330.9,T\r\n",
    "$HEHDT,330.7,T\r\n",
    "$HEHDT,330.6,T\r\n",
    "$HEHDT,330.4,T\r\n",
    "$HEHDT,330.3,T\r\n",
    "$HEHDT,330.1,T\r\n",
    "$HEHDT,329.9,T\r\n",
    "$HEHDT,329.8,T\r\n",
    "$HEHDT,329.6,T\r\n",
};
static const size_t DEFAULT_COUNT = sizeof(nmea_default) / sizeof(nmea_default[0]);

// ---------------------------------------------------------------------------
// Active transmission set
// Pointers into nmea_default[] by default; switched to dyn_buf[] after a
// custom sequence is received from the web page.
// ---------------------------------------------------------------------------

static char        dyn_buf[128][20];   // storage for web-uploaded sentences
static const char* active[128];        // currently transmitting pointers
static size_t      active_count  = 0;
static size_t      sentence_index = 0;

static void activate_default() {
    for (size_t i = 0; i < DEFAULT_COUNT; i++)
        active[i] = nmea_default[i];
    active_count   = DEFAULT_COUNT;
    sentence_index = 0;
}

// Parse a comma-separated list of heading values from the POST body and
// populate dyn_buf[], then make it the active table.
static void apply_uploaded_sequence(const String& body) {
    size_t count = 0;
    int    start = 0;

    while (count < 128) {
        int    sep = body.indexOf(',', start);
        String tok = (sep < 0) ? body.substring(start)
                                : body.substring(start, sep);
        tok.trim();
        if (tok.length() == 0) break;

        float h = tok.toFloat();
        snprintf(dyn_buf[count], sizeof(dyn_buf[count]), "$HEHDT,%.1f,T\r\n", h);
        active[count] = dyn_buf[count];
        count++;

        if (sep < 0) break;
        start = sep + 1;
    }

    if (count > 0) {
        active_count   = count;
        sentence_index = 0;
        Serial.printf("Loaded %u custom sentences from web page\n", (unsigned)count);
    } else {
        Serial.println("Warning: received empty sequence, keeping current table");
    }
}

// ---------------------------------------------------------------------------
// Web server
// ---------------------------------------------------------------------------

static WebServer server(80);

static void setup_server() {
    // Serve the knob page
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", WEB_PAGE);
    });

    // Receive the completed 125-heading sequence
    server.on("/update", HTTP_POST, []() {
        String body = server.arg("plain");
        if (body.length() == 0) {
            server.send(400, "text/plain", "empty body");
            return;
        }
        apply_uploaded_sequence(body);
        server.send(200, "text/plain", "ok");
    });

    server.begin();
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);

    // UART1 for NMEA output
    Serial1.begin(9600, SERIAL_8N1, NMEA_UART_RX_PIN, NMEA_UART_TX_PIN);

    // Onboard LED (GPIO12 on AirM2M Core ESP32-C3)
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    activate_default();

    // Start Wi-Fi access point
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("AP started — SSID: %s  IP: %s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());

    setup_server();

    Serial.printf("NMEA emulator ready: %u sentences, %u ms interval, TX GPIO%d\n",
                  (unsigned)active_count, (unsigned)TX_INTERVAL_MS, NMEA_UART_TX_PIN);
}

void loop() {
    // Service any pending HTTP request before transmitting.
    server.handleClient();

    Serial1.print(active[sentence_index]);
    sentence_index = (sentence_index + 1) % active_count;

    // Brief LED blink when the sequence wraps around to entry 0.
    if (sentence_index == 0) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
        digitalWrite(LED_BUILTIN, LOW);
    }

    delay(TX_INTERVAL_MS);
}
