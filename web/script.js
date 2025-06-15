(function() {
    const liveTable = document.querySelector('#live-table tbody');
    const lastUpdate = document.getElementById('last-update');
    const themeToggle = document.getElementById('theme-toggle');

    let theme = localStorage.getItem('theme');
    if(!theme) {
        theme = window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
    }
    document.body.classList.toggle('dark', theme === 'dark');
    themeToggle.addEventListener('click', () => {
        theme = theme === 'dark' ? 'light' : 'dark';
        document.body.classList.toggle('dark', theme === 'dark');
        localStorage.setItem('theme', theme);
    });

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
        if(lastUpdate) lastUpdate.textContent = new Date().toLocaleTimeString();
    }

    function fetchLive() {
        fetch('/api/live').then(r => r.json()).then(updateLive).catch(() => {});
    }

    setInterval(fetchLive, 5000);
    fetchLive();

    function loadSettings() {
        fetch('/api/settings')
            .then(r => r.json())
            .then(data => {
                document.getElementById('site-name').value = data.siteName || '';
                const wifi = data.wifi || {};
                document.getElementById('wifi-ssid').value = wifi.ssid || '';
                document.getElementById('wifi-pass').value = wifi.password || '';
                const mqtt = data.mqtt || {};
                document.getElementById('mqtt-host').value = mqtt.host || '';
                document.getElementById('mqtt-port').value = mqtt.port || '';
                document.getElementById('mqtt-user').value = mqtt.user || '';
                document.getElementById('mqtt-pass').value = mqtt.pass || '';
                document.getElementById('mqtt-qos').value = mqtt.qos || 0;
                const thr = data.thresholds || {};
                if (thr.lidar) {
                    document.getElementById('lidar-min').value = thr.lidar.min;
                    document.getElementById('lidar-max').value = thr.lidar.max;
                }
                if (thr.smoke) {
                    document.getElementById('smoke-min').value = thr.smoke.min;
                    document.getElementById('smoke-max').value = thr.smoke.max;
                }
                if (thr.eco2) {
                    document.getElementById('eco2-min').value = thr.eco2.min;
                    document.getElementById('eco2-max').value = thr.eco2.max;
                }
                if (thr.tvoc) {
                    document.getElementById('tvoc-min').value = thr.tvoc.min;
                    document.getElementById('tvoc-max').value = thr.tvoc.max;
                }
                if (thr.pressure) {
                    document.getElementById('pressure-min').value = thr.pressure.min;
                    document.getElementById('pressure-max').value = thr.pressure.max;
                }
                const clog = data.clog || {};
                document.getElementById('clog-min').value = clog.clogMin || '';
                document.getElementById('clog-hold').value = clog.clogHold || '';
                document.getElementById('debug-enable').checked = !!data.debugEnable;
                document.getElementById('ui-user').value = data.uiUser || '';
            })
            .catch(() => {});
    }

    loadSettings();

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
                },
                pressure: {
                    min: Number(document.getElementById('pressure-min').value),
                    max: Number(document.getElementById('pressure-max').value)
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
