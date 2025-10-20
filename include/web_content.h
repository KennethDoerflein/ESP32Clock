// web_content.h

#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

/**
 * @file web_content.h
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
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.13.1/font/bootstrap-icons.min.css" />
<style>
  .container { max-width: 500px; }
</style>
)rawliteral";

/**
 * @brief The main index page (Control Panel).
 * Provides navigation to the other configuration pages.
 */
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html data-bs-theme="dark">
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
          <a href="/settings" class="btn btn-secondary btn-lg">Clock Settings</a>
          <a href="/alarms" class="btn btn-warning btn-lg">Alarm Settings</a>
          <a href="/update" class="btn btn-info btn-lg">Update Firmware</a>
          %SERIAL_LOG_BUTTON%
        </div>
        <div class="mt-4 text-center text-muted">
          <small>Firmware Version: %FIRMWARE_VERSION%</small>
        </div>
      </div>
      <div class="card-footer text-center text-muted small">
        IP: %IP_ADDRESS% &bull; Hostname: %HOSTNAME%
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

/**
 * @brief A simple WiFi setup page for the initial configuration.
 * This page is served in AP mode and has no external dependencies.
 */
const char SIMPLE_WIFI_SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Setup</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; background-color: #222; color: #fff; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .container { background-color: #333; padding: 2rem; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.3); width: 90%; max-width: 400px; }
        h1 { text-align: center; margin-bottom: 1.5rem; }
        form div { margin-bottom: 1rem; }
        label { display: block; margin-bottom: 0.5rem; }
        input[type="text"], input[type="password"] { width: 100%; padding: 0.75rem; border: 1px solid #555; background-color: #444; color: #fff; border-radius: 4px; box-sizing: border-box; }
        button { padding: 0.75rem; border: none; color: white; border-radius: 4px; cursor: pointer; font-size: 1rem; flex-grow: 1; }
        .button-group { display: flex; gap: 0.5rem; }
        #test-button { background-color: #6c757d; }
        #test-button:hover { background-color: #5a6268; }
        #save-button { background-color: #007bff; }
        #save-button:hover { background-color: #0056b3; }
        hr { border: 1px solid #444; margin: 1.5rem 0; }
        #networks-list { list-style: none; padding: 0; max-height: 150px; overflow-y: auto; border: 1px solid #555; border-radius: 4px; }
        #networks-list li { padding: 0.75rem; cursor: pointer; border-bottom: 1px solid #555; }
        #networks-list li:last-child { border-bottom: none; }
        #networks-list li:hover { background-color: #444; }
        #scan-status { text-align: center; margin-top: 1rem; color: #aaa; }
    </style>
</head>
<body>
    <div class="container">
        <h1>WiFi Setup</h1>
        <form action="/wifi/save" method="POST">
            <div>
                <label for="ssid">WiFi Name (SSID)</label>
                <input type="text" id="ssid" name="ssid" required>
            </div>
            <div>
                <label for="password">Password</label>
                <input type="password" id="password" name="password">
            </div>
            <div class="button-group">
                <button type="button" id="test-button">Test Connection</button>
                <button type="submit" id="save-button">Save & Connect</button>
            </div>
        </form>
        <hr>
        <h2>Select a Network</h2>
        <ul id="networks-list"></ul>
        <div id="scan-status">Scanning for networks...</div>
    </div>
    <script>
        const form = document.querySelector('form');
        const networksList = document.getElementById('networks-list');
        const scanStatus = document.getElementById('scan-status');
        const ssidInput = document.getElementById('ssid');
        const saveButton = document.getElementById('save-button');
        const testButton = document.getElementById('test-button');
        let scanInterval;

        function selectNetwork(ssid) { ssidInput.value = ssid; }
        
        async function fetchAndRenderNetworks() {
            try {
                const response = await fetch('/api/wifi/scan');
                const data = await response.json();
                if (Array.isArray(data)) {
                    clearInterval(scanInterval);
                    renderNetworks(data);
                } else if (data.status === 'scanning') {
                    // Continue polling
                }
            } catch (error) {
                clearInterval(scanInterval);
                scanStatus.textContent = 'Error scanning networks.';
            }
        }

        function renderNetworks(networks) {
            networksList.innerHTML = '';
            if (networks.length === 0) {
                scanStatus.textContent = 'No networks found.';
                scanStatus.style.display = 'block';
                return;
            }
            scanStatus.style.display = 'none';
            const uniqueNetworks = Object.values(networks.reduce((acc, net) => {
                if (!acc[net.ssid] || acc[net.ssid].rssi < net.rssi) { acc[net.ssid] = net; }
                return acc;
            }, {}));
            uniqueNetworks.sort((a, b) => b.rssi - a.rssi);
            uniqueNetworks.forEach(net => {
                const listItem = document.createElement('li');
                listItem.textContent = net.ssid;
                listItem.onclick = () => selectNetwork(net.ssid);
                networksList.appendChild(listItem);
            });
        }

        function startNetworkFetch() {
            if (scanInterval) clearInterval(scanInterval);
            fetchAndRenderNetworks(); // Initial fetch
            scanInterval = setInterval(fetchAndRenderNetworks, 3000); // Poll for results
        }
        
        document.addEventListener('DOMContentLoaded', startNetworkFetch);

        let pollingInterval = null;

        async function pollStatus(isSave) {
            try {
                const response = await fetch('/wifi/status');
                const status = await response.text();

                if (status === 'success') {
                    clearInterval(pollingInterval);
                    alert(isSave ? 'Success! Credentials saved. The device will now reboot.' : 'Connection successful!');
                    if (isSave) {
                        saveButton.textContent = 'Rebooting...';
                    } else {
                        saveButton.disabled = false;
                        testButton.disabled = false;
                        testButton.textContent = 'Test Connection';
                    }
                } else if (status === 'failed') {
                    clearInterval(pollingInterval);
                    alert('Connection failed. Please check your password and try again.');
                    saveButton.disabled = false;
                    testButton.disabled = false;
                    saveButton.textContent = 'Save & Connect';
                    testButton.textContent = 'Test Connection';
                }
            } catch (error) {
                clearInterval(pollingInterval);
                alert('Error checking status. Please try again.');
                saveButton.disabled = false;
                testButton.disabled = false;
                saveButton.textContent = 'Save & Connect';
                testButton.textContent = 'Test Connection';
            }
        }

        async function startTest(endpoint, button, isSave) {
            if (ssidInput.value.trim() === '') {
                alert('WiFi Name (SSID) cannot be empty.');
                return;
            }

            button.textContent = 'Testing...';
            saveButton.disabled = true;
            testButton.disabled = true;

            try {
                const formData = new FormData(form);
                const response = await fetch(endpoint, {
                    method: 'POST',
                    body: formData
                });

                if (response.ok) {
                    pollingInterval = setInterval(() => pollStatus(isSave), 2000);
                } else {
                    const errorText = await response.text();
                    alert(`Error: ${errorText}`);
                    saveButton.disabled = false;
                    testButton.disabled = false;
                    button.textContent = isSave ? 'Save & Connect' : 'Test Connection';
                }
            } catch (error) {
                alert('An unexpected error occurred. Please try again.');
                saveButton.disabled = false;
                testButton.disabled = false;
                button.textContent = isSave ? 'Save & Connect' : 'Test Connection';
            }
        }
        
        saveButton.addEventListener('click', (e) => {
            e.preventDefault();
            startTest('/wifi/save', saveButton, true);
        });

        testButton.addEventListener('click', (e) => {
            e.preventDefault();
            startTest('/wifi/test', testButton, false);
        });
    </script>
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
<html data-bs-theme="dark">
<head>
  <title>%WIFI_PAGE_TITLE%</title>
  %HEAD%
  <style>
    .network-item {
      cursor: pointer;
    }
    .network-item:hover {
      background-color: #e9ecef;
      color: #212529;
    }
    #networks-list {
      max-height: 200px;
      overflow-y: auto;
    }
  </style>
</head>
<body>
  <div class="container mt-5">
    <div class="card shadow-sm">
      <div class="card-body">
        <h1 class="card-title text-center mb-4">%WIFI_PAGE_TITLE%</h1>
        <form id="wifi-form" action="/wifi/save" method="POST">
          <div class="mb-3">
            <input type="text" class="form-control" id="ssid" name="ssid" placeholder="SSID" required>
          </div>
          <div class="mb-3">
            <input type="password" class="form-control" name="password" placeholder="Password">
          </div>
          <div class="d-grid">
            <button type="submit" id="save-button" class="btn btn-success btn-lg">Save & Connect</button>
          </div>
        </form>
        <div id="form-status" class="mt-3"></div>
        <hr class="my-4">
        <div class="text-center mb-3">
          <button id="scan-button" class="btn btn-primary">
            <i class="bi bi-wifi"></i> Scan for Networks
          </button>
        </div>
        <div id="networks-status" class="text-center text-muted mb-2 d-none"></div>
        <div id="networks-list" class="list-group"></div>
        <div class="d-grid mt-4 %BACK_BUTTON_CLASS%">
          <a href="/" class="btn btn-secondary">Back to Menu</a>
        </div>
      </div>
    </div>
  </div>
  <script>
    const scanButton = document.getElementById('scan-button');
    const networksStatus = document.getElementById('networks-status');
    const networksList = document.getElementById('networks-list');
    const wifiForm = document.getElementById('wifi-form');
    const saveButton = document.getElementById('save-button');
    const formStatus = document.getElementById('form-status');
    let scanInterval;

    function selectNetwork(ssid) {
      document.getElementById('ssid').value = ssid;
    }

    function getSignalIcon(rssi) {
        if (rssi >= -67) return '<i class="bi bi-reception-4 text-success"></i>';
        if (rssi >= -70) return '<i class="bi bi-reception-3 text-success"></i>';
        if (rssi >= -80) return '<i class="bi bi-reception-2 text-warning"></i>';
        if (rssi >= -90) return '<i class="bi bi-reception-1 text-danger"></i>';
        return '<i class="bi bi-reception-0 text-danger"></i>';
    }
    
    function getEncryptionIcon(encryption) {
        return encryption !== 'OPEN' ? '<i class="bi bi-lock-fill"></i>' : '<i class="bi bi-unlock-fill"></i>';
    }


    async function startScan() {
      scanButton.disabled = true;
      networksStatus.classList.remove('d-none');
      networksStatus.innerHTML = `
        <div class="spinner-border spinner-border-sm" role="status">
          <span class="visually-hidden">Loading...</span>
        </div>
        <span class="ms-2">Scanning for networks...</span>`;
      networksList.innerHTML = '';

      // Immediately try to fetch results
      await fetchAndRenderNetworks();

      // Start polling
      scanInterval = setInterval(fetchAndRenderNetworks, 2000);
    }

    async function fetchAndRenderNetworks() {
      try {
        const response = await fetch('/api/wifi/scan');
        const data = await response.json();

        if (Array.isArray(data)) {
          // Scan is complete and we have data
          clearInterval(scanInterval);
          renderNetworks(data);
        } else if (data.status === 'scanning') {
          // Scan is still in progress, the UI already shows "Scanning..."
        }
      } catch (error) {
        clearInterval(scanInterval);
        networksStatus.innerHTML = 'Error scanning for networks.';
        scanButton.disabled = false;
      }
    }

    function renderNetworks(networks) {
      scanButton.disabled = false;
      networksList.innerHTML = ''; // Clear previous results
      
      if (networks.length === 0) {
        networksStatus.innerHTML = 'No networks found.';
        return;
      }

      networksStatus.classList.add('d-none');
      
      // Filter out duplicate SSIDs, keeping the one with the strongest signal
      const uniqueNetworks = Object.values(networks.reduce((acc, net) => {
        if (!acc[net.ssid] || acc[net.ssid].rssi < net.rssi) {
          acc[net.ssid] = net;
        }
        return acc;
      }, {}));

      uniqueNetworks.sort((a, b) => b.rssi - a.rssi); // Sort by signal strength

      uniqueNetworks.forEach(net => {
        const listItem = document.createElement('a');
        listItem.href = '#';
        listItem.className = 'list-group-item list-group-item-action d-flex justify-content-between align-items-center network-item';
        listItem.onclick = (e) => {
            e.preventDefault();
            selectNetwork(net.ssid);
        };
        
        listItem.innerHTML = `
          <span>
            ${getEncryptionIcon(net.encryption)}
            <strong class="ms-2">${net.ssid}</strong>
          </span>
          <span class="badge bg-light text-dark rounded-pill">
             ${getSignalIcon(net.rssi)}
             <span class="ms-1">${net.rssi} dBm</span>
          </span>
        `;
        networksList.appendChild(listItem);
      });
    }

    function showStatus(message, type = 'info') {
      formStatus.innerHTML = `<div class="alert alert-${type}">${message}</div>`;
    }

    wifiForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      saveButton.disabled = true;
      showStatus('Saving credentials...', 'info');

      try {
        const formData = new FormData(wifiForm);
        const response = await fetch('/wifi/save', {
          method: 'POST',
          body: formData
        });

        const responseText = await response.text();
        if (response.ok) {
          showStatus(responseText, 'success');
        } else {
          showStatus(`Error: ${responseText}`, 'danger');
          saveButton.disabled = false;
        }
      } catch (error) {
        showStatus('An unexpected error occurred.', 'danger');
        saveButton.disabled = false;
      }
    });

    scanButton.addEventListener('click', startScan);
    document.addEventListener('DOMContentLoaded', startScan);
  </script>
</body>
</html>
)rawliteral";

/**
 * @brief The Clock Settings page.
 * Contains a form to adjust settings like brightness, time format, and temperature unit.
 * Fetches data from /api/settings and auto-saves on change.
 */
const char SETTINGS_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html data-bs-theme="dark">
<head>
  <title>Clock Settings</title>
  %HEAD%
  <style>
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
  <body>
    <div class="container mt-5">
      <div class="card shadow-sm">
        <div class="card-body">
          <div class="d-flex justify-content-between align-items-center mb-4">
            <h1 class="card-title m-0">Clock Settings</h1>
            <div class="status-indicator"></div>
          </div>
          
          <div class="alert alert-info d-flex align-items-center" role="alert">
            <i class="bi bi-info-circle-fill me-2"></i>
            <div>Heads up! All your changes are saved automatically.</div>
          </div>

          <form id="settings-form">
            <div
              class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
              <label class="form-check-label" for="auto-brightness">Auto Brightness</label>
              <input
                class="form-check-input"
                type="checkbox"
                role="switch"
                id="auto-brightness"
                name="autoBrightness" />
            </div>

            <div class="mb-3 p-3 border rounded">
              <label for="brightness" class="form-label d-flex justify-content-between">
                <span>Manual Brightness</span>
                <span id="brightness-value">255</span>
              </label>
              <input
                type="range"
                class="form-range"
                id="brightness"
                name="brightness"
                min="10"
                max="255" />
            </div>

            <div
              class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
              <label class="form-check-label" for="24hour">24-Hour Format</label>
              <input
                class="form-check-input"
                type="checkbox"
                role="switch"
                id="24hour"
                name="use24HourFormat" />
            </div>

            <div
              class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
              <label class="form-check-label" for="celsius">Use Celsius (&deg;C)</label>
              <input
                class="form-check-input"
                type="checkbox"
                role="switch"
                id="celsius"
                name="useCelsius" />
            </div>
          </form>

          <hr>

          <form id="hostname-form" class="mt-4">
            <div class="mb-3 p-3 border rounded">
              <label for="hostname" class="form-label">Hostname</label>
              <div class="input-group">
                <input
                  type="text"
                  class="form-control"
                  id="hostname"
                  name="hostname"
                  value="%HOSTNAME%" />
                <button class="btn btn-primary" type="submit">
                  Save & Reboot
                </button>
              </div>
            </div>
          </form>

          <div class="d-grid gap-2 mt-4">
            <a href="/" class="btn btn-secondary">Back to Menu</a>
              <button type="button" class="btn btn-danger w-100 mt-3" onclick="rebootDevice()">
                Reboot Device
              </button>
              <button type="button" class="btn btn-danger w-100 mt-3" onclick="factoryReset()">
                Factory Reset
              </button>
            </div>
          </form>
        </div>
      </div>
    </div>
    <script>
      const DEBOUNCE_DELAY_MS = 1500;
      const ERROR_DISPLAY_MS = 3000;
      let saveTimeout;

      const statusEl = document.querySelector(".status-indicator");
      const form = document.getElementById("settings-form");
      const autoBrightnessEl = document.getElementById("auto-brightness");
      const brightnessEl = document.getElementById("brightness");
      const brightnessValueEl = document.getElementById("brightness-value");
      const twentyFourHourEl = document.getElementById("24hour");
      const celsiusEl = document.getElementById("celsius");

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

      function handleInputChange() {
        clearTimeout(saveTimeout);
        statusEl.innerHTML = STATUS_INDICATORS.UNSAVED;
        saveTimeout = setTimeout(saveSettings, DEBOUNCE_DELAY_MS);
      }
      
      function toggleBrightnessSlider() {
          brightnessEl.disabled = autoBrightnessEl.checked;
      }

      async function loadSettings() {
        statusEl.innerHTML = "";
        try {
          const response = await fetch('/api/settings');
          const settings = await response.json();
          autoBrightnessEl.checked = settings.autoBrightness;
          
          // Use 'actualBrightness' for the slider's value and text
          brightnessEl.value = settings.actualBrightness;
          brightnessValueEl.textContent = settings.actualBrightness;
          
          twentyFourHourEl.checked = settings.use24HourFormat;
          celsiusEl.checked = settings.useCelsius;
          
          // Update the slider's enabled/disabled state
          toggleBrightnessSlider(); 
          
          statusEl.innerHTML = STATUS_INDICATORS.SAVED;
        } catch (e) {
          statusEl.innerHTML = STATUS_INDICATORS.ERROR;
        }
      }

      async function saveSettings() {
        statusEl.innerHTML = STATUS_INDICATORS.SAVING;
        
        const settings = {
            autoBrightness: autoBrightnessEl.checked,
            // When saving, send the slider's current value as the user's preference
            brightness: parseInt(brightnessEl.value), 
            use24HourFormat: twentyFourHourEl.checked,
            useCelsius: celsiusEl.checked
        };

        try {
          await fetch('/api/settings/save', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify(settings)
          });
          statusEl.innerHTML = STATUS_INDICATORS.SAVED;
          
          // After a successful save, reload the settings to get the
          // updated 'actualBrightness' from the device.
          setTimeout(loadSettings, 500);

        } catch (e) {
          statusEl.innerHTML = STATUS_INDICATORS.ERROR;
          setTimeout(() => {
            statusEl.innerHTML = STATUS_INDICATORS.UNSAVED;
          }, ERROR_DISPLAY_MS);
        }
      }

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

      function factoryReset() {
        if (confirm("Are you sure you want to perform a factory reset? This will erase all settings.")) {
          fetch("/factory-reset").then((response) => {
            if (response.ok) {
              alert("Factory reset successful. Device is rebooting...");
            } else {
              alert("Failed to send factory reset command.");
            }
          });
        }
      }
      
      // Update the text value as the slider moves
      brightnessEl.addEventListener('input', () => {
          brightnessValueEl.textContent = brightnessEl.value;
      });

      // Toggle the slider when the checkbox is clicked
      autoBrightnessEl.addEventListener('change', toggleBrightnessSlider);
      
      form.addEventListener('input', handleInputChange);
      document.addEventListener("DOMContentLoaded", loadSettings);

      document.getElementById('hostname-form').addEventListener('submit', function(e) {
        e.preventDefault();
        const hostname = document.getElementById('hostname').value;
        const formData = new FormData();
        formData.append('hostname', hostname);
        fetch('/api/settings/hostname', {
          method: 'POST',
          body: formData
        })
        .then(response => {
          if (response.ok) {
            alert('Hostname saved. The device will now reboot.');
            setTimeout(() => {
              window.location.href = '/';
            }, 1000);
          } else {
            alert('Error saving hostname.');
          }
        });
      });
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
<html lang="en" data-bs-theme="dark">
  <head>
    <title>Alarm Settings</title>
    %HEAD%
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
  <body>
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
          <div class="d-flex justify-content-between align-items-center alarm-header" data-bs-target="#${collapseId}">
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

        const collapseElement = card.querySelector(`#${collapseId}`);
        const collapseInstance = new bootstrap.Collapse(collapseElement, {
          toggle: false
        });

        card.querySelector(".alarm-header").addEventListener("click", (event) => {
          if (!event.target.classList.contains("form-check-input")) {
            collapseInstance.toggle();
          }
        });
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

/**
 * @brief The OTA Update page
 * Provides forms for manual and GitHub updates with on-page status feedback.
 */
const char UPDATE_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en" data-bs-theme="dark">
<head>
  <title>Firmware Update</title>
  %HEAD%
</head>
<body>
  <div class="container mt-5">
    <div class="card shadow-sm">
      <div class="card-body">
        <h1 class="card-title text-center mb-4">Firmware Update</h1>

        <!-- Manual Update Section -->
        <div class="card mb-4">
          <div class="card-body">
            <h5 class="card-title d-flex align-items-center"><i class="bi bi-upload me-2"></i>Manual Update</h5>
            <p class="card-text text-muted small">Select a .bin file from your computer to upload and flash.</p>
            <form id="upload-form">
              <div class="input-group">
                <input type="file" class="form-control" id="firmware" name="firmware" accept=".bin" required>
                <button class="btn btn-primary" type="submit">Upload</button>
              </div>
            </form>
          </div>
        </div>

        <!-- Online Update Section -->
        <div class="card">
          <div class="card-body">
            <h5 class="card-title d-flex align-items-center"><i class="bi bi-cloud-download me-2"></i>Online Update (Untested)</h5>
            <p class="card-text text-muted small">Check for the latest release and update automatically.</p>
            <div class="d-grid">
              <button id="online-button" class="btn btn-success">Check for Updates</button>
            </div>
          </div>
        </div>
        
        <!-- Status and Back Button -->
        <div id="status" class="mt-4"></div>
        <div class="d-grid mt-4">
          <a href="/" id="back-button" class="btn btn-secondary">Back to Menu</a>
        </div>
      </div>
    </div>
  </div>

  <script>
    const uploadForm = document.getElementById('upload-form');
    const onlineButton = document.getElementById('online-button');
    const statusDiv = document.getElementById('status');
    const fileInput = document.getElementById('firmware');
    const backButton = document.getElementById('back-button');
    const uploadButton = uploadForm.querySelector('button');
    let isUpdating = false;

    window.addEventListener('beforeunload', (event) => {
      if (isUpdating) {
        event.preventDefault();
        event.returnValue = '';
      }
    });

    function showStatus(message, type = 'info') {
      statusDiv.innerHTML = `<div class="alert alert-${type} d-flex align-items-center" role="alert">
          ${type === 'info' ? '<div class="spinner-border spinner-border-sm me-2" role="status"><span class="visually-hidden">Loading...</span></div>' : ''}
          <div>${message}</div>
        </div>`;
    }

    function setButtonsDisabled(disabled) {
        uploadButton.disabled = disabled;
        onlineButton.disabled = disabled;
        if(disabled) {
            backButton.classList.add('disabled');
        } else {
            backButton.classList.remove('disabled');
        }
    }

    uploadForm.addEventListener('submit', function(e) {
      e.preventDefault();
      if (!fileInput.files.length) {
        showStatus('Please select a firmware file first.', 'warning');
        return;
      }
      showStatus('Uploading firmware... Do not close this page.');
      setButtonsDisabled(true);
      isUpdating = true;

      const formData = new FormData(this);
      fetch('/update', {
        method: 'POST',
        body: formData
      })
      .then(response => {
        if (!response.ok) {
          throw new Error(`Server responded with status: ${response.status}`);
        }
        return response.text();
      })
      .then(text => {
        showStatus(text, 'success');
        setButtonsDisabled(false);
        isUpdating = false;
      })
      .catch(error => {
        showStatus(`Upload failed: ${error.message}`, 'danger');
        setButtonsDisabled(false);
        isUpdating = false;
      });
    });

    onlineButton.addEventListener('click', function() {
      showStatus('Checking for online updates...');
      setButtonsDisabled(true);
      isUpdating = true;

      fetch('/api/update/github', { method: 'POST' })
      .then(response => {
        if (!response.ok) {
          throw new Error(`Server responded with status: ${response.status}`);
        }
        return response.text();
      })
      .then(text => {
        showStatus(text, 'success');
        // Re-enable buttons after the check.
        setButtonsDisabled(false);
        isUpdating = false;
      })
      .catch(error => {
        showStatus(`Online update check failed: ${error.message}`, 'danger');
        setButtonsDisabled(false);
        isUpdating = false;
      });
    });
  </script>
</body>
</html>
)rawliteral";

/**
 * @brief The Serial Log page for development builds.
 * Displays real-time serial logs with a retro console theme and download functionality.
 */
const char SERIAL_LOG_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en" data-bs-theme="dark">
<head>
    <title>Serial Log</title>
    %HEAD%
    <style>
        /* Retro Console Theme */
        @import url('https://fonts.googleapis.com/css2?family=VT323&display=swap');

        body {
            background-color: #000;
            color: #00FF00;
            font-family: 'VT323', monospace; /* Authentic retro font with a fallback */
        }
        .container {
            max-width: 95%;
        }
        .retro-header {
            color: #00FF00;
            text-shadow: 0 0 5px #00FF00;
        }
        .btn-retro {
            background-color: #1a1a1a;
            border: 1px solid #00FF00;
            color: #00FF00;
            text-shadow: 0 0 2px #00FF00;
        }
        .btn-retro:hover {
            background-color: #00FF00;
            color: #000;
            text-shadow: none;
        }
        #log {
            background-color: #0a0a0a;
            border: 2px solid #00FF00;
            border-radius: 5px;
            height: 70vh;
            overflow-y: auto;
            white-space: pre-wrap;
            word-wrap: break-word;
            font-size: 1.2rem;
            text-shadow: 0 0 2px #00FF00;
            box-shadow: 0 0 15px rgba(0, 255, 0, 0.3) inset;
        }
        .log-connect { color: #39FF14; }
        .log-disconnect { color: #FFA500; }
        .log-error { color: #FF3131; }
    </style>
</head>
<body>
    <div class="container mt-4">
        <div class="d-flex justify-content-between align-items-center mb-3">
            <h1 class="h3 retro-header">Serial LOG</h1>
            <div>
                <button id="download-btn" class="btn btn-retro me-2">Download Log</button>
                <a href="/" class="btn btn-retro">Back to Menu</a>
            </div>
        </div>
        <div id="log" class="p-3"></div>
    </div>
    <script>
        const logDiv = document.getElementById('log');
        const downloadBtn = document.getElementById('download-btn');

        function connect() {
            const socket = new WebSocket(`ws://${window.location.host}/ws`);
            socket.onopen = () => {
                logDiv.innerHTML += '<span class="log-connect">[SYSTEM] WebSocket connection established. Listening...</span>\n';
            };
            socket.onmessage = (event) => {
                const textNode = document.createTextNode(event.data);
                logDiv.appendChild(textNode);
                logDiv.scrollTop = logDiv.scrollHeight;
            };
            socket.onclose = (event) => {
                logDiv.innerHTML += `<span class="log-disconnect">[SYSTEM] WebSocket connection closed (Code: ${event.code}). Reconnecting...</span>\n`;
                setTimeout(connect, 3000);
            };
            socket.onerror = (error) => {
                 logDiv.innerHTML += '<span class="log-error">[SYSTEM] WebSocket error occurred.</span>\n';
            }
        }

        downloadBtn.addEventListener('click', () => {
            // Get the text content, which preserves line breaks
            const logText = logDiv.textContent;
            // Create a blob, which is a file-like object
            const blob = new Blob([logText], { type: 'text/plain;charset=utf-8;' });
            // Create a temporary invisible link
            const link = document.createElement("a");
            link.href = URL.createObjectURL(blob);
            link.download = "esp32clocklog.txt";
            // Programmatically click the link to trigger the download
            document.body.appendChild(link);
            link.click();
            // Clean up by removing the temporary link
            document.body.removeChild(link);
        });

        document.addEventListener('DOMContentLoaded', connect);
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_CONTENT_H