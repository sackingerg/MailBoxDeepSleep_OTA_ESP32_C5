#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#include "config.h"
#include "debug.h"

/*
  OTAPortal (ESP32-C5)
  --------------------
  - ESP32 creates a Wi-Fi AP and serves a web page at http://192.168.4.1/
  - You upload a firmware .bin from iPhone Files / Windows disk
  - The upload is streamed directly into flash using Update.h
  - On success, ESP restarts

  IMPORTANT:
  - While the portal is running, do NOT deep sleep.
  - We also disable WiFi power-save via WiFi.setSleep(false).
*/
//

class OTAPortal {
public:
  OTAPortal() : _server(OTA_HTTP_PORT) {}

  void begin() {
    _quit = false;
    _updateInProgress = false;
    _updateSuccess = false;
    _updateError = "";

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);        // keep radio awake/responsive

    bool ok = WiFi.softAP(OTA_AP_SSID, OTA_AP_PASSWORD);
    delay(50);

    DBG_PRINTLN("");
    DBG_PRINTLN("=== OTA PORTAL START ===");
    DBG_PRINT("AP start: "); DBG_PRINTLN(ok ? "OK" : "FAILED");
    DBG_PRINT("SSID: "); DBG_PRINTLN(OTA_AP_SSID);
    DBG_PRINT("IP:   "); DBG_PRINTLN(WiFi.softAPIP());
    DBG_PRINTLN("========================");

    registerRoutes();
    _server.begin();
  }

  void handleClient() { _server.handleClient(); }
  bool quitRequested() const { return _quit; }

  void end() {
    DBG_PRINTLN("Stopping OTA portal; WiFi OFF.");
    _server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(50);
  }

private:
  WebServer _server;
  volatile bool _quit = false;

  bool _updateInProgress = false;
  bool _updateSuccess = false;
  String _updateError;

  String pageHtml() {
    String html;
    html += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Mailbox OTA</title>";
    html += "<style>body{font-family:Arial;margin:16px;line-height:1.35}";
    html += ".card{border:1px solid #ddd;border-radius:12px;padding:14px;margin:12px 0}";
    html += "button{font-size:18px;padding:12px 14px;width:100%;border-radius:10px;border:none;cursor:pointer;transition:background 0.3s}";
    html += "button[type=submit]{background:#4CAF50;color:white}";  // Green for Upload
    html += "button[type=submit]:hover{background:#45a049}";
    html += "button.quit{background:#f44336;color:white}";  // Red for Quit
    html += "button.quit:hover{background:#da190b}";
    html += "input[type=file]{width:100%;font-size:16px}";
    html += ".small{font-size:12px;color:#555}";
    html += ".ok{color:#0a7d00;font-weight:bold}";
    html += ".err{color:#b00020;font-weight:bold}";
    html += "</style></head><body>";

    html += "<h2>Mailbox OTA Upload (ESP32-C5)</h2>";
    html += "<div class='small'>Firmware: <b>" + String(FW_VERSION_STRING) + "</b></div>";

    html += "<div class='card'><b>Connect to Wi-Fi:</b> " + String(OTA_AP_SSID) +
            "<br><b>Open:</b> http://192.168.4.1/</div>";

    html += "<div class='card'><h3>1) Upload new firmware (.bin)</h3>";
    html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='firmware' accept='.bin' required>";
    html += "<p class='small'>Pick the .bin from:<br>"
            "- <b>iPhone</b>: Files → On My iPhone / iCloud Drive / Downloads<br>"
            "- <b>Windows</b>: Downloads/Documents<br>"
            "Expected filename (hint): <b>" + String(OTA_EXPECTED_BIN_NAME) + "</b></p>";
    html += "<button type='submit'>Upload & Flash</button></form>";

    if (_updateInProgress) html += "<p class='small'>Uploading...</p>";
    if (_updateSuccess) html += "<p class='ok'>Update complete. Rebooting...</p>";
    if (_updateError.length()) html += "<p class='err'>Update failed: " + _updateError + "</p>";
    html += "</div>";

    html += "<div class='card'><h3>2) Quit</h3>"
            "<form method='POST' action='/quit'>"
            "<button class='quit' type='submit'>Quit (turn off Wi-Fi and continue)</button>"
            "</form>"
            "<p class='small'>If you do nothing, the portal ends when the boot window expires (extended if connected).</p></div>";

    html += "</body></html>";
    return html;
  }

  void registerRoutes() {
    _server.on("/", HTTP_GET, [&]() { _server.send(200, "text/html", pageHtml()); });

    _server.on("/quit", HTTP_POST, [&]() {
      _quit = true;
      _server.send(200, "text/plain", "Quit selected. Wi-Fi will turn off and the mailbox program will continue.");
    });

    _server.on(
      "/update",
      HTTP_POST,
      // Final response after upload ends
      [&]() {
        _server.send(200, "text/html", pageHtml());
        if (_updateSuccess) {
          DBG_PRINTLN("OTA OK -> rebooting.");
          DBG_FLUSH();
          delay(400);
          ESP.restart();
        }
      },
      // Upload stream handler
      [&]() {
        HTTPUpload& upload = _server.upload();

        if (upload.status == UPLOAD_FILE_START) {
          _updateInProgress = true;
          _updateSuccess = false;
          _updateError = "";

          DBG_PRINTLN("OTA: Upload start");
          DBG_PRINT("OTA: Filename: "); DBG_PRINTLN(upload.filename);

          if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            _updateError = Update.errorString();
            DBG_PRINT("OTA: Update.begin failed: "); DBG_PRINTLN(_updateError);
          }
        }
        else if (upload.status == UPLOAD_FILE_WRITE) {
          if (!_updateError.length()) {
            size_t written = Update.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize) {
              _updateError = Update.errorString();
              DBG_PRINT("OTA: Write failed: "); DBG_PRINTLN(_updateError);
            }
          }
        }
        else if (upload.status == UPLOAD_FILE_END) {
          DBG_PRINT("OTA: Upload end. Total bytes: "); DBG_PRINTLN(upload.totalSize);

          if (!_updateError.length()) {
            if (Update.end(true)) {
              _updateSuccess = true;
              DBG_PRINTLN("OTA: Update.end OK");
            } else {
              _updateError = Update.errorString();
              DBG_PRINT("OTA: Update.end failed: "); DBG_PRINTLN(_updateError);
            }
          }
          _updateInProgress = false;
        }
        else if (upload.status == UPLOAD_FILE_ABORTED) {
          _updateInProgress = false;
          _updateError = "Upload aborted";
          DBG_PRINTLN("OTA: Upload aborted");
        }
      }
    );
  }
};