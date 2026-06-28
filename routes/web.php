<?php

use Illuminate\Support\Facades\Route;

Route::get('/', function () {
    return view('welcome');
});

Route::post('/ota-proxy', function (\Illuminate\Http\Request $request) {
    $request->validate([
        'ip' => 'required|ip',
        'update' => 'required|file',
        'username' => 'required|string',
        'password' => 'required|string'
    ]);

    $ip = $request->input('ip');
    $file = $request->file('update');
    $username = $request->input('username');
    $password = $request->input('password');

    try {
        $authBase64 = base64_encode("{$username}:{$password}");

        // Kirim request ke ESP32 OTA Server menggunakan Laravel Http Client
        $response = \Illuminate\Support\Facades\Http::timeout(90) // OTA update can take some time
            ->withHeaders([
                'Authorization' => "Basic {$authBase64}"
            ])
            ->attach('update', file_get_contents($file->getRealPath()), $file->getClientOriginalName())
            ->post("http://{$ip}/update");

        if ($response->successful()) {
            return response()->json([
                'success' => true,
                'message' => 'Success! ESP32 is rebooting...'
            ]);
        }

        return response()->json([
            'success' => false,
            'message' => 'Upload failed (Status ' . $response->status() . '): ' . $response->body()
        ], 500);

    } catch (\Exception $e) {
        return response()->json([
            'success' => false,
            'message' => 'Exception: ' . $e->getMessage() . ' at ' . $e->getFile() . ':' . $e->getLine()
        ], 500);
    }
});
