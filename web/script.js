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

    // WebSocket for servo control
    const ws = new WebSocket(`ws://${location.host}/ws`);

    function sendCmd(cmd) {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(cmd);
        }
    }

    document.getElementById('btn-x-plus').addEventListener('click', () => sendCmd('X+'));
    document.getElementById('btn-x-minus').addEventListener('click', () => sendCmd('X-'));
    document.getElementById('btn-y-plus').addEventListener('click', () => sendCmd('Y+'));
    document.getElementById('btn-y-minus').addEventListener('click', () => sendCmd('Y-'));

    document.addEventListener('keydown', ev => {
        if (ev.key === 'ArrowUp') sendCmd('Y+');
        else if (ev.key === 'ArrowDown') sendCmd('Y-');
        else if (ev.key === 'ArrowLeft') sendCmd('X-');
        else if (ev.key === 'ArrowRight') sendCmd('X+');
    });
})();
