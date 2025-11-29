#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

/**
 * @file WebContent.h
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
<meta charset="UTF-8">
<meta http-equiv="content-language" content="en-US">
<meta name="language" content="English">
<meta name="google" content="notranslate">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet">
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.13.1/font/bootstrap-icons.min.css" />
<style>
  .container { max-width: 600px; }
</style>
)rawliteral";

const char SERIAL_LOG_TAB_HTML[] PROGMEM = R"rawliteral(
<li class="nav-item" role="presentation">
  <button class="nav-link" id="serial-log-tab" data-bs-toggle="tab" data-bs-target="#serial-log" type="button" role="tab" aria-controls="serial-log" aria-selected="false">Logs</button>
</li>
)rawliteral";

const char SERIAL_LOG_TAB_PANE_HTML[] PROGMEM = R"rawliteral(
<div class="tab-pane fade" id="serial-log" role="tabpanel" aria-labelledby="serial-log-tab">
  <div class="card mb-3">
    <div class="card-body">
      <h5 class="card-title d-flex align-items-center"><i class="bi bi-file-text me-2"></i>System Log File</h5>
      <p class="card-text text-muted small">Manage the persistent system log file stored on the device.</p>
      <div class="d-grid gap-2">
        <a href="/api/log/download" id="download-system-log-btn" class="btn btn-secondary" target="_blank">Download System Log</a>
        <button id="rollover-btn" class="btn btn-outline-danger" title="Rollover the log file manually.">Rollover Log</button>
      </div>
    </div>
  </div>
  <div class="log-container">
    <div class="log-header">
      <h5>Live Log</h5>
      <button id="download-btn" class="btn btn-sm btn-outline-secondary" title="Download the current log as a text file.">Download Live Log</button>
    </div>
    <div id="log">Waiting for logs...</div>
  </div>
</div>
)rawliteral";

const char SERIAL_LOG_SCRIPT_JS[] PROGMEM = R"rawliteral(
<script>
  const logDiv = document.getElementById('log');
  const downloadBtn = document.getElementById('download-btn');
  const serialLogTab = document.getElementById('serial-log-tab');
  let socket;

  function connect() {
    if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
      return;
    }
    socket = new WebSocket(`ws://${window.location.host}/ws/log`);
    socket.onopen = () => {
        logDiv.innerHTML = '<span class="log-connect">[SYSTEM] WebSocket connection established. Listening...</span><br>';
    };
    socket.onmessage = (event) => {
        const shouldScroll = logDiv.scrollTop + logDiv.clientHeight >= logDiv.scrollHeight - 10;
        const textNode = document.createTextNode(event.data);
        logDiv.appendChild(textNode);
        if (shouldScroll) {
          logDiv.scrollTop = logDiv.scrollHeight;
        }
    };
    socket.onclose = (event) => {
        logDiv.innerHTML += `<span class="log-disconnect">[SYSTEM] WebSocket connection closed (Code: ${event.code}). Reconnecting...</span><br>`;
        setTimeout(connect, 3000);
    };
    socket.onerror = (error) => {
         logDiv.innerHTML += '<span class="log-error">[SYSTEM] WebSocket error occurred.</span><br>';
    }
  }

  function disconnect() {
    if (socket && socket.readyState === WebSocket.OPEN) {
      socket.close();
    }
  }

  if (serialLogTab) {
    serialLogTab.addEventListener('shown.bs.tab', function () {
      connect();
    });
    serialLogTab.addEventListener('hidden.bs.tab', function () {
      disconnect();
    });
  }
  
  if (downloadBtn) {
    downloadBtn.addEventListener('click', () => {
        const logText = logDiv.textContent;
        const blob = new Blob([logText], { type: 'text/plain;charset=utf-8;' });
        const link = document.createElement("a");
        link.href = URL.createObjectURL(blob);
        link.download = "esp32clocklog.txt";
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    });
  }

</script>
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
  <div class="container my-5">
    <div class="card shadow-sm">
      <div class="card-body text-center">
        <h1 class="card-title mb-4">Control Panel</h1>
        <div class="d-grid gap-3">
          <a href="/wifi" class="btn btn-primary btn-lg d-flex align-items-center justify-content-center" title="Configure WiFi network settings."><i class="bi bi-wifi me-2"></i>Configure WiFi</a>
          <a href="/alarms" class="btn btn-warning btn-lg d-flex align-items-center justify-content-center" title="Set and manage alarms."><i class="bi bi-alarm-fill me-2"></i>Alarms</a>
          <a href="/settings" class="btn btn-info btn-lg d-flex align-items-center justify-content-center" title="Adjust clock and display settings."><i class="bi bi-gear-fill me-2"></i>Settings</a>
          <a href="/system" class="btn btn-secondary btn-lg d-flex align-items-center justify-content-center" title="Update firmware and view logs."><i class="bi bi-hdd-fill me-2"></i>System</a>
        </div>
        <div class="mt-4 text-center text-muted">
          <small>Firmware Version: %FIRMWARE_VERSION%</small>
        </div>
      </div>
    </div>
    <div class="card mt-4">
      <div class="card-body">
        <h5 class="card-title text-center mb-3">Sensor Data</h5>
        <div id="sensor-data-container" class="row text-center">
          <div class="col" id="bme-temp-col" style="display: none;">
            <h6><i class="bi bi-thermometer-sun me-1"></i> Temp</h6>
            <p class="fs-4 mb-0" id="bme-temp"></p>
          </div>
          <div class="col" id="bme-humidity-col" style="display: none;">
            <h6><i class="bi bi-droplet-half me-1"></i> Humidity</h6>
            <p class="fs-4 mb-0" id="bme-humidity"></p>
          </div>
          <div class="col">
            <h6><i class="bi bi-thermometer-half me-1"></i> RTC Temp</h6>
            <p class="fs-4 mb-0" id="rtc-temp"></p>
          </div>
        </div>
      </div>
      <div class="card-footer text-center text-muted small">
        IP: %IP_ADDRESS% &bull; Hostname: %HOSTNAME%
      </div>
    </div>
  </div>
  <script>
    document.addEventListener('DOMContentLoaded', function() {
      const bmeTempCol = document.getElementById('bme-temp-col');
      const bmeHumidityCol = document.getElementById('bme-humidity-col');
      const bmeTempEl = document.getElementById('bme-temp');
      const bmeHumidityEl = document.getElementById('bme-humidity');
      const rtcTempEl = document.getElementById('rtc-temp');

      function updateSensorReadings() {
        fetch('/api/sensors')
          .then(response => response.json())
          .then(data => {
            if (data.bmeFound) {
              bmeTempEl.textContent = `${data.bmeTemp}°${data.unit}`;
              bmeHumidityEl.textContent = `${data.bmeHumidity}%`;
              bmeTempCol.style.display = 'block';
              bmeHumidityCol.style.display = 'block';
            } else {
              bmeTempCol.style.display = 'none';
              bmeHumidityCol.style.display = 'none';
            }
            
            if (data.rtcTemp) {
              rtcTempEl.textContent = `${data.rtcTemp}°${data.unit}`;
            } else {
              rtcTempEl.textContent = 'N/A';
            }
          })
          .catch(error => console.error('Error fetching sensor data:', error));
      }

      // Initial update
      updateSensorReadings();
      // Update sensors every 3 seconds
      setInterval(updateSensorReadings, 3000);
    });
  </script>
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
        input[type="text"], input[type="password"] { font-size: 1rem; width: 100%; padding: 0.75rem; border: 1px solid #555; background-color: #444; color: #fff; border-radius: 4px; box-sizing: border-box; }
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
                <button type="button" id="test-button" title="Test the WiFi connection without saving.">Test Connection</button>
                <button type="submit" id="save-button" title="Save the WiFi credentials and connect.">Save & Connect</button>
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
  <div class="container my-5">
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
            <button type="submit" id="save-button" class="btn btn-success btn-lg" title="Save the selected WiFi network credentials and connect.">Save & Connect</button>
          </div>
        </form>
        <div id="form-status" class="mt-3"></div>
        <hr class="my-4">
        <div class="text-center mb-3">
          <button id="scan-button" class="btn btn-primary" title="Scan for available WiFi networks.">
            <i class="bi bi-wifi"></i> Scan for Networks
          </button>
        </div>
        <div id="networks-status" class="text-center text-muted mb-2 d-none"></div>
        <div id="networks-list" class="list-group"></div>
        <div class="d-grid mt-4 %BACK_BUTTON_CLASS%">
          <a href="/" class="btn btn-secondary" title="Return to the main menu.">Back to Menu</a>
        </div>
      </div>
    </div>
    <form id="hostname-form" class="mt-4">
      <div class="mb-3 p-3 border rounded">
        <label for="hostname" class="form-label" title="Set the network hostname for the device.">Hostname</label>
        <div class="input-group">
          <input
            type="text"
            class="form-control"
            id="hostname"
            name="hostname"
            value="%HOSTNAME%"
            title="Enter the desired hostname." />
          <button class="btn btn-warning" type="submit" title="Save the new hostname and reboot the device.">
            Save & Reboot
          </button>
        </div>
      </div>
    </form>
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

      // Start the scan on the backend
      try {
        await fetch('/api/wifi/scan?start=true');
      } catch (error) {
        networksStatus.innerHTML = 'Error starting scan.';
        scanButton.disabled = false;
        return;
      }

      // Start polling for results
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

    document.getElementById('hostname-form').addEventListener('submit', function(e) {
      e.preventDefault();
      const hostname = document.getElementById('hostname').value;
      const formData = new FormData();
      formData.append('hostname', hostname);
      fetch('/api/wifi/hostname', {
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
 * @brief The Settings page.
 * Contains forms to adjust clock settings and display colors.
 */
const char SETTINGS_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html data-bs-theme="dark">
<head>
  <title>Settings</title>
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
      .color-picker-wrapper {
        margin-bottom: 1rem;
        padding: 0.75rem;
        border: 1px solid #444;
        border-radius: 0.375rem;
        text-align: center;
      }
      .color-picker-wrapper label {
        margin-bottom: 0.5rem;
        display: block;
      }
      input[type="color"] {
        min-width: 100px;
        height: 40px;
        padding: 0.2rem;
        border-radius: 0.375rem;
        border: none;
        cursor: pointer;
      }
      /* Make range inputs easier to use on touch devices */
      .form-range {
        padding: 0;
      }
      .form-range::-webkit-slider-thumb {
        width: 2rem;
        height: 2rem;
        margin-top: -0.75rem; /* Vertically center thumb on track */
      }
      .form-range::-moz-range-thumb {
        width: 2rem;
        height: 2rem;
      }
      .form-range::-webkit-slider-runnable-track {
        height: 0.5rem;
      }
      .form-range::-moz-range-track {
        height: 0.5rem;
      }
      .form-range::-ms-track {
        height: 0.5rem;
      }
  </style>
</head>
  <body>
    <div class="container my-5">
      <div class="card shadow-sm">
        <div class="card-body">
          <div class="d-flex justify-content-between align-items-center mb-4">
            <h1 class="card-title m-0">Settings</h1>
            <div class="status-indicator"></div>
          </div>
          
          <div class="alert alert-info d-flex align-items-center" role="alert">
            <i class="bi bi-info-circle-fill me-2"></i>
            <div>Heads up! All your changes are saved automatically.</div>
          </div>

          <ul class="nav nav-tabs nav-fill" id="settingsTabs" role="tablist">
            <li class="nav-item" role="presentation">
              <button class="nav-link active" id="general-tab" data-bs-toggle="tab" data-bs-target="#general" type="button" role="tab" aria-controls="general" aria-selected="true">General</button>
            </li>
            <li class="nav-item" role="presentation">
              <button class="nav-link" id="display-tab" data-bs-toggle="tab" data-bs-target="#display" type="button" role="tab" aria-controls="display" aria-selected="false">Display</button>
            </li>
          </ul>

          <div class="tab-content" id="settingsTabsContent">
            <div class="tab-pane fade show active" id="general" role="tabpanel" aria-labelledby="general-tab">
              <form id="settings-form" class="p-3">
                <div id="auto-brightness-section" class="mb-3 p-3 border rounded">
                  <div class="form-check form-switch ps-0 d-flex justify-content-between align-items-center">
                    <label class="form-check-label" for="auto-brightness" title="Enable or disable automatic brightness adjustment.">Auto Brightness</label>
                    <input class="form-check-input" type="checkbox" role="switch" id="auto-brightness" name="autoBrightness" %AUTO_BRIGHTNESS_CHECKED% />
                  </div>
                  <div id="auto-brightness-controls" class="mt-3 %AUTO_BRIGHTNESS_CONTROLS_CLASS%">
                    <hr>
                    <label for="auto-brightness-start-hour" class="form-label d-flex justify-content-between">
                      <span>Start Hour</span>
                      <span id="auto-brightness-start-hour-value">%AUTO_BRIGHTNESS_START_HOUR_VALUE%</span>
                    </label>
                    <input type="range" class="form-range" id="auto-brightness-start-hour" name="autoBrightnessStartHour" min="0" max="23" value="%AUTO_BRIGHTNESS_START_HOUR%" />
                    <label for="auto-brightness-end-hour" class="form-label d-flex justify-content-between mt-2">
                      <span>End Hour</span>
                      <span id="auto-brightness-end-hour-value">%AUTO_BRIGHTNESS_END_HOUR_VALUE%</span>
                    </label>
                    <input type="range" class="form-range" id="auto-brightness-end-hour" name="autoBrightnessEndHour" min="0" max="23" value="%AUTO_BRIGHTNESS_END_HOUR%" />
                    <label for="day-brightness" class="form-label d-flex justify-content-between mt-2">
                      <span>Day Brightness</span>
                      <span id="day-brightness-value">%DAY_BRIGHTNESS_VALUE%</span>
                    </label>
                    <input type="range" class="form-range" id="day-brightness" name="dayBrightness" min="%BRIGHTNESS_MIN%" max="%BRIGHTNESS_MAX%" value="%DAY_BRIGHTNESS%" />
                    <label for="night-brightness" class="form-label d-flex justify-content-between mt-2">
                      <span>Night Brightness</span>
                      <span id="night-brightness-value">%NIGHT_BRIGHTNESS_VALUE%</span>
                    </label>
                    <input type="range" class="form-range" id="night-brightness" name="nightBrightness" min="%BRIGHTNESS_MIN%" max="%BRIGHTNESS_MAX%" value="%NIGHT_BRIGHTNESS%" />
                  </div>
                </div>
                <div id="manual-brightness-section" class="mb-3 p-3 border rounded %MANUAL_BRIGHTNESS_CLASS%">
                  <label for="brightness" class="form-label d-flex justify-content-between">
                    <span title="Set the display brightness manually.">Manual Brightness</span>
                    <span id="brightness-value">%BRIGHTNESS_VALUE%</span>
                  </label>
                  <input type="range" class="form-range" id="brightness" name="brightness" min="%BRIGHTNESS_MIN%" max="%BRIGHTNESS_MAX%" title="Adjust the manual brightness level." value="%BRIGHTNESS%" />
                </div>
                <div class="mb-3 p-3 border rounded">
                  <div class="form-check form-switch ps-0 d-flex justify-content-between align-items-center">
                    <label class="form-check-label" for="24hour" title="Switch between 12-hour and 24-hour time formats.">24-Hour Format</label>
                    <input class="form-check-input" type="checkbox" role="switch" id="24hour" name="use24HourFormat" %USE_24_HOUR_FORMAT_CHECKED% />
                  </div>
                  <hr class="my-2">
                  <div class="form-check form-switch ps-0 d-flex justify-content-between align-items-center">
                    <label class="form-check-label" for="celsius" title="Switch between Celsius and Fahrenheit temperature units.">Use Celsius (&deg;C)</label>
                    <input class="form-check-input" type="checkbox" role="switch" id="celsius" name="useCelsius" %USE_CELSIUS_CHECKED% />
                  </div>
                  <hr class="my-2">
                  <div class="form-check form-switch ps-0 d-flex justify-content-between align-items-center">
                    <label class="form-check-label" for="screen-flipped" title="Flip the screen orientation 180 degrees.">Flip Screen</label>
                    <input class="form-check-input" type="checkbox" role="switch" id="screen-flipped" name="screenFlipped" %SCREEN_FLIPPED_CHECKED% />
                  </div>
                  <hr class="my-2">
                  <div class="form-check form-switch ps-0 d-flex justify-content-between align-items-center">
                    <div>
                      <label class="form-check-label" for="invert-colors" title="Invert the display colors.">Invert Colors</label>
                      <small class="form-text text-muted d-block">Needed for some IPS displays</small>
                    </div>
                    <input class="form-check-input" type="checkbox" role="switch" id="invert-colors" name="invertColors" %INVERT_COLORS_CHECKED% />
                  </div>
                </div>
                <div class="mb-3 p-3 border rounded">
                  <div class="form-check form-switch ps-0 d-flex justify-content-between align-items-center">
                    <label class="form-check-label" for="temp-correction-enabled">Enable Temperature Correction</label>
                    <input class="form-check-input" type="checkbox" role="switch" id="temp-correction-enabled" name="tempCorrectionEnabled" %TEMP_CORRECTION_ENABLED_CHECKED%>
                  </div>
                  <div id="temp-correction-controls" class="%TEMP_CORRECTION_CONTROLS_CLASS%">
                    <hr>
                    <label for="temp-correction" class="form-label">Temperature Correction (&deg;%TEMP_CORRECTION_UNIT%)</label>
                    <input type="number" class="form-control" id="temp-correction" name="tempCorrection" step="0.1" value="%TEMP_CORRECTION_VALUE%">
                    <small class="form-text text-muted">If the clock's temperature is 2 degrees too high, enter -2.0 here.</small>
                  </div>
                </div>
                <div class="mb-3 p-3 border rounded">
                  <label for="timezone-select" class="form-label">Timezone</label>
                  <select class="form-select" id="timezone-select" name="timezone">
                    <option value="EST5EDT,M3.2.0/2:00,M11.1.0/2:00" %TIMEZONE_SELECTED_EST%>Eastern Time</option>
                    <option value="CST6CDT,M3.2.0/2:00,M11.1.0/2:00" %TIMEZONE_SELECTED_CST%>Central Time</option>
                    <option value="MST7MDT,M3.2.0/2:00,M11.1.0/2:00" %TIMEZONE_SELECTED_MST%>Mountain Time</option>
                    <option value="PST8PDT,M3.2.0/2:00,M11.1.0/2:00" %TIMEZONE_SELECTED_PST%>Pacific Time</option>
                    <option value="MST7" %TIMEZONE_SELECTED_AZ%>Arizona</option>
                    <option value="AKST9AKDT,M3.2.0/2:00,M11.1.0/2:00" %TIMEZONE_SELECTED_AK%>Alaska</option>
                    <option value="HST10" %TIMEZONE_SELECTED_HI%>Hawaii</option>
                  </select>
                </div>
                <div class="mb-3 p-3 border rounded">
                  <label for="snooze-duration" class="form-label">Snooze Duration (minutes)</label>
                  <input type="number" class="form-control" id="snooze-duration" name="snoozeDuration" min="1" max="60" value="%SNOOZE_DURATION%">
                </div>
                <div class="mb-3 p-3 border rounded">
                  <label for="dismiss-duration" class="form-label">Hold to Dismiss (seconds)</label>
                  <input type="number" class="form-control" id="dismiss-duration" name="dismissDuration" min="1" max="10" value="%DISMISS_DURATION%">
                </div>
              </form>
              <div class="d-grid gap-2 mt-3">
                <button type="button" id="reset-general-btn" class="btn btn-danger" title="Reset all general settings to their default values.">Reset General Settings</button>
              </div>
            </div>
            <div class="tab-pane fade" id="display" role="tabpanel" aria-labelledby="display-tab">
              <form id="display-settings-form" class="p-3">
                <div class="row">
                  <div class="col-md-6">
                    <div class="color-picker-wrapper">
                      <label for="background-color" title="Set the main background color.">Background Color</label>
                      <input type="color" id="background-color" name="backgroundColor" title="Select a background color." value="%BACKGROUND_COLOR%">
                    </div>
                  </div>
                  <div class="col-md-6">
                    <div class="color-picker-wrapper">
                      <label for="time-color" title="Set the color for the time display.">Time Color</label>
                      <input type="color" id="time-color" name="timeColor" title="Select a color for the time." value="%TIME_COLOR%">
                    </div>
                  </div>
                  <div class="col-md-6">
                    <div class="color-picker-wrapper">
                      <label for="tod-color" title="Set the color for the AM/PM indicator.">TOD Color</label>
                      <input type="color" id="tod-color" name="todColor" title="Select a color for the AM/PM indicator." value="%TOD_COLOR%">
                    </div>
                  </div>
                  <div class="col-md-6">
                    <div class="color-picker-wrapper">
                      <label for="seconds-color" title="Set the color for the seconds display.">Seconds Color</label>
                      <input type="color" id="seconds-color" name="secondsColor" title="Select a color for the seconds." value="%SECONDS_COLOR%">
                    </div>
                  </div>
                  <div class="col-md-6">
                    <div class="color-picker-wrapper">
                      <label for="day-of-week-color" title="Set the color for the day of the week.">Day of Week Color</label>
                      <input type="color" id="day-of-week-color" name="dayOfWeekColor" title="Select a color for the day of the week." value="%DAY_OF_WEEK_COLOR%">
                    </div>
                  </div>
                  <div class="col-md-6">
                    <div class="color-picker-wrapper">
                      <label for="date-color" title="Set the color for the date display.">Date Color</label>
                      <input type="color" id="date-color" name="dateColor" title="Select a color for the date." value="%DATE_COLOR%">
                    </div>
                  </div>
                  <div class="col-md-6">
                    <div class="color-picker-wrapper">
                      <label for="temp-color" title="Set the color for the temperature display.">Temperature Color</label>
                      <input type="color" id="temp-color" name="tempColor" title="Select a color for the temperature." value="%TEMP_COLOR%">
                    </div>
                  </div>
                  <div class="col-md-6">
                    <div class="color-picker-wrapper">
                      <label for="humidity-color" title="Set the color for the humidity display.">Humidity Color</label>
                      <input type="color" id="humidity-color" name="humidityColor" title="Select a color for the humidity." value="%HUMIDITY_COLOR%">
                    </div>
                  </div>
                </div>
              </form>
              <div class="d-grid gap-2 mt-4">
                <button type="button" id="reset-colors-btn" class="btn btn-danger" title="Reset all colors to their default values.">Reset Colors</button>
              </div>
            </div>
          </div>

          <div class="d-grid gap-2 mt-4">
            <a href="/" class="btn btn-secondary" title="Return to the main menu.">Back to Menu</a>
          </div>
        </div>
      </div>
    </div>
    <script>
      const ERROR_DISPLAY_MS = 3000;
      let saveGeneralTimeout;
      let saveDisplayTimeout;

      const BRIGHTNESS_MIN = %BRIGHTNESS_MIN%;
      const BRIGHTNESS_MAX = %BRIGHTNESS_MAX%;

      const statusEl = document.querySelector(".status-indicator");
      const settingsForm = document.getElementById("settings-form");
      const displaySettingsForm = document.getElementById("display-settings-form");
      
      const autoBrightnessEl = document.getElementById("auto-brightness");
      const brightnessEl = document.getElementById("brightness");
      const brightnessValueEl = document.getElementById("brightness-value");
      const manualBrightnessSectionEl = document.getElementById("manual-brightness-section");
      const autoBrightnessControlsEl = document.getElementById("auto-brightness-controls");
      const autoBrightnessStartHourEl = document.getElementById("auto-brightness-start-hour");
      const autoBrightnessStartHourValueEl = document.getElementById("auto-brightness-start-hour-value");
      const autoBrightnessEndHourEl = document.getElementById("auto-brightness-end-hour");
      const autoBrightnessEndHourValueEl = document.getElementById("auto-brightness-end-hour-value");
      const dayBrightnessEl = document.getElementById("day-brightness");
      const dayBrightnessValueEl = document.getElementById("day-brightness-value");
      const nightBrightnessEl = document.getElementById("night-brightness");
      const nightBrightnessValueEl = document.getElementById("night-brightness-value");
      const twentyFourHourEl = document.getElementById("24hour");
      const celsiusEl = document.getElementById("celsius");
      const tempCorrectionEnabledEl = document.getElementById("temp-correction-enabled");
      const tempCorrectionEl = document.getElementById("temp-correction");
      const tempCorrectionControlsEl = document.getElementById("temp-correction-controls");
      const screenFlippedEl = document.getElementById("screen-flipped");
      const invertColorsEl = document.getElementById("invert-colors");
      const timezoneEl = document.getElementById("timezone-select");
      const colorPickers = displaySettingsForm.querySelectorAll('input[type="color"]');
      const resetGeneralBtn = document.getElementById('reset-general-btn');
      const resetColorsBtn = document.getElementById('reset-colors-btn');

      const STATUS_INDICATORS = {
        SAVED: '<i class="bi bi-check-circle-fill text-success"></i> <span class="text-muted">Saved</span>',
        UNSAVED: '<i class="bi bi-pencil-fill text-warning"></i> <span class="text-warning">Unsaved</span>',
        SAVING: '<div class="spinner-border spinner-border-sm text-primary" role="status"></div> <span class="text-primary">Saving...</span>',
        ERROR: '<i class="bi bi-exclamation-triangle-fill text-danger"></i> <span class="text-danger">Error</span>',
      };

      function setStatus(status) {
        statusEl.innerHTML = status;
      }

      function clamp(v, a, b) {
        return Math.max(a, Math.min(b, v));
      }

      function toPercent(value, min = BRIGHTNESS_MIN, max = BRIGHTNESS_MAX) {
        const num = Number(value);
        const clamped = clamp(num, min, max);
        const pct = Math.round(((clamped - min) / (max - min)) * 100);
        return `${pct}%`;
      }

      function formatHour(hour, is24Hour) {
        if (is24Hour) {
          const h = parseInt(hour);
          return h < 10 ? '0' + h : h;
        }
        const h = parseInt(hour);
        if (h === 0) return '12 AM';
        if (h === 12) return '12 PM';
        if (h < 12) return `${h} AM`;
        return `${h - 12} PM`;
      }

      function handleGeneralInputChange(event) {
        clearTimeout(saveGeneralTimeout);
        setStatus(STATUS_INDICATORS.UNSAVED);
        saveGeneralTimeout = setTimeout(() => saveGeneralSettings(event), 100);
      }

      function handleDisplayInputChange() {
        clearTimeout(saveDisplayTimeout);
        setStatus(STATUS_INDICATORS.UNSAVED);
        saveDisplayTimeout = setTimeout(saveDisplaySettings, 50);
      }

      function toggleBrightnessSlider() {
        const isAuto = autoBrightnessEl.checked;
        if (isAuto) {
          manualBrightnessSectionEl.classList.add('d-none');
          autoBrightnessControlsEl.classList.remove('d-none');
        } else {
          manualBrightnessSectionEl.classList.remove('d-none');
          autoBrightnessControlsEl.classList.add('d-none');
          fetch('/api/settings')
            .then(response => response.json())
            .then(settings => {
              brightnessEl.value = settings.actualBrightness;
              brightnessValueEl.textContent = toPercent(settings.actualBrightness);
            })
            .catch(e => console.error('Failed to sync brightness slider:', e));
        }
      }

      function updateBrightnessUI(settings) {
        brightnessEl.value = settings.actualBrightness ?? 128;
        brightnessValueEl.textContent = toPercent(brightnessEl.value);
        
        dayBrightnessEl.value = settings.dayBrightness ?? 255;
        dayBrightnessValueEl.textContent = toPercent(dayBrightnessEl.value);
        
        nightBrightnessEl.value = settings.nightBrightness ?? 10;
        nightBrightnessValueEl.textContent = toPercent(nightBrightnessEl.value);
      }
      
      function updateHourFormatUI(settings) {
        const is24Hour = settings.use24HourFormat ?? false;
        twentyFourHourEl.checked = is24Hour;
        
        autoBrightnessStartHourEl.value = settings.autoBrightnessStartHour ?? 7;
        autoBrightnessStartHourValueEl.textContent = formatHour(autoBrightnessStartHourEl.value, is24Hour);
        
        autoBrightnessEndHourEl.value = settings.autoBrightnessEndHour ?? 21;
        autoBrightnessEndHourValueEl.textContent = formatHour(autoBrightnessEndHourEl.value, is24Hour);
      }
      
      function updateGeneralSettingsUI(settings) {
        autoBrightnessEl.checked = settings.autoBrightness || false;
        celsiusEl.checked = settings.useCelsius || false;
        screenFlippedEl.checked = settings.screenFlipped || false;
        invertColorsEl.checked = settings.invertColors || false;
        timezoneEl.value = settings.timezone || "EST5EDT,M3.2.0/2:00,M11.1.0/2:00";
        document.getElementById('snooze-duration').value = settings.snoozeDuration || 9;
        document.getElementById('dismiss-duration').value = settings.dismissDuration || 3;
        
        let tempCorrection = settings.tempCorrection || 0.0;
        if (!settings.useCelsius) {
          tempCorrection = tempCorrection * 9.0 / 5.0;
        }
        tempCorrectionEl.value = tempCorrection.toFixed(1);
        tempCorrectionEnabledEl.checked = settings.tempCorrectionEnabled || false;
        
        if (tempCorrectionEnabledEl.checked) {
          tempCorrectionControlsEl.classList.remove('d-none');
        } else {
          tempCorrectionControlsEl.classList.add('d-none');
        }

        // Manually trigger UI updates that depend on these values
        updateBrightnessUI(settings);
        updateHourFormatUI(settings);
        toggleBrightnessSlider();
      }
      
      function updateDisplaySettingsUI(settings) {
        if (settings.backgroundColor) document.getElementById('background-color').value = settings.backgroundColor;
        if (settings.timeColor) document.getElementById('time-color').value = settings.timeColor;
        if (settings.todColor) document.getElementById('tod-color').value = settings.todColor;
        if (settings.secondsColor) document.getElementById('seconds-color').value = settings.secondsColor;
        if (settings.dayOfWeekColor) document.getElementById('day-of-week-color').value = settings.dayOfWeekColor;
        if (settings.dateColor) document.getElementById('date-color').value = settings.dateColor;
        if (settings.tempColor) document.getElementById('temp-color').value = settings.tempColor;
        if (settings.humidityColor) document.getElementById('humidity-color').value = settings.humidityColor;
      }
      
      async function fetchGeneralSettings() {
        try {
          const response = await fetch('/api/settings');
          if (!response.ok) throw new Error('Failed to fetch general settings');
          const settings = await response.json();
          updateGeneralSettingsUI(settings);
        } catch (e) {
          console.error(e);
          setStatus(STATUS_INDICATORS.ERROR);
        }
      }

      async function fetchDisplaySettings() {
        try {
          const response = await fetch('/api/display');
          if (!response.ok) throw new Error('Failed to fetch display settings');
          const settings = await response.json();
          updateDisplaySettingsUI(settings);
        } catch (e) {
          console.error(e);
          setStatus(STATUS_INDICATORS.ERROR);
        }
      }

      async function saveGeneralSettings(event) {
        clearTimeout(saveGeneralTimeout); // Clear pending saves
        clearTimeout(saveDisplayTimeout);
        setStatus(STATUS_INDICATORS.SAVING);

        const settings = {
          autoBrightness: autoBrightnessEl.checked,
          // send numeric raw values to the server
          brightness: parseInt(brightnessEl.value),
          autoBrightnessStartHour: parseInt(autoBrightnessStartHourEl.value),
          autoBrightnessEndHour: parseInt(autoBrightnessEndHourEl.value),
          dayBrightness: parseInt(dayBrightnessEl.value),
          nightBrightness: parseInt(nightBrightnessEl.value),
          use24HourFormat: twentyFourHourEl.checked,
          useCelsius: celsiusEl.checked,
          tempCorrectionEnabled: tempCorrectionEnabledEl.checked,
          screenFlipped: screenFlippedEl.checked,
          invertColors: invertColorsEl.checked,
          timezone: timezoneEl.value,
          snoozeDuration: parseInt(document.getElementById('snooze-duration').value),
          dismissDuration: parseInt(document.getElementById('dismiss-duration').value),
          tempCorrection: parseFloat(document.getElementById('temp-correction').value)
        };

        if (!settings.useCelsius) {
          settings.tempCorrection = settings.tempCorrection * 5.0 / 9.0;
        }

        try {
          // Save general settings
          const saveSettingsResponse = await fetch('/api/settings/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(settings)
          });
          if (!saveSettingsResponse.ok) throw new Error('Failed to save general settings');

          const target = event ? event.target : null;
          if (!target || (target.id !== 'temp-correction' && target.type !== 'range')) {
            await fetchGeneralSettings(); // Re-fetch to get new state (like actualBrightness)
          }
          setStatus(STATUS_INDICATORS.SAVED);
          
        } catch (e) {
          console.error(e);
          setStatus(STATUS_INDICATORS.ERROR);
          // Removed setTimeout that reverted to UNSAVED
        }
      }

      async function saveDisplaySettings() {
        clearTimeout(saveGeneralTimeout); // Clear pending saves
        clearTimeout(saveDisplayTimeout);
        setStatus(STATUS_INDICATORS.SAVING);

        const displaySettings = {};
        colorPickers.forEach(picker => {
          displaySettings[picker.name] = picker.value;
        });

        try {
          // Save display settings
          const saveDisplayResponse = await fetch('/api/display/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(displaySettings)
          });
          if (!saveDisplayResponse.ok) throw new Error('Failed to save display settings');

          // After saving, fetch the new display state
          await fetchDisplaySettings(); // Re-fetch new state
          setStatus(STATUS_INDICATORS.SAVED);
        } catch (e) {
          console.error(e);
          setStatus(STATUS_INDICATORS.ERROR);
          // Removed setTimeout that reverted to UNSAVED
        }
      }
      
      // Update text labels in real-time as user slides
      brightnessEl.addEventListener('input', () => {
        brightnessValueEl.textContent = toPercent(brightnessEl.value);
      });
      dayBrightnessEl.addEventListener('input', () => {
        dayBrightnessValueEl.textContent = toPercent(dayBrightnessEl.value);
      });
      nightBrightnessEl.addEventListener('input', () => {
        nightBrightnessValueEl.textContent = toPercent(nightBrightnessEl.value);
      });
      
      // Update hour formats in real-time
      autoBrightnessStartHourEl.addEventListener('input', () => {
         autoBrightnessStartHourValueEl.textContent = formatHour(autoBrightnessStartHourEl.value, twentyFourHourEl.checked);
      });
      autoBrightnessEndHourEl.addEventListener('input', () => {
         autoBrightnessEndHourValueEl.textContent = formatHour(autoBrightnessEndHourEl.value, twentyFourHourEl.checked);
      });
      twentyFourHourEl.addEventListener('change', () => {
         autoBrightnessStartHourValueEl.textContent = formatHour(autoBrightnessStartHourEl.value, twentyFourHourEl.checked);
         autoBrightnessEndHourValueEl.textContent = formatHour(autoBrightnessEndHourEl.value, twentyFourHourEl.checked);
      });

      celsiusEl.addEventListener('change', () => {
        const tempCorrectionEl = document.getElementById('temp-correction');
        const tempCorrectionLabel = document.querySelector('label[for="temp-correction"]');
        let currentValue = parseFloat(tempCorrectionEl.value);

        if (celsiusEl.checked) {
          // We are switching TO Celsius, so the current value is in Fahrenheit
          currentValue = currentValue * 5.0 / 9.0;
          tempCorrectionLabel.innerHTML = `Temperature Correction (&deg;C)`;
        } else {
          // We are switching TO Fahrenheit, so the current value is in Celsius
          currentValue = currentValue * 9.0 / 5.0;
          tempCorrectionLabel.innerHTML = `Temperature Correction (&deg;F)`;
        }
        tempCorrectionEl.value = currentValue.toFixed(1);
      });

      tempCorrectionEnabledEl.addEventListener('change', () => {
        if (tempCorrectionEnabledEl.checked) {
          tempCorrectionControlsEl.classList.remove('d-none');
        } else {
          tempCorrectionControlsEl.classList.add('d-none');
        }
      });

      tempCorrectionEl.addEventListener('input', () => {
        let value = tempCorrectionEl.value;
        let decimalIndex = value.indexOf('.');
        if (decimalIndex !== -1 && value.length - decimalIndex > 2) {
          tempCorrectionEl.value = parseFloat(value).toFixed(1);
        }
      });

      autoBrightnessEl.addEventListener('change', toggleBrightnessSlider);
      
      // Use 'input' for sliders for real-time feedback, and 'change' for other inputs
      settingsForm.addEventListener('input', (e) => {
        // For sliders and the temp correction, save with a debounce
        if (e.target.type === 'range' || e.target.id === 'temp-correction') {
          handleGeneralInputChange(e);
        }
      });
      settingsForm.addEventListener('change', (e) => handleGeneralInputChange(e));
      displaySettingsForm.addEventListener('change', handleDisplayInputChange);
      
      document.addEventListener("DOMContentLoaded", () => {
        setStatus(STATUS_INDICATORS.SAVED);
        toggleBrightnessSlider(); // Set initial UI state for brightness sections
        fetchGeneralSettings();
        fetchDisplaySettings();
      });

      resetColorsBtn.addEventListener('click', async () => {
        if (confirm('Are you sure you want to reset all colors to their default values?')) {
          clearTimeout(saveGeneralTimeout); // Clear pending saves
          clearTimeout(saveDisplayTimeout);
          setStatus(STATUS_INDICATORS.SAVING);

          try {
            const response = await fetch('/api/display/reset', { method: 'POST' });
            if (response.ok) {
              setStatus(STATUS_INDICATORS.SAVED);
              await fetchDisplaySettings(); // Fetch and apply new default values
            } else {
              throw new Error('Failed to reset colors');
            }
          } catch (e) {
            console.error(e);
            setStatus(STATUS_INDICATORS.ERROR);
          }
        }
      });

      resetGeneralBtn.addEventListener('click', async () => {
        if (confirm('Are you sure you want to reset all general settings to their default values?')) {
          clearTimeout(saveGeneralTimeout); // Clear pending saves
          clearTimeout(saveDisplayTimeout);
          setStatus(STATUS_INDICATORS.SAVING);

          try {
            const response = await fetch('/api/settings/reset', { method: 'POST' });
            if (response.ok) {
              setStatus(STATUS_INDICATORS.SAVED);
              await fetchGeneralSettings(); // Fetch and apply new default values
            } else {
              throw new Error('Failed to reset general settings');
            }
          } catch (e) {
            console.error(e);
            setStatus(STATUS_INDICATORS.ERROR);
          }
        }
      });
    </script>

    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>
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
<html data-bs-theme="dark">
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
    <div class="container my-5">
      <div class="card shadow-sm">
        <div class="card-body">
          <div class="d-flex justify-content-between align-items-center mb-4">
            <h1 class="card-title m-0">Alarm Settings</h1>
            <div class="status-indicator"></div>
          </div>

          <div class="alert alert-info d-flex align-items-center" role="alert">
            <i class="bi bi-info-circle-fill me-2"></i>
            <div>Heads up! All your changes are saved automatically.</div>
          </div>

          <div id="alarms-container">
            <p class="text-center">Loading alarms...</p>
          </div>
          <div class="d-grid gap-2 mt-4">
            <a href="/" class="btn btn-secondary" title="Return to the main menu.">Back to Menu</a>
          </div>
        </div>
      </div>
    </div>

    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>

    <script>
      const DAY_MAP = { 1: "Su", 2: "M", 4: "Tu", 8: "W", 16: "Th", 32: "F", 64: "Sa" };
      const alarmsContainer = document.getElementById("alarms-container");
      const statusEl = document.querySelector(".status-indicator");

      const ERROR_DISPLAY_MS = 3000;
      let saveTimeout;

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
        saveTimeout = setTimeout(saveAllAlarms, 50);
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
          } day-btn" data-value="${value}" title="Toggle ${label}">${label}</button>`;
        });

        card.innerHTML = `
        <div class="card-body">
          <div class="d-flex justify-content-between align-items-center alarm-header" data-bs-target="#${collapseId}" title="Expand or collapse alarm settings.">
            <h5 class="card-title mb-0">Alarm ${alarm.id + 1}</h5>
            <div class="d-flex align-items-center">
                 <i class="bi bi-chevron-down collapse-icon me-3"></i>
                 <div class="form-check form-switch">
                   <input class="form-check-input" type="checkbox" role="switch" ${
                     alarm.enabled ? "checked" : ""
                   } title="Enable or disable this alarm.">
                 </div>
            </div>
          </div>
          <div class="collapse" id="${collapseId}">
            <hr>
            <div class="mb-3">
              <label class="form-label">Time</label>
              <input type="time" class="form-control" value="${String(
                alarm.hour
              ).padStart(2, "0")}:${String(alarm.minute).padStart(
                2,
                "0"
              )}" title="Set the alarm time.">
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
          toggle: false,
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
            handleInputChange();
          });
        });

        card
          .querySelector('input[type="time"]')
          .addEventListener("change", () => handleInputChange());
        card
          .querySelector('input[type="checkbox"]')
          .addEventListener("change", () => handleInputChange());

        return card;
      }

      async function loadAlarms() {
        alarmsContainer.innerHTML =
          '<p class="text-center">Loading alarms...</p>';
        try {
          const response = await fetch("/api/alarms");
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
        });
        statusEl.innerHTML = STATUS_INDICATORS.SAVED;
      }

      async function saveAllAlarms() {
        statusEl.innerHTML = STATUS_INDICATORS.SAVING;

        const allAlarmsData = [];
        alarmsContainer.querySelectorAll(".card").forEach((card) => {
          const alarmData = {
            id: parseInt(card.dataset.id),
            enabled: card.querySelector(".form-check-input").checked,
            hour: parseInt(
              card.querySelector('input[type="time"]').value.split(":")[0]
            ),
            minute: parseInt(
              card.querySelector('input[type="time"]').value.split(":")[1]
            ),
            days: 0,
          };
          card.querySelectorAll(".day-btn.btn-primary").forEach((btn) => {
            alarmData.days |= parseInt(btn.dataset.value);
          });
          allAlarmsData.push(alarmData);
        });

        try {
          await fetch("/api/alarms/save", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(allAlarmsData),
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
 * @brief The System page for updates and logs.
 * Provides forms for manual and GitHub updates, and a real-time serial log.
 */
const char SYSTEM_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html data-bs-theme="dark">
<head>
  <title>System</title>
  %HEAD%
  <style>
    @import url('https://fonts.googleapis.com/css2?family=VT323&display=swap');
    
    .log-container {
      background-color: #000;
      border-radius: 0.375rem;
      overflow: hidden;
    }
    .log-header {
      background-color: #000;
      padding: 0.5rem 1rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .log-header h5 {
      color: #00FF00;
      text-shadow: 0 0 8px #00FF00;
      margin: 0;
    }
    .log-header .btn {
      background-color: transparent;
      border: 1px solid #00FF00;
      color: #00FF00;
      text-shadow: 0 0 5px #00FF00;
    }
    .log-header .btn:hover {
      background-color: #00FF00;
      color: #000;
      text-shadow: none;
    }
    #log {
      background-color: #000;
      height: 350px;
      padding: 1rem;
      overflow-y: auto;
      white-space: pre-wrap;
      word-wrap: break-word;
      font-family: 'VT323', monospace;
      font-size: 1.2rem;
      color: #00FF00;
      text-shadow: 0 0 2px #00FF00;
    }
    #log::-webkit-scrollbar {
      width: 8px;
    }
    #log::-webkit-scrollbar-track {
      background: #222;
    }
    #log::-webkit-scrollbar-thumb {
      background: #00FF00;
      border-radius: 4px;
    }
    .log-connect { color: #39FF14; }
    .log-disconnect { color: #FFA500; }
    .log-error { color: #FF3131; }
  </style>
</head>
<body>
  <div class="container my-5">
    <div class="card shadow-sm">
      <div class="card-body">
        <h1 class="card-title text-center mb-4">System</h1>
        <ul class="nav nav-tabs nav-fill" id="systemTabs" role="tablist">
          <li class="nav-item" role="presentation">
            <button class="nav-link active" id="update-tab" data-bs-toggle="tab" data-bs-target="#update" type="button" role="tab" aria-controls="update" aria-selected="true">Firmware Update</button>
          </li>
          %SERIAL_LOG_TAB%
        </ul>
        <div class="tab-content p-3 border-top-0 border" id="systemTabsContent">
          <div class="tab-pane fade show active" id="update" role="tabpanel" aria-labelledby="update-tab">
            <div class="card mb-4">
              <div class="card-body">
                <h5 class="card-title d-flex align-items-center"><i class="bi bi-upload me-2"></i>Manual Update</h5>
                <p class="card-text text-muted small">Select a .bin file from your computer to upload and flash.</p>
                <form id="upload-form">
                  <div class="input-group">
                    <input type="file" class="form-control" id="firmware" name="firmware" accept=".bin" required title="Select a .bin firmware file.">
                    <button class="btn btn-primary" type="submit" title="Upload the selected firmware file.">Upload</button>
                  </div>
                </form>
              </div>
            </div>
            <div id="status" class="my-4"></div>
            <div class="card">
              <div class="card-body">
                <h5 class="card-title d-flex align-items-center"><i class="bi bi-cloud-download me-2"></i>Online Update</h5>
                <p class="card-text text-muted small">Check for the latest release and update automatically.</p>
                <div class="d-grid">
                  <button id="online-button" class="btn btn-success" title="Check for and apply updates from the internet.">Check for Updates</button>
                </div>
              </div>
            </div>
            <div class="card mt-4">
              <div class="card-body">
                <h5 class="card-title d-flex align-items-center"><i class="bi bi-clock me-2"></i>Time Synchronization</h5>
                <p class="card-text text-muted small">Manually trigger a time sync with the NTP server.</p>
                <div class="d-grid">
                  <button id="ntp-sync-button" class="btn btn-info">Sync Now</button>
                </div>
                <div id="ntp-sync-status" class="mt-2 text-center"></div>
              </div>
            </div>
          </div>
          %SERIAL_LOG_TAB_PANE%
        </div>
        <div class="card mt-4">
            <div class="card-body">
                <h5 class="card-title text-center mb-3">System Stats</h5>
                <div id="system-stats-container" class="text-center">
                    <div class="row mb-3">
                        <div class="col">
                            <h6><i class="bi bi-cpu-fill me-1"></i> Core Temp</h6>
                            <p class="fs-4 mb-0" id="core-temp"></p>
                        </div>
                        <div class="col">
                            <h6><i class="bi bi-memory me-1"></i> Free RAM</h6>
                            <p class="fs-4 mb-0" id="free-ram"></p>
                        </div>
                    </div>
                    <div class="row">
                        <div class="col">
                            <h6><i class="bi bi-clock-history me-1"></i> Uptime</h6>
                            <p class="fs-4 mb-0" id="uptime"></p>
                        </div>
                        <div class="col">
                            <h6><i class="bi bi-wifi me-1"></i> Wi-Fi RSSI</h6>
                            <p class="fs-4 mb-0" id="wifi-rssi"></p>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <div class="d-grid gap-2 mt-4">
          <a href="/" id="back-button" class="btn btn-secondary" title="Return to the main menu.">Back to Menu</a>
          
          <button type="button" id="reboot-button" class="btn btn-warning w-100 mt-3" onclick="rebootDevice()" title="Reboot the device.">
            Reboot Device
          </button>
          <button type="button" id="factory-reset-keep-wifi-button" class="btn btn-outline-danger w-100 mt-4" onclick="factoryResetExceptWiFi()" title="Reset all settings to factory defaults, but keep WiFi credentials.">
          Factory Reset (keep WiFi)
          </button>
          <button type="button" id="factory-reset-button" class="btn btn-danger w-100 mt-3" onclick="factoryReset()" title="Reset all settings to factory defaults. This will clear EVERYTHING including WiFi credentials.">
           Factory Reset (full wipe)
          </button>
        </div>
      </div>
    </div>
  </div>
  <script>
    const uploadForm = document.getElementById('upload-form');
    const onlineButton = document.getElementById('online-button');
    const ntpSyncButton = document.getElementById('ntp-sync-button');
    const rolloverBtn = document.getElementById('rollover-btn');
    const downloadSystemLogBtn = document.getElementById('download-system-log-btn');
    const logsTab = document.getElementById('serial-log-tab');
    const ntpSyncStatusDiv = document.getElementById('ntp-sync-status');
    const statusDiv = document.getElementById('status');
    const fileInput = document.getElementById('firmware');
    const backButton = document.getElementById('back-button');
    const uploadButton = uploadForm.querySelector('button');
    const rebootBtn = document.getElementById('reboot-button');
    const resetBtn = document.getElementById('factory-reset-button');
    const resetKeepWifiBtn = document.getElementById('factory-reset-keep-wifi-button');
    let isUpdating = false;
    let pollInterval = null;

    const freeRamEl = document.getElementById('free-ram');
    const uptimeEl = document.getElementById('uptime');
    const wifiRssiEl = document.getElementById('wifi-rssi');
    const coreTempEl = document.getElementById('core-temp');

    function updateSystemStats() {
        fetch('/api/system/stats')
            .then(response => response.json())
            .then(data => {
                if (data.freeHeap) {
                    freeRamEl.textContent = `${(data.freeHeap / 1024).toFixed(2)} KB`;
                } else {
                    freeRamEl.textContent = 'N/A';
                }

                if (data.uptime) {
                    const uptime = new Date(data.uptime);
                    const days = Math.floor(data.uptime / (1000 * 60 * 60 * 24));
                    const hours = uptime.getUTCHours();
                    const minutes = uptime.getUTCMinutes();
                    uptimeEl.textContent = `${days}d ${hours}h ${minutes}m`;
                } else {
                    uptimeEl.textContent = 'N/A';
                }

                if (data.rssi) {
                    wifiRssiEl.textContent = `${data.rssi} dBm`;
                } else {
                    wifiRssiEl.textContent = 'N/A';
                }

                if (data.coreTemp) {
                    coreTempEl.textContent = `${data.coreTemp}°${data.unit}`;
                } else {
                    coreTempEl.textContent = 'N/A';
                }
            })
            .catch(error => console.error('Error fetching system stats:', error));
    }

    // Initial update
    updateSystemStats();
    // Update system stats every 5 seconds
    setInterval(updateSystemStats, 5000);

    window.addEventListener('beforeunload', (event) => {
      if (isUpdating) {
        event.preventDefault();
        event.returnValue = '';
      }
    });

    function startPollingStatus() {
      pollInterval = setInterval(() => {
        fetch('/api/update/status')
          .then(r => {
            if (!r.ok) { throw new Error('Network error or device rebooting'); }
            return r.json();
          })
          .then(data => {
            if (data.inProgress) {
              showStatus('Updating firmware... please wait.', 'info');
              setButtonsDisabled(true);
            } else {
              // Update finished, but polling caught it.
              clearInterval(pollInterval);
              showStatus('Update complete. Device will reboot shortly.', 'success');
              setButtonsDisabled(false); // Re-enable buttons now
              isUpdating = false;
            }
          })
          .catch((error) => {
            // Fetch fails if device reboots. This is an expected "success" case.
            clearInterval(pollInterval);
            showStatus('Update complete. Device is rebooting...', 'success');
            setButtonsDisabled(false); // Re-enable buttons now
            isUpdating = false;
          });
      }, 2500);
    }

    function showStatus(message, type = 'info') {
      statusDiv.innerHTML = `<div class="alert alert-${type} d-flex align-items-center" role="alert">
          ${type === 'info' ? '<div class="spinner-border spinner-border-sm me-2" role="status"><span class="visually-hidden">Loading...</span></div>' : ''}
          <div>${message}</div>
        </div>`;
    }

    function setSystemButtonsDisabled(disabled) {
      ntpSyncButton.disabled = disabled;
      if (rebootBtn) rebootBtn.disabled = disabled;
      if (resetBtn) resetBtn.disabled = disabled;
      if (resetKeepWifiBtn) resetKeepWifiBtn.disabled = disabled;
      if (rolloverBtn) rolloverBtn.disabled = disabled;
      if (downloadSystemLogBtn) {
        if (disabled) {
          downloadSystemLogBtn.classList.add('disabled');
        } else {
          downloadSystemLogBtn.classList.remove('disabled');
        }
      }
      if (logsTab) {
        if (disabled) {
          logsTab.classList.add('disabled');
        } else {
          logsTab.classList.remove('disabled');
        }
      }
    }

    function setButtonsDisabled(disabled) {
        uploadButton.disabled = disabled;
        onlineButton.disabled = disabled;
        
        // Disable/enable back button
        if(disabled) {
            backButton.classList.add('disabled');
        } else {
            backButton.classList.remove('disabled');
        }

        setSystemButtonsDisabled(disabled);
    }

    uploadForm.addEventListener('submit', function(e) {
      e.preventDefault();
      if (!fileInput.files.length) {
        showStatus('Please select a firmware file first.', 'warning');
        return;
      }
      showStatus('Uploading firmware... Do not close this page.');
      setButtonsDisabled(true);
      isUpdating = true; // Set flag to prevent navigation

      const formData = new FormData(this);
      fetch('/update', {
        method: 'POST',
        body: formData
      })
      .then(response => {
        if (!response.ok) {
          // Server sent a 500 or other error
          throw new Error(`Server responded with status: ${response.status} ${response.statusText}`);
        }
        return response.text();
      })
      .then(text => {
        // Server sent 200 OK
        if (text.includes('Update successful')) {
            showStatus(text, 'success');
            // Device is rebooting. Re-enable buttons and allow navigation.
            setButtonsDisabled(false);
            isUpdating = false; 
        } else {
            // This means the update FAILED from the server side (e.g., Update.end() failed)
            showStatus(text, 'danger');
            setButtonsDisabled(false);
            isUpdating = false;
        }
      })
      .catch(error => {
        // This catch block will be hit if the server reboots *before* sending a response,
        // OR if the response.ok was false.
        showStatus(`Upload failed: ${error.message}`, 'danger');
        setButtonsDisabled(false);
        isUpdating = false;
      });
    });

    onlineButton.addEventListener('click', function() {
      showStatus('Checking for online updates...', 'info');
      setButtonsDisabled(true);
      isUpdating = true;

      fetch('/api/update/github', { method: 'POST' })
      .then(response => response.text()) // Server always sends 200 OK
      .then(text => {
        // Now analyze the text response from our server
        if (text.includes('Starting update')) {
          showStatus(text, 'info'); // Blue spinner, update is starting
          startPollingStatus();
        } else if (text.toLowerCase().includes('error') || text.toLowerCase().includes('failed')) {
          showStatus(text, 'danger'); // Red alert, show error
          setButtonsDisabled(false);  // Re-enable buttons
          isUpdating = false;
        } else {
          // This is for "No new update found."
          showStatus(text, 'success'); // Green alert, show success
          setButtonsDisabled(false);   // Re-enable buttons
          isUpdating = false;
        }
      })
      .catch(error => {
        // This will only catch actual network failures
        showStatus(`Online update check failed: ${error.message}`, 'danger');
        setButtonsDisabled(false);
        isUpdating = false;
      });
    });

    ntpSyncButton.addEventListener('click', function() {
      setButtonsDisabled(true);

      fetch('/api/system/ntp-sync', { method: 'POST' })
        .then(response => {
          if (response.ok) {
            return response.text();
          }
          throw new Error('Sync request failed.');
        })
        .then(text => {
          ntpSyncStatusDiv.innerHTML = `<span class="text-success">${text}</span>`;
          setTimeout(() => {
            ntpSyncStatusDiv.innerHTML = '';
          }, 5000);
        })
        .catch(error => {
          ntpSyncStatusDiv.innerHTML = `<span class="text-danger">${error.message}</span>`;
        })
        .finally(() => {
          if (!isUpdating) {
            setButtonsDisabled(false);
          }
        });
    });

    if (rolloverBtn) {
      rolloverBtn.addEventListener('click', function() {
        if (confirm('Are you sure you want to manually rollover the log file? The current backup will be overwritten.')) {
          setButtonsDisabled(true);
          fetch('/api/log/rotate', { method: 'POST' })
          .then(response => {
            if (response.ok) {
              alert("Log rotated successfully.");
            } else {
              alert("Failed to rotate log.");
            }
          })
          .catch(e => alert("Error: " + e))
          .finally(() => {
            if (!isUpdating) {
              setButtonsDisabled(false);
            }
          });
        }
      });
    }

    function rebootDevice() {
      if (confirm("Are you sure you want to reboot the device?")) {
        fetch("/reboot").then(res => alert(res.ok ? "Device is rebooting..." : "Failed to reboot."));
      }
    }

    function factoryReset() {
      const confirmationText = "RESET";
      const userInput = prompt(`To confirm, please type "${confirmationText}" in the box below.`);

      if (userInput === confirmationText) {
        fetch("/factory-reset").then(res => alert(res.ok ? "Factory reset successful. Device is rebooting..." : "Failed to reset."));
      } else if (userInput !== null) {
        alert("The text you entered did not match. Factory reset has been canceled.");
      }
    }

    function factoryResetExceptWiFi() {
      const confirmationText = "RESET";
      const userInput = prompt(`To confirm, please type "${confirmationText}" in the box below.`);

      if (userInput === confirmationText) {
        fetch("/factory-reset-except-wifi").then(res => alert(res.ok ? "Factory reset successful. Device is rebooting..." : "Failed to reset."));
      } else if (userInput !== null) {
        alert("The text you entered did not match. Factory reset has been canceled.");
      }
    }
  </script>
  %SERIAL_LOG_SCRIPT%
  <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>
</body>
</html>
)rawliteral";

#endif // WEB_CONTENT_H