// Prefix unik harus persis dengan kode ESP32
const TOPIC_PREFIX = "abin_esp32s3";
const TOPIC_STATUS = `${TOPIC_PREFIX}/status`;
const TOPIC_DHT    = `${TOPIC_PREFIX}/sensor/dht`;
const TOPIC_DIST   = `${TOPIC_PREFIX}/sensor/distance`;
const TOPIC_TELE   = `${TOPIC_PREFIX}/telemetry`;
const TOPIC_RGB    = `${TOPIC_PREFIX}/led/rgb`;
const TOPIC_CMD    = `${TOPIC_PREFIX}/cmd`;
const TOPIC_LOG    = `${TOPIC_PREFIX}/debug/log`;

// Menggunakan EMQX Public WebSocket broker
// Port 8083 untuk ws (unsecure), atau 8084 untuk wss (secure/SSL)
const brokerUrl = "wss://broker.emqx.io:8084/mqtt"; 

appendLog("[SYSTEM] Menghubungkan ke EMQX Public Broker via SSL WebSockets...", "gray");

const client = mqtt.connect(brokerUrl, {
    clientId: `laravel_dashboard_${Math.random().toString(16).substr(2, 8)}`,
    clean: true,
    connectTimeout: 4000,
    reconnectPeriod: 2000,
});

// --- HANDLER EVENTS MQTT ---

client.on('connect', () => {
    appendLog("[SYSTEM] Terkoneksi ke MQTT Broker!", "emerald");
    updateWebStatus(true);
    
    // Subscribe ke semua topik sensor, status ESP, dan log
    client.subscribe([TOPIC_STATUS, TOPIC_DHT, TOPIC_DIST, TOPIC_TELE, TOPIC_LOG], (err) => {
        if (!err) {
            appendLog("[SYSTEM] Sukses subscribe ke semua topik ESP32-S3.", "emerald");
        }
    });
});

client.on('offline', () => {
    appendLog("[SYSTEM] Koneksi Broker Terputus!", "red");
    updateWebStatus(false);
    updateEspStatus(false);
});

client.on('message', (topic, payload) => {
    const message = payload.toString();

    // 1. STATUS ONLINE/OFFLINE ESP32 (LWT)
    if (topic === TOPIC_STATUS) {
        if (message === "online") {
            updateEspStatus(true);
            appendLog("[SYSTEM] ESP32-S3 STATUS: ONLINE", "emerald");
        } else {
            updateEspStatus(false);
            appendLog("[SYSTEM] ESP32-S3 STATUS: OFFLINE", "red");
        }
    }

    // 2. DATA SENSOR DHT11 (JSON)
    else if (topic === TOPIC_DHT) {
        updateEspStatus(true); // Automatically set to online when data is received
        try {
            const data = JSON.parse(message);
            document.getElementById('temp-val').innerText = data.temp;
            document.getElementById('humi-val').innerText = data.humi;
        } catch (e) {
            console.error("Gagal parse DHT JSON:", e);
        }
    }

    // 3. DATA SENSOR HC-SR04
    else if (topic === TOPIC_DIST) {
        updateEspStatus(true); // Automatically set to online when data is received
        const dist = parseFloat(message);
        document.getElementById('dist-val').innerText = dist.toFixed(1);
        
        // Animasi progress bar berdasarkan jarak (asumsi maks 100 cm untuk visualisasi)
        let percent = Math.min((dist / 100) * 100, 100);
        document.getElementById('dist-bar').style.width = `${percent}%`;
    }

    // 4. TELEMETRI CORE/ESP SYSTEM INFO
    else if (topic === TOPIC_TELE) {
        updateEspStatus(true); // Automatically set to online when data is received
        try {
            const data = JSON.parse(message);
            
            // Core temperature
            document.getElementById('tele-core').innerText = `${data.core_temp.toFixed(1)} °C`;
            
            // Heap Memory RAM
            const freeKB = (data.free_heap / 1024).toFixed(1);
            const totalKB = (data.total_heap / 1024).toFixed(1);
            const usedPercent = ((data.total_heap - data.free_heap) / data.total_heap) * 100;
            document.getElementById('tele-heap-text').innerText = `${freeKB} / ${totalKB} KB Free`;
            document.getElementById('tele-heap-bar').style.width = `${100 - usedPercent}%`;
            
            // Wi-Fi Signal Strength RSSI
            const rssi = data.rssi;
            const rssiEl = document.getElementById('tele-rssi');
            const badgeEl = document.getElementById('tele-wifi-badge');
            rssiEl.innerText = `${rssi} dBm`;
            
            if (rssi >= -60) {
                badgeEl.innerText = "Excellent";
                badgeEl.className = "px-1.5 py-0.5 rounded text-[10px] font-bold bg-emerald-500/10 text-emerald-400";
            } else if (rssi >= -75) {
                badgeEl.innerText = "Good";
                badgeEl.className = "px-1.5 py-0.5 rounded text-[10px] font-bold bg-yellow-500/10 text-yellow-400";
            } else {
                badgeEl.innerText = "Weak";
                badgeEl.className = "px-1.5 py-0.5 rounded text-[10px] font-bold bg-red-500/10 text-red-400";
            }

            // CPU Clock & Uptime
            document.getElementById('tele-cpu').innerText = `${data.cpu} MHz`;
            document.getElementById('tele-uptime').innerText = formatUptime(data.uptime);

            // IP Address & OTA Form Action Setup
            if (data.ip) {
                const teleIpEl = document.getElementById('tele-ip');
                if (teleIpEl) teleIpEl.innerText = data.ip;
                
                const otaForm = document.getElementById('ota-form');
                const otaSubmitBtn = document.getElementById('ota-submit-btn');
                if (otaForm && otaSubmitBtn) {
                    otaForm.action = `http://${data.ip}/update`;
                    otaSubmitBtn.disabled = false;
                    otaSubmitBtn.className = "w-full bg-indigo-600 hover:bg-indigo-500 text-black font-extrabold py-2 rounded-lg text-[10px] tracking-wider cursor-pointer transition-all";
                }
            }

        } catch (e) {
            console.error("Gagal parse Telemetry JSON:", e);
        }
    }

    // 5. LIVE LOGS SERIAL DARI ESP32
    else if (topic === TOPIC_LOG) {
        updateEspStatus(true); // Automatically set to online when data is received
        appendLog(message, "amber");
    }
});

// --- LOGIC FUNGSI KONTROL LED RGB & COMMAND ---

let sliderR, sliderG, sliderB, colorPicker;

document.addEventListener('DOMContentLoaded', () => {
    sliderR = document.getElementById('slider-r');
    sliderG = document.getElementById('slider-g');
    sliderB = document.getElementById('slider-b');
    colorPicker = document.getElementById('color-picker');

    sliderR.addEventListener('input', publishRGBThrottled);
    sliderG.addEventListener('input', publishRGBThrottled);
    sliderB.addEventListener('input', publishRGBThrottled);

    // Handler Color Picker Input
    colorPicker.addEventListener('input', (e) => {
        const hex = e.target.value;
        // Konversi HEX ke RGB
        const r = parseInt(hex.slice(1, 3), 16);
        const g = parseInt(hex.slice(3, 5), 16);
        const b = parseInt(hex.slice(5, 7), 16);
        
        sliderR.value = r;
        sliderG.value = g;
        sliderB.value = b;
        publishRGB();
    });

    // Dukungan Enter Key di input command
    document.getElementById('cmd-input').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') sendCommand();
    });

    // Handler form upload firmware OTA secara lokal (bukan tab baru)
    const otaForm = document.getElementById('ota-form');
    if (otaForm) {
        otaForm.addEventListener('submit', (e) => {
            e.preventDefault();
            
            const fileInput = otaForm.querySelector('input[name="update"]');
            if (!fileInput.files.length) return;
            
            const submitBtn = document.getElementById('ota-submit-btn');
            const file = fileInput.files[0];
            const espIp = document.getElementById('tele-ip').innerText;
            
            if (espIp === "--") {
                appendLog("[SYSTEM] Error: ESP32 sedang offline!", "red");
                return;
            }
            
            // Set Loading state
            submitBtn.disabled = true;
            submitBtn.innerText = "UPLOADING FIRMWARE...";
            submitBtn.className = "w-full bg-indigo-800 text-indigo-300 font-extrabold py-2 rounded-lg text-[10px] tracking-wider transition-all";
            appendLog(`[SYSTEM] Memulai proses upload OTA ke ESP32 di IP: ${espIp}...`, "sky");
            
            const formData = new FormData();
            formData.append('update', file);
            formData.append('ip', espIp);
            formData.append('username', otaForm.querySelector('input[name="username"]').value);
            formData.append('password', otaForm.querySelector('input[name="password"]').value);
            
            const csrfToken = otaForm.querySelector('input[name="_token"]').value;
            
            fetch('/ota-proxy', {
                method: 'POST',
                headers: {
                    'X-CSRF-TOKEN': csrfToken
                },
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    appendLog(`[SYSTEM] OTA SUCCESS: ${data.message}`, "emerald");
                    alert(data.message);
                } else {
                    appendLog(`[SYSTEM] OTA FAILED: ${data.message}`, "red");
                    alert(`OTA Gagal: ${data.message}`);
                }
            })
            .catch(err => {
                console.error("Error OTA:", err);
                appendLog(`[SYSTEM] OTA ERROR: Gagal mengirim request ke server proxy.`, "red");
                alert("OTA Error: Gagal menghubungi server proxy.");
            })
            .finally(() => {
                submitBtn.disabled = false;
                submitBtn.innerText = "UPLOAD FIRMWARE";
                submitBtn.className = "w-full bg-indigo-600 hover:bg-indigo-500 text-black font-extrabold py-2 rounded-lg text-[10px] tracking-wider cursor-pointer transition-all";
                fileInput.value = '';
            });
        });
    }
});

// Fungsi publish warna LED RGB ke MQTT
function publishRGB() {
    const r = sliderR.value;
    const g = sliderG.value;
    const b = sliderB.value;
    
    // Update preview warna di Web
    document.getElementById('r-val').innerText = r;
    document.getElementById('g-val').innerText = g;
    document.getElementById('b-val').innerText = b;
    document.getElementById('color-preview').style.backgroundColor = `rgb(${r},${g},${b})`;
    
    // Kirim payload string "R,G,B"
    client.publish(TOPIC_RGB, `${r},${g},${b}`);
}

// Throttling untuk menghindari kelebihan muatan MQTT saat menggeser slider dengan cepat
let throttleTimer = null;
function publishRGBThrottled() {
    if (!throttleTimer) {
        throttleTimer = setTimeout(() => {
            publishRGB();
            throttleTimer = null;
        }, 80); // Mengirim maksimal setiap 80ms
    }
}

// Fungsi Tombol Preset Cepat
function setPreset(r, g, b) {
    if (!sliderR) {
        sliderR = document.getElementById('slider-r');
        sliderG = document.getElementById('slider-g');
        sliderB = document.getElementById('slider-b');
        colorPicker = document.getElementById('color-picker');
    }
    sliderR.value = r;
    sliderG.value = g;
    sliderB.value = b;
    // Sinkronkan Color Picker hex
    const toHex = (c) => ("0" + c.toString(16)).slice(-2);
    colorPicker.value = `#${toHex(r)}${toHex(g)}${toHex(b)}`;
    publishRGB();
}

// Kirim Command Custom dari Konsol
function sendCommand() {
    const inputEl = document.getElementById('cmd-input');
    const cmd = inputEl.value.trim();
    if (cmd) {
        client.publish(TOPIC_CMD, cmd);
        appendLog(`[KONSOL-WEB] Mengirim perintah -> "${cmd}"`, "sky");
        inputEl.value = '';
    }
}

// --- UTILITY FUNCTIONS ---

function updateWebStatus(isOnline) {
    const dot = document.getElementById('web-status-dot');
    const text = document.getElementById('web-status-text');
    if (dot && text) {
        if (isOnline) {
            dot.className = "w-2.5 h-2.5 rounded-full bg-emerald-500 inline-block";
            text.className = "font-semibold text-emerald-400";
            text.innerText = "Connected";
        } else {
            dot.className = "w-2.5 h-2.5 rounded-full bg-red-500 inline-block";
            text.className = "font-semibold text-red-400";
            text.innerText = "Offline";
        }
    }
}

function updateEspStatus(isOnline) {
    const dot = document.getElementById('esp-status-dot');
    const text = document.getElementById('esp-status-text');
    if (dot && text) {
        if (isOnline) {
            dot.className = "w-2.5 h-2.5 rounded-full bg-emerald-500 inline-block";
            text.className = "font-semibold text-emerald-400";
            text.innerText = "Online";
        } else {
            dot.className = "w-2.5 h-2.5 rounded-full bg-red-500 inline-block";
            text.className = "font-semibold text-red-400";
            text.innerText = "Disconnected";
            
            // Kosongkan indikator sensor jika ESP offline
            document.getElementById('temp-val').innerText = "--";
            document.getElementById('humi-val').innerText = "--";
            document.getElementById('dist-val').innerText = "--";
            document.getElementById('dist-bar').style.width = "0%";

            // Kosongkan IP & Nonaktifkan OTA Button jika offline
            const teleIpEl = document.getElementById('tele-ip');
            if (teleIpEl) teleIpEl.innerText = "--";
            
            const otaSubmitBtn = document.getElementById('ota-submit-btn');
            if (otaSubmitBtn) {
                otaSubmitBtn.disabled = true;
                otaSubmitBtn.className = "w-full bg-indigo-600/50 cursor-not-allowed text-white font-extrabold py-2 rounded-lg text-[10px] tracking-wider transition-all";
            }
        }
    }
}

let autoscrollEnabled = true;

function toggleAutoscroll() {
    autoscrollEnabled = !autoscrollEnabled;
    const btn = document.getElementById('autoscroll-toggle');
    if (btn) {
        if (autoscrollEnabled) {
            btn.innerText = "📌 Auto-Scroll: ON";
            btn.className = "text-[10px] text-emerald-400 bg-emerald-950/50 hover:bg-emerald-900/50 border border-emerald-800/30 px-2 py-1 rounded transition-all";
            
            const terminal = document.getElementById('terminal');
            if (terminal) terminal.scrollTop = terminal.scrollHeight;
        } else {
            btn.innerText = "📌 Auto-Scroll: OFF";
            btn.className = "text-[10px] text-gray-400 bg-gray-800/50 hover:bg-gray-700/50 border border-gray-700/30 px-2 py-1 rounded transition-all";
        }
    }
}

function appendLog(text, colorClass) {
    const terminal = document.getElementById('terminal');
    if (!terminal) return;
    const line = document.createElement('div');
    
    // Berikan warna teks berdasarkan status log
    if (colorClass === "emerald") line.className = "text-emerald-400";
    else if (colorClass === "red") line.className = "text-red-500";
    else if (colorClass === "amber") line.className = "text-amber-400";
    else if (colorClass === "sky") line.className = "text-sky-400";
    else line.className = "text-gray-400";

    line.innerText = text;
    terminal.appendChild(line);
    
    // Auto-scroll ke bawah terminal log jika diaktifkan
    if (autoscrollEnabled) {
        terminal.scrollTop = terminal.scrollHeight;
    }
}

function clearConsole() {
    const terminal = document.getElementById('terminal');
    if (terminal) {
        terminal.innerHTML = '<div class="text-gray-500">[SYSTEM] Terminal dibersihkan.</div>';
    }
}

function formatUptime(seconds) {
    const hrs = Math.floor(seconds / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${hrs}j ${mins}m ${secs}d`;
}

// Expose functions globally for HTML onclick event handlers
window.setPreset = setPreset;
window.clearConsole = clearConsole;
window.sendCommand = sendCommand;
window.toggleAutoscroll = toggleAutoscroll;
