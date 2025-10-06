// pages.h

#ifndef PAGES_H
#define PAGES_H

// Common header for all pages, includes Bootstrap 5 from a CDN
const char BOOTSTRAP_HEAD[] PROGMEM = R"rawliteral(
<meta name="viewport" content="width=device-width, initial-scale=1">
<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet">
<style>
  body { background-color: #f0f2f5; }
  .container { max-width: 500px; }
</style>
)rawliteral";

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
        </div>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

const char WIFI_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>WiFi Configuration</title>
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
        <h1 class="card-title text-center mb-4">Configure WiFi</h1>
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
        <div class="d-grid mt-4">
          <a href="/" class="btn btn-secondary">Back to Menu</a>
        </div>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

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
        
          <div class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
            <label class="form-check-label" for="auto-brightness">Auto Brightness</label>
            <input class="form-check-input" type="checkbox" role="switch" id="auto-brightness" name="autoBrightness" %AUTO_BRIGHTNESS_CHECKED%>
          </div>

          <div class="mb-3 p-3 border rounded">
            <label for="brightness" class="form-label">Manual Brightness</label>
            <input type="range" class="form-range" id="brightness" name="brightness" min="10" max="255" value="%BRIGHTNESS_VALUE%">
          </div>
          
          <div class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
            <label class="form-check-label" for="24hour">24-Hour Format</label>
            <input class="form-check-input" type="checkbox" role="switch" id="24hour" name="use24HourFormat" %IS_24_HOUR_CHECKED%>
          </div>
          
          <div class="form-check form-switch mb-3 p-3 border rounded d-flex justify-content-between align-items-center">
            <label class="form-check-label" for="celsius">Use Celsius (&deg;C)</label>
            <input class="form-check-input" type="checkbox" role="switch" id="celsius" name="useCelsius" %USE_CELSIUS_CHECKED%>
          </div>

          <div class="d-grid gap-2 mt-4">
            <button type="submit" class="btn btn-success">Save Settings</button>
            <a href="/" class="btn btn-secondary">Back to Menu</a>
          </div>
        </form>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";
#endif // PAGES_H
