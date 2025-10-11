// pages.h

#ifndef PAGES_H
#define PAGES_H

/**
 * @file pages.h
 * @brief Contains the HTML, CSS, and JavaScript for the web interface.
 *
 * All content is stored as C-style string literals in PROGMEM to conserve RAM.
 * The pages use a simple template system where placeholders like %VAR% are
 * replaced by dynamic content in the main C++ code.
 */

/**
 * @brief Common HTML head content, including Bootstrap 5 CSS and basic styling.
 * This is injected into every page using the `%HEAD%` placeholder.
 */
const char BOOTSTRAP_HEAD[] PROGMEM = R"rawliteral(
<meta name="viewport" content="width=device-width, initial-scale=1">
<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet">
<style>
  body { background-color: #f0f2f5; }
  .container { max-width: 500px; }
</style>
)rawliteral";

/**
 * @brief The main index page (Control Panel).
 * Provides navigation to the other configuration pages.
 */
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Clock Control</title>
  %HEAD%
</head>
<body>
  <div class="container mt-5">
    <div class="card shadow-sm">
      <div class="card-body text-center">
        <h1 class="card-title mb-4">Control Panel</h1>
        <div class="d-grid gap-3">
          <a href="/wifi" class="btn btn-primary btn-lg">Configure WiFi</a>
          <a href="/update" class="btn btn-info btn-lg">Update Firmware</a>
          <a href="/settings" class="btn btn-secondary btn-lg">Clock Settings</a>
          <a href="/alarms" class="btn btn-warning btn-lg">Alarm Settings</a>
        </div>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

/**
 * @brief The WiFi Configuration page.
 * Displays available networks and provides a form to enter credentials.
 * Contains a placeholder `%NETWORKS%` which is replaced by the list of scanned networks.
 */
const char WIFI_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>%WIFI_PAGE_TITLE%</title>
  %HEAD%
  <script>
    function selectNetwork(ssid) {
      document.getElementById('ssid').value = ssid;
    }
  </script>
</head>
<body>
  <div class="container mt-5">
    <div class="card shadow-sm">
      <div class="card-body">
        <h1 class="card-title text-center mb-4">%WIFI_PAGE_TITLE%</h1>
        <form action="/wifi/save" method="POST">
          <div class="mb-3">
            <input type="text" class="form-control" id="ssid" name="ssid" placeholder="SSID" required>
          </div>
          <div class="mb-3">
            <input type="password" class="form-control" name="password" placeholder="Password">
          </div>
          <div class="d-grid">
            <button type="submit" class="btn btn-success btn-lg">Save & Connect</button>
          </div>
        </form>
        <hr class="my-4">
        <h2 class="h4 text-center">Available Networks</h2>
        <div id="networks" class="list-group">%NETWORKS%</div>
        <div class="d-grid mt-4 %BACK_BUTTON_CLASS%">
          <a href="/" class="btn btn-secondary">Back to Menu</a>
        </div>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

/**
 * @brief The Firmware Update page.
 * Provides two update methods: manual upload and a one-click GitHub update.
 * Includes JavaScript to handle the file upload progress bar and the AJAX request for the GitHub update.
 */
const char UPDATE_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Firmware Update</title>
  %HEAD%
</head>
<body>
  <div class="container mt-5">
    <div class="card shadow-sm">
      <div class="card-body">
        <h1 class="card-title text-center mb-4">Firmware Update</h1>
        
        <h2 class="h5">Manual Upload</h2>
        <form id="upload-form" action="/update/upload" method="POST" enctype="multipart/form-data" class="mb-3">
          <div class="input-group">
            <input type="file" class="form-control" name="update" required>
            <button type="submit" class="btn btn-primary">Upload</button>
          </div>
        </form>
        
        <hr class="my-4">

        <h2 class="h5">GitHub Update</h2>
        <div class="d-grid">
          <button id="github-update-btn" class="btn btn-dark">Check for Updates from GitHub</button>
        </div>

        <div class="progress mt-4" role="progressbar" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100" style="height: 2rem;">
          <div id="progress-bar" class="progress-bar progress-bar-striped progress-bar-animated" style="width: 0%">0%</div>
        </div>
        <div id="status-message" class="text-center mt-2"></div>
        
        <div class="d-grid mt-4">
          <a href="/" class="btn btn-secondary">Back to Menu</a>
        </div>
      </div>
    </div>
  </div>
  <script>
    const form = document.getElementById('upload-form');
    const progressBar = document.getElementById('progress-bar');
    const statusMessage = document.getElementById('status-message');
    const githubBtn = document.getElementById('github-update-btn');

    form.addEventListener('submit', function(e) {
        e.preventDefault();
        const formData = new FormData(this);
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/update/upload', true);

        xhr.upload.addEventListener('progress', function(e) {
            if (e.lengthComputable) {
                const percentComplete = (e.loaded / e.total) * 100;
                progressBar.style.width = percentComplete.toFixed(2) + '%';
                progressBar.textContent = percentComplete.toFixed(2) + '%';
            }
        });

        xhr.onload = function() {
            if (xhr.status === 200) {
                statusMessage.textContent = 'Update successful! Rebooting...';
                setTimeout(() => location.reload(), 3000);
            } else {
                statusMessage.textContent = 'Update failed: ' + xhr.responseText;
            }
        };
        
        xhr.onerror = function() {
            statusMessage.textContent = 'An error occurred during the upload.';
        };

        xhr.send(formData);
        statusMessage.textContent = 'Uploading...';
    });

    githubBtn.addEventListener('click', function() {
        statusMessage.textContent = 'Checking GitHub for updates...';
        progressBar.style.width = '0%';
        progressBar.textContent = '0%';

        fetch('/update/github')
            .then(response => response.text())
            .then(text => {
                statusMessage.textContent = text;
                if (text.includes('successful')) {
                   // The backend handles the reboot.
                }
            })
            .catch(err => {
                statusMessage.textContent = 'Error: ' + err;
            });
    });
  </script>
</body>
</html>
)rawliteral";

/**
 * @brief The Clock Settings page.
 * Contains a form to adjust settings like brightness, time format, and temperature unit.
 * Placeholders like `%AUTO_BRIGHTNESS_CHECKED%` are replaced with current setting values.
 */
const char SETTINGS_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Clock Settings</title>
  %HEAD%
</head>
  <body>
    <div class="container mt-5">
      <div class="card shadow-sm">
        <div class="card-body">
          <h1 class="card-title text-center mb-4">Clock Settings</h1>
          <form action="/settings/save" method="POST">
            <div
              class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
              <label class="form-check-label" for="auto-brightness">Auto Brightness</label>
              <input
                class="form-check-input"
                type="checkbox"
                role="switch"
                id="auto-brightness"
                name="autoBrightness"
                %AUTO_BRIGHTNESS_CHECKED% />
            </div>

            <div class="mb-3 p-3 border rounded">
              <label for="brightness" class="form-label">Manual Brightness</label>
              <input
                type="range"
                class="form-range"
                id="brightness"
                name="brightness"
                min="10"
                max="255"
                value="%BRIGHTNESS_VALUE%" />
            </div>

            <div
              class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
              <label class="form-check-label" for="24hour">24-Hour Format</label>
              <input
                class="form-check-input"
                type="checkbox"
                role="switch"
                id="24hour"
                name="use24HourFormat"
                %IS_24_HOUR_CHECKED% />
            </div>

            <div
              class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
              <label class="form-check-label" for="celsius">Use Celsius (&deg;C)</label>
              <input
                class="form-check-input"
                type="checkbox"
                role="switch"
                id="celsius"
                name="useCelsius"
                %USE_CELSIUS_CHECKED% />
            </div>

            <div class="d-grid gap-2 mt-4">
              <button type="submit" class="btn btn-success">Save Settings</button>
              <a href="/" class="btn btn-secondary">Back to Menu</a>
              <button type="button" class="btn btn-danger w-100 mt-3" onclick="rebootDevice()">
                Reboot Device
              </button>
            </div>
          </form>
        </div>
      </div>
    </div>
    <script>
      function rebootDevice() {
        if (confirm("Are you sure you want to reboot the device?")) {
          fetch("/reboot").then((response) => {
            if (response.ok) {
              alert("Device is rebooting...");
            } else {
              alert("Failed to send reboot command.");
            }
          });
        }
      }
    </script>
  </body>
</html>
)rawliteral";

/**
 * @brief The Alarm Settings page, enhanced with a modern, responsive UI.
 * Features auto-saving, collapsible sections, and clear status indicators.
 * Communicates with the device's backend via the original /api/alarms endpoints.
 */
const char ALARMS_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
  <head>
    <title>Alarm Settings</title>
    %HEAD%
    <link
      rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css" />

    <style>
      :root {
        --btn-base-size: 2.5rem; /* 40px */
        --btn-min-size: 2.1rem; /* ~34px */
        --border-radius-round: 50%;
        --transition-speed: 0.3s;
      }

      .card {
        max-width: 500px;
        margin: auto;
      }

      .day-btn-group .btn {
        display: inline-flex;
        justify-content: center;
        align-items: center;
        border-radius: var(--border-radius-round);
        width: clamp(var(--btn-min-size), 8vw, var(--btn-base-size));
        aspect-ratio: 1 / 1;
        padding: 0;
        font-size: 0.9rem;
      }

      .alarm-header {
        cursor: pointer;
      }

      .collapse-icon {
        transition: transform var(--transition-speed) ease;
      }
      .collapse-icon.collapsed {
        transform: rotate(-90deg);
      }
      
      .status-indicator {
        font-size: 0.9rem;
        font-weight: bold;
        display: flex;
        align-items: center;
        gap: 0.4rem;
        min-width: 95px;
      }
    </style>
  </head>
  <body class="bg-light">
    <div class="container mt-5">
      <div class="card shadow-sm">
        <div class="card-body">
          <h1 class="card-title text-center mb-4">Alarm Settings</h1>

          <div class="alert alert-info d-flex align-items-center" role="alert">
            <i class="bi bi-info-circle-fill me-2"></i>
            <div>Heads up! All your changes are saved automatically.</div>
          </div>

          <div id="alarms-container">
            <p class="text-center">Loading alarms...</p>
          </div>
          <div class="d-grid gap-2 mt-4">
            <a href="/" class="btn btn-secondary">Back to Menu</a>
          </div>
        </div>
      </div>
    </div>

    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>

    <script>
      const DAY_MAP = { 1: "Su", 2: "M", 4: "Tu", 8: "W", 16: "Th", 32: "F", 64: "Sa" };
      const alarmsContainer = document.getElementById("alarms-container");

      const DEBOUNCE_DELAY_MS = 1500;
      const ERROR_DISPLAY_MS = 3000;
      const saveTimeouts = {};

      const STATUS_INDICATORS = {
        SAVED:
          '<i class="bi bi-check-circle-fill text-success"></i> <span class="text-muted">Saved</span>',
        UNSAVED:
          '<i class="bi bi-pencil-fill text-warning"></i> <span class="text-warning">Unsaved</span>',
        SAVING:
          '<div class="spinner-border spinner-border-sm text-primary" role="status"></div> <span class="text-primary">Saving...</span>',
        ERROR:
          '<i class="bi bi-exclamation-triangle-fill text-danger"></i> <span class="text-danger">Error</span>',
      };

      function handleInputChange(cardElement) {
        const alarmId = cardElement.dataset.id;
        const statusEl = cardElement.querySelector(".status-indicator");
        clearTimeout(saveTimeouts[alarmId]);
        statusEl.innerHTML = STATUS_INDICATORS.UNSAVED;
        saveTimeouts[alarmId] = setTimeout(
          () => saveAllAlarms(cardElement),
          DEBOUNCE_DELAY_MS
        );
      }

      function createAlarmCard(alarm) {
        const card = document.createElement("div");
        card.className = "card mb-3";
        card.dataset.id = alarm.id;
        const collapseId = `collapse-alarm-${alarm.id}`;

        let daysHtml = "";
        const dayKeys = [2, 4, 8, 16, 32, 64, 1];
        dayKeys.forEach((value) => {
          const label = DAY_MAP[value];
          const isActive = (alarm.days & value) > 0;
          daysHtml += `<button type="button" class="btn ${
            isActive ? "btn-primary" : "btn-outline-secondary"
          } day-btn" data-value="${value}">${label}</button>`;
        });

        card.innerHTML = `
        <div class="card-body">
          <div class="d-flex justify-content-between align-items-center alarm-header" data-bs-toggle="collapse" data-bs-target="#${collapseId}">
            <div class="d-flex align-items-center">
                <h5 class="card-title mb-0 me-3">Alarm ${alarm.id + 1}</h5>
                <span class="status-indicator"></span>
            </div>
            <div class="d-flex align-items-center">
                 <i class="bi bi-chevron-down collapse-icon me-3"></i>
                 <div class="form-check form-switch">
                   <input class="form-check-input" type="checkbox" role="switch" ${
                     alarm.enabled ? "checked" : ""
                   }>
                 </div>
            </div>
          </div>
          <div class="collapse" id="${collapseId}">
            <hr>
            <div class="mb-3">
              <label class="form-label">Time</label>
              <input type="time" class="form-control" value="${String(
                alarm.hour
              ).padStart(2, "0")}:${String(alarm.minute).padStart(2, "0")}">
            </div>
            <div>
              <label class="form-label">Repeat on</label>
              <div class="d-flex justify-content-around day-btn-group">${daysHtml}</div>
            </div>
          </div>
        </div>
      `;

        card
          .querySelector(".form-check")
          .addEventListener("click", (event) => event.stopPropagation());
        card
          .querySelector(".collapse-icon")
          .addEventListener("click", (event) => event.stopPropagation());

        const collapseElement = card.querySelector(`#${collapseId}`);
        const collapseIcon = card.querySelector(".collapse-icon");
        collapseElement.addEventListener("show.bs.collapse", () =>
          collapseIcon.classList.remove("collapsed")
        );
        collapseElement.addEventListener("hide.bs.collapse", () =>
          collapseIcon.classList.add("collapsed")
        );
        if (!collapseElement.classList.contains("show"))
          collapseIcon.classList.add("collapsed");

        card.querySelectorAll(".day-btn").forEach((btn) => {
          btn.addEventListener("click", () => {
            btn.classList.toggle("btn-primary");
            btn.classList.toggle("btn-outline-secondary");
            handleInputChange(card);
          });
        });

        card
          .querySelector('input[type="time"]')
          .addEventListener("change", () => handleInputChange(card));
        card
          .querySelector('input[type="checkbox"]')
          .addEventListener("change", () => handleInputChange(card));

        return card;
      }

      async function loadAlarms() {
        alarmsContainer.innerHTML =
          '<p class="text-center">Loading alarms...</p>';
        try {
          const response = await fetch('/api/alarms');
          const alarms = await response.json();
          renderAlarms(alarms);
        } catch (e) {
          alarmsContainer.innerHTML =
            '<p class="text-center text-danger">Failed to load alarms.</p>';
        }
      }

      function renderAlarms(alarms) {
        alarmsContainer.innerHTML = "";
        if (!alarms || alarms.length === 0) {
          alarms = Array.from({ length: 5 }, (_, i) => ({
            id: i,
            enabled: false,
            hour: 6,
            minute: 0,
            days: 0,
          }));
        }
        alarms.forEach((alarm) => {
          const cardElement = createAlarmCard(alarm);
          alarmsContainer.appendChild(cardElement);
          cardElement.querySelector(".status-indicator").innerHTML =
            STATUS_INDICATORS.SAVED;
        });
      }

      async function saveAllAlarms(changedCardElement) {
        const statusEl = changedCardElement.querySelector(".status-indicator");
        statusEl.innerHTML = STATUS_INDICATORS.SAVING;

        const allAlarmsData = [];
        alarmsContainer.querySelectorAll(".card").forEach((card) => {
          const alarmData = {
            id: parseInt(card.dataset.id),
            enabled: card.querySelector(".form-check-input").checked,
            hour: parseInt(card.querySelector('input[type="time"]').value.split(":")[0]),
            minute: parseInt(card.querySelector('input[type="time"]').value.split(":")[1]),
            days: 0,
          };
          card.querySelectorAll(".day-btn.btn-primary").forEach((btn) => {
            alarmData.days |= parseInt(btn.dataset.value);
          });
          allAlarmsData.push(alarmData);
        });

        try {
          await fetch('/api/alarms/save', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify(allAlarmsData)
          });
          statusEl.innerHTML = STATUS_INDICATORS.SAVED;
        } catch (e) {
          statusEl.innerHTML = STATUS_INDICATORS.ERROR;
          setTimeout(() => {
            statusEl.innerHTML = STATUS_INDICATORS.UNSAVED;
          }, ERROR_DISPLAY_MS);
        }
      }

      document.addEventListener("DOMContentLoaded", loadAlarms);
    </script>
  </body>
</html>
)rawliteral";

#endif // PAGES_H
