<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 Real-Time Dashboard</title>
    <script src="https://unpkg.com/mqtt/dist/mqtt.min.js"></script>
    @vite(['resources/css/app.css', 'resources/js/app.js'])
</head>
<body class="bg-gray-950 text-gray-100 min-h-screen flex flex-col font-sans selection:bg-emerald-500 selection:text-black">

    <header class="border-b border-gray-800 bg-gray-900/50 backdrop-blur-md sticky top-0 z-50 px-6 py-4 flex flex-col sm:flex-row justify-between items-center gap-4">
        <div class="flex items-center gap-3">
            <div class="w-10 h-10 rounded-xl bg-emerald-500/10 border border-emerald-500/30 flex items-center justify-center text-emerald-400 font-bold text-xl shadow-lg shadow-emerald-500/10 animate-pulse">
                S3
            </div>
            <div>
                <h1 class="text-lg font-bold tracking-tight">ESP32-S3 IoT Console</h1>
                <p class="text-xs text-gray-400">Laravel & MQTT Real-Time Framework</p>
            </div>
        </div>
        
        <div class="flex items-center gap-4">
            <div class="flex items-center gap-2 bg-gray-800/80 px-3  py-1.5 rounded-full border border-gray-700 text-xs">
                <span class="text-gray-400">Web Client:</span>
                <span id="web-status-dot" class="w-2.5 h-2.5 rounded-full bg-red-500 inline-block"></span>
                <span id="web-status-text" class="font-semibold text-red-400">Offline</span>
            </div>
            <div class="flex items-center gap-2 bg-gray-800/80 px-3 py-1.5 rounded-full border border-gray-700 text-xs">
                <span class="text-gray-400">ESP32-S3:</span>
                <span id="esp-status-dot" class="w-2.5 h-2.5 rounded-full bg-red-500 inline-block"></span>
                <span id="esp-status-text" class="font-semibold text-red-400">Disconnected</span>
            </div>
        </div>
    </header>

    <main class="flex-1 p-6 max-w-7xl mx-auto w-full grid grid-cols-1 lg:grid-cols-12 gap-6">
        
        <div class="lg:col-span-8 flex flex-col gap-6">
            
            <div class="grid grid-cols-1 md:grid-cols-3 gap-6">
                <div class="bg-gray-900 border border-gray-800 rounded-2xl p-6 shadow-xl relative overflow-hidden group">
                    <div class="absolute top-0 right-0 w-32 h-32 bg-orange-500/5 rounded-full blur-3xl group-hover:bg-orange-500/10 transition-all"></div>
                    <div class="flex justify-between items-start mb-4">
                        <span class="text-gray-400 text-sm font-semibold tracking-wide">TEMPERATURE</span>
                        <span class="p-2 rounded-lg bg-orange-500/10 text-orange-400">🌡️</span>
                    </div>
                    <div class="flex items-baseline gap-1">
                        <span id="temp-val" class="text-4xl font-extrabold text-orange-400 tracking-tight">--</span>
                        <span class="text-xl font-bold text-gray-500">°C</span>
                    </div>
                    <div class="mt-4 text-xs text-gray-500 flex items-center gap-1">
                        <span>DHT11 Core Sensor</span>
                    </div>
                </div>

                <div class="bg-gray-900 border border-gray-800 rounded-2xl p-6 shadow-xl relative overflow-hidden group">
                    <div class="absolute top-0 right-0 w-32 h-32 bg-cyan-500/5 rounded-full blur-3xl group-hover:bg-cyan-500/10 transition-all"></div>
                    <div class="flex justify-between items-start mb-4">
                        <span class="text-gray-400 text-sm font-semibold tracking-wide">HUMIDITY</span>
                        <span class="p-2 rounded-lg bg-cyan-500/10 text-cyan-400">💧</span>
                    </div>
                    <div class="flex items-baseline gap-1">
                        <span id="humi-val" class="text-4xl font-extrabold text-cyan-400 tracking-tight">--</span>
                        <span class="text-xl font-bold text-gray-500">%</span>
                    </div>
                    <div class="mt-4 text-xs text-gray-500">Relative Humidity</div>
                </div>

                <div class="bg-gray-900 border border-gray-800 rounded-2xl p-6 shadow-xl relative overflow-hidden group">
                    <div class="absolute top-0 right-0 w-32 h-32 bg-purple-500/5 rounded-full blur-3xl group-hover:bg-purple-500/10 transition-all"></div>
                    <div class="flex justify-between items-start mb-4">
                        <span class="text-gray-400 text-sm font-semibold tracking-wide">DISTANCE</span>
                        <span class="p-2 rounded-lg bg-purple-500/10 text-purple-400">📏</span>
                    </div>
                    <div class="flex items-baseline gap-1">
                        <span id="dist-val" class="text-4xl font-extrabold text-purple-400 tracking-tight">--</span>
                        <span class="text-xl font-bold text-gray-500">cm</span>
                    </div>
                    <div class="w-full bg-gray-800 rounded-full h-1.5 mt-4">
                        <div id="dist-bar" class="bg-purple-500 h-1.5 rounded-full transition-all duration-300" style="width: 0%"></div>
                    </div>
                </div>
            </div>

            <div class="bg-gray-900 border border-gray-800 rounded-2xl p-6 shadow-xl">
                <div class="flex items-center justify-between border-b border-gray-800 pb-4 mb-6">
                    <div>
                        <h2 class="text-base font-bold">RGB LED Controller (Common Cathode)</h2>
                        <p class="text-xs text-gray-400">Kustomisasi warna LED RGB Anda via PWM Sliders</p>
                    </div>
                    <div id="color-preview" class="w-10 h-10 rounded-full border-2 border-gray-700 bg-black shadow-lg transition-all"></div>
                </div>

                <div class="grid grid-cols-1 md:grid-cols-2 gap-8">
                    <div class="flex flex-col gap-5">
                        <div>
                            <div class="flex justify-between text-xs font-semibold mb-2 text-red-400">
                                <span>RED CHANNEL</span>
                                <span id="r-val">0</span>
                            </div>
                            <input type="range" id="slider-r" min="0" max="255" value="0" 
                                class="w-full h-2 bg-gray-800 rounded-lg appearance-none cursor-pointer accent-red-500">
                        </div>

                        <div>
                            <div class="flex justify-between text-xs font-semibold mb-2 text-green-400">
                                <span>GREEN CHANNEL</span>
                                <span id="g-val">0</span>
                            </div>
                            <input type="range" id="slider-g" min="0" max="255" value="0" 
                                class="w-full h-2 bg-gray-800 rounded-lg appearance-none cursor-pointer accent-green-500">
                        </div>

                        <div>
                            <div class="flex justify-between text-xs font-semibold mb-2 text-blue-400">
                                <span>BLUE CHANNEL</span>
                                <span id="b-val">0</span>
                            </div>
                            <input type="range" id="slider-b" min="0" max="255" value="0" 
                                class="w-full h-2 bg-gray-800 rounded-lg appearance-none cursor-pointer accent-blue-500">
                        </div>
                    </div>

                    <div class="flex flex-col justify-between border-t md:border-t-0 md:border-l border-gray-800 pt-6 md:pt-0 md:pl-6">
                        <div>
                            <label class="block text-xs font-semibold text-gray-400 mb-3">PALET WARNA INSTAN</label>
                            <div class="grid grid-cols-4 gap-2">
                                <button onclick="setPreset(255, 0, 0)" class="bg-red-900/50 hover:bg-red-500 hover:text-black border border-red-500/30 text-red-400 text-xs py-2 rounded-lg font-bold transition-all">Merah</button>
                                <button onclick="setPreset(0, 255, 0)" class="bg-green-900/50 hover:bg-green-500 hover:text-black border border-green-500/30 text-green-400 text-xs py-2 rounded-lg font-bold transition-all">Hijau</button>
                                <button onclick="setPreset(0, 0, 255)" class="bg-blue-900/50 hover:bg-blue-500 hover:text-black border border-blue-500/30 text-blue-400 text-xs py-2 rounded-lg font-bold transition-all">Biru</button>
                                <button onclick="setPreset(255, 255, 0)" class="bg-yellow-900/50 hover:bg-yellow-500 hover:text-black border border-yellow-500/30 text-yellow-400 text-xs py-2 rounded-lg font-bold transition-all">Kuning</button>
                                <button onclick="setPreset(255, 0, 255)" class="bg-purple-900/50 hover:bg-purple-500 hover:text-black border border-purple-500/30 text-purple-400 text-xs py-2 rounded-lg font-bold transition-all">Ungu</button>
                                <button onclick="setPreset(0, 255, 255)" class="bg-cyan-900/50 hover:bg-cyan-500 hover:text-black border border-cyan-500/30 text-cyan-400 text-xs py-2 rounded-lg font-bold transition-all">Sian</button>
                                <button onclick="setPreset(255, 255, 255)" class="bg-gray-800 hover:bg-white hover:text-black border border-gray-600 text-gray-200 text-xs py-2 rounded-lg font-bold transition-all">Putih</button>
                                <button onclick="setPreset(0, 0, 0)" class="bg-black hover:bg-gray-800 border border-gray-800 text-red-500 text-xs py-2 rounded-lg font-bold transition-all">OFF</button>
                            </div>
                        </div>

                        <div class="mt-4 flex items-center justify-between bg-gray-800/40 p-3 rounded-xl border border-gray-800">
                            <span class="text-xs font-semibold text-gray-300">Color Picker Input:</span>
                            <input type="color" id="color-picker" class="w-12 h-8 rounded cursor-pointer border-0 bg-transparent">
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <div class="lg:col-span-4 flex flex-col gap-6">
            
            <div class="bg-gray-900 border border-gray-800 rounded-2xl p-6 shadow-xl">
                <h2 class="text-base font-bold mb-4 flex items-center gap-2">
                    <span class="text-emerald-400">⚡</span> ESP32-S3 System Monitor
                </h2>
                
                <div class="flex flex-col gap-3.5 text-xs">
                    <div class="flex justify-between items-center bg-gray-900/50 p-2.5 rounded-xl border border-gray-800">
                        <span class="text-gray-400">Suhu Internal CPU:</span>
                        <span id="tele-core" class="font-bold text-gray-200">-- °C</span>
                    </div>

                    <div class="bg-gray-900/50 p-2.5 rounded-xl border border-gray-800">
                        <div class="flex justify-between items-center mb-1.5">
                            <span class="text-gray-400">Free Memory (RAM):</span>
                            <span id="tele-heap-text" class="font-bold text-gray-200">-- / -- KB</span>
                        </div>
                        <div class="w-full bg-gray-800 rounded-full h-1.5">
                            <div id="tele-heap-bar" class="bg-emerald-500 h-1.5 rounded-full" style="width: 0%"></div>
                        </div>
                    </div>

                    <div class="flex justify-between items-center bg-gray-900/50 p-2.5 rounded-xl border border-gray-800">
                        <span class="text-gray-400">Kekuatan Wi-Fi RSSI:</span>
                        <div class="flex items-center gap-1.5">
                            <span id="tele-rssi" class="font-bold text-emerald-400">-- dBm</span>
                            <span id="tele-wifi-badge" class="px-1.5 py-0.5 rounded text-[10px] font-bold bg-emerald-500/10 text-emerald-400">OK</span>
                        </div>
                    </div>

                    <div class="flex justify-between items-center bg-gray-900/50 p-2.5 rounded-xl border border-gray-800">
                        <span class="text-gray-400">CPU Frequency Clock:</span>
                        <span id="tele-cpu" class="font-bold text-purple-400">-- MHz</span>
                    </div>

                    <div class="flex justify-between items-center bg-gray-900/50 p-2.5 rounded-xl border border-gray-800">
                        <span class="text-gray-400">System Uptime:</span>
                        <span id="tele-uptime" class="font-bold text-cyan-400">--</span>
                    </div>
                </div>
            </div>

            <div class="bg-gray-900 border border-gray-800 rounded-2xl p-6 shadow-xl">
                <h2 class="text-base font-bold mb-4 flex items-center gap-2">
                    <span class="text-indigo-400">🔄</span> Firmware Update (Web OTA)
                </h2>
                
                <div class="flex flex-col gap-3.5 text-xs">
                    <div class="flex justify-between items-center bg-gray-900/50 p-2.5 rounded-xl border border-gray-800">
                        <span class="text-gray-400">ESP32 IP Address:</span>
                        <span id="tele-ip" class="font-bold text-indigo-400">--</span>
                    </div>

                    <div class="bg-gray-900/50 p-3 rounded-xl border border-gray-800 flex flex-col gap-2">
                        <span class="text-gray-400 font-semibold mb-1">Upload Firmware (.bin):</span>
                        <form id="ota-form" class="flex flex-col gap-2">
                            @csrf
                            <div class="flex gap-2 mb-1">
                                <input type="text" name="username" placeholder="Username OTA" required
                                    class="w-1/2 bg-black text-gray-200 border border-gray-800 rounded-lg px-2.5 py-1.5 text-[10px] focus:outline-none focus:border-indigo-500">
                                <input type="password" name="password" placeholder="Password OTA" required
                                    class="w-1/2 bg-black text-gray-200 border border-gray-800 rounded-lg px-2.5 py-1.5 text-[10px] focus:outline-none focus:border-indigo-500">
                            </div>
                            <input type="file" name="update" required
                                class="w-full text-xs text-gray-400 file:mr-2 file:py-1 file:px-2 file:rounded-md file:border-0 file:text-[10px] file:font-semibold file:bg-indigo-950 file:text-indigo-300 hover:file:bg-indigo-900 cursor-pointer">
                            <button type="submit" id="ota-submit-btn" disabled
                                class="w-full bg-indigo-600/50 cursor-not-allowed text-white font-extrabold py-2 rounded-lg text-[10px] tracking-wider transition-all">
                                UPLOAD FIRMWARE
                            </button>
                        </form>
                    </div>
                </div>
            </div>

            <div class="bg-gray-900 border border-gray-800 rounded-2xl p-6 shadow-xl flex flex-col">
                <div class="flex items-center justify-between border-b border-gray-800 pb-3 mb-4">
                    <div class="flex items-center gap-2">
                        <span class="w-2.5 h-2.5 rounded-full bg-emerald-500 inline-block animate-ping"></span>
                        <h2 class="text-sm font-bold tracking-wider">LIVE DEBUG LOGS</h2>
                    </div>
                    <div class="flex items-center gap-2">
                        <button id="autoscroll-toggle" onclick="toggleAutoscroll()" class="text-[10px] text-emerald-400 bg-emerald-950/50 hover:bg-emerald-900/50 border border-emerald-800/30 px-2 py-1 rounded transition-all">
                            📌 Auto-Scroll: ON
                        </button>
                        <button onclick="clearConsole()" class="text-[10px] text-gray-400 hover:text-white bg-gray-800 hover:bg-gray-700 px-2 py-1 rounded transition-all">Clear</button>
                    </div>
                </div>

                <div id="terminal" class="bg-black text-green-400 font-mono text-xs p-4 h-64 overflow-y-auto rounded-xl border border-gray-800 space-y-1">
                    <div class="text-gray-500">[SYSTEM] Menunggu koneksi broker...</div>
                </div>

                <div class="mt-4 flex gap-2">
                    <input type="text" id="cmd-input" placeholder="Ketik cmd: reboot, ping, led_off..." 
                        class="flex-1 bg-black text-green-400 font-mono text-xs border border-gray-800 px-3 py-2.5 rounded-lg focus:outline-none focus:border-green-500">
                    <button onclick="sendCommand()" class="bg-green-600 hover:bg-green-500 text-black font-extrabold text-xs px-4 rounded-lg transition-all">KIRIM</button>
                </div>
            </div>

        </div>

    </main>

    <footer class="text-center py-4 border-t border-gray-800 text-[10px] text-gray-500 bg-gray-950/80">
        Created for ESP32-S3 and Laravel Realtime Control System © 2026.
    </footer>

</body>
</html>