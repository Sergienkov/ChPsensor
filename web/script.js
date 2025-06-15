(function() {
    const liveTable = document.querySelector('#live-table tbody');

    function updateLive(data) {
        const row = document.createElement('tr');
        row.innerHTML = [
            data.lidar,
            data.smoke,
            data.eco2,
            data.tvoc,
            data.temp,
            data.rh,
            data.x,
            data.y
        ].map(v => `<td>${v}</td>`).join('');
        liveTable.innerHTML = '';
        liveTable.appendChild(row);
    }

    function fetchLive() {
        fetch('/api/live').then(r => r.json()).then(updateLive).catch(() => {});
    }

    setInterval(fetchLive, 5000);
    fetchLive();

    function fillSettings(data) {
        document.getElementById('site-name').value = data.siteName || '';
        document.getElementById('wifi-ssid').value = data.wifi.ssid || '';
        document.getElementById('wifi-pass').value = data.wifi.password || '';
        document.getElementById('mqtt-host').value = data.mqtt.host || '';
        document.getElementById('mqtt-port').value = data.mqtt.port || 1883;
        document.getElementById('mqtt-user').value = data.mqtt.user || '';
        document.getElementById('mqtt-pass').value = data.mqtt.pass || '';
        document.getElementById('mqtt-qos').value = data.mqtt.qos || 0;
        document.getElementById('lidar-min').value = data.thresholds.lidar.min;
        document.getElementById('lidar-max').value = data.thresholds.lidar.max;
        document.getElementById('smoke-min').value = data.thresholds.smoke.min;
        document.getElementById('smoke-max').value = data.thresholds.smoke.max;
        document.getElementById('eco2-min').value = data.thresholds.eco2.min;
        document.getElementById('eco2-max').value = data.thresholds.eco2.max;
        document.getElementById('tvoc-min').value = data.thresholds.tvoc.min;
        document.getElementById('tvoc-max').value = data.thresholds.tvoc.max;
        document.getElementById('clog-min').value = data.clog.clogMin;
        document.getElementById('clog-hold').value = data.clog.clogHold;
        document.getElementById('debug-enable').checked = data.debugEnable;
        document.getElementById('ui-user').value = data.uiUser;
    }

    fetch('/api/settings').then(r => r.json()).then(fillSettings).catch(() => {});

    // Settings submit
    const settingsForm = document.getElementById('settings-form');
    settingsForm.addEventListener('submit', ev => {
        ev.preventDefault();
        const data = {
            siteName: document.getElementById('site-name').value,
            wifi: {
                ssid: document.getElementById('wifi-ssid').value,
                password: document.getElementById('wifi-pass').value
            },
            mqtt: {
                host: document.getElementById('mqtt-host').value,
                port: Number(document.getElementById('mqtt-port').value),
                user: document.getElementById('mqtt-user').value,
                pass: document.getElementById('mqtt-pass').value,
                qos: Number(document.getElementById('mqtt-qos').value)
            },
            thresholds: {
                lidar: {
                    min: Number(document.getElementById('lidar-min').value),
                    max: Number(document.getElementById('lidar-max').value)
                },
                smoke: {
                    min: Number(document.getElementById('smoke-min').value),
                    max: Number(document.getElementById('smoke-max').value)
                },
                eco2: {
                    min: Number(document.getElementById('eco2-min').value),
                    max: Number(document.getElementById('eco2-max').value)
                },
                tvoc: {
                    min: Number(document.getElementById('tvoc-min').value),
                    max: Number(document.getElementById('tvoc-max').value)
                }
            },
            clog: {
                clogMin: Number(document.getElementById('clog-min').value),
                clogHold: Number(document.getElementById('clog-hold').value)
            },
            debugEnable: document.getElementById('debug-enable').checked,
            uiUser: document.getElementById('ui-user').value
        };
        fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
    });

    // Password change
    const pwForm = document.getElementById('password-form');
    pwForm.addEventListener('submit', ev => {
        ev.preventDefault();
        const data = { password: document.getElementById('new-password').value };
        fetch('/api/password', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
    });

    // OTA modal
    const otaModal = document.getElementById('ota-modal');
    const otaButton = document.getElementById('ota-button');
    const otaCancel = document.getElementById('ota-cancel');

    otaButton.addEventListener('click', () => otaModal.classList.add('show'));
    otaCancel.addEventListener('click', () => otaModal.classList.remove('show'));

    // WebSocket servo control
    const ws = new WebSocket(`ws://${location.host}/ws`);
    document.getElementById('x-plus').addEventListener('click', () => ws.send('X+'));
    document.getElementById('x-minus').addEventListener('click', () => ws.send('X-'));
    document.getElementById('y-plus').addEventListener('click', () => ws.send('Y+'));
    document.getElementById('y-minus').addEventListener('click', () => ws.send('Y-'));
})();
