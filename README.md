# 🏡 Smart Home Patio - Sistem Jemuran Otomatis & Keamanan Teras berbasis ESP32

Proyek Internet of Things (IoT) menggunakan mikrokontroler **ESP32** untuk mengotomatisasi buka-tutup kanopi jemuran berdasarkan kondisi cuaca (cahaya, kelembapan udara, dan suhu ekstrem), pencahayaan lampu teras otomatis, serta sistem keamanan teras berbasis sensor gerak *PIR* dan *Buzzer* yang terintegrasi secara *real-time* dengan **Web Dashboard** melalui broker **EMQX Cloud (MQTT/WSS)**.

---

## 🚀 Fitur Utama
1. **Kanopi Jemuran Otomatis Adaptif Cuaca (Motor Servo):** Membuka dan menutup atap jemuran secara otomatis berdasarkan kombinasi intensitas cahaya, kelembapan udara (potensi hujan), dan batas suhu panas berlebih ($> 40^\circ\text{C}$) agar pakaian terlindungi.
2. **Pencahayaan Teras Otomatis (LED Kuning):** Menyalakan lampu teras secara otomatis saat lingkungan mulai gelap/malam hari dan memadamkannya saat pagi hari untuk efisiensi energi.
3. **Sistem Keamanan Perimeter Teras (Sensor PIR & Buzzer):** Mendeteksi pergerakan di area teras dan langsung menyalakan indikator bahaya (LED Merah) serta peringatan suara (Buzzer).
4. **Mode Ganda (`AUTO` vs `MANUAL` Web Override):** Mendukung kendali jarak jauh secara *real-time* melalui *Web Dashboard* (via MQTT) maupun sakelar tombol fisik di rangkaian tanpa bentrok dengan logika otomatis.
5. **Tampilan LCD I2C (16x2) & Serial Monitor Terstruktur:** Menampilkan status suhu, kelembapan, mode operasi, posisi atap kanopi, serta kondisi keamanan secara langsung di perangkat maupun terminal debugging.

---

## 🛠️ Komponen yang Digunakan
- **Mikrokontroler:** ESP32 DevKit V1
- **Sensor Suhu & Kelembapan Udara:** DHT22 (`GPIO 15`)
- **Sensor Intensitas Cahaya:** LDR / Photoresistor (terhubung ke pin analog `GPIO 34`)
- **Sensor Gerak:** PIR Motion Sensor (`GPIO 13`)
- **Tombol Mode Fisik:** Push Button dengan `INPUT_PULLUP` (`GPIO 32`)
- **Aktuator / Output:**
  - Kanopi Jemuran / Motor Servo (`GPIO 14`)
  - Lampu Teras / LED Kuning (`GPIO 27`)
  - Indikator Alarm / LED Merah (`GPIO 33`)
  - Buzzer Alarm (`GPIO 25` — PWM Channel 7)
- **Display:** LCD 16x2 dengan Modul I2C (`Address: 0x27`, `SDA: GPIO 21`, `SCL: GPIO 22`)

---

## 📌 Logika Kerja Sistem
1. **Cuaca Cerah & Normal (`LDR < 2000`, `Kelembapan < 80%`, `Suhu <= 40°C`)** ➔ Kanopi Servo **Terbuka (`90°`)** (Pakaian dijemur).
2. **Cuaca Ekstrem / Malam (`LDR >= 2000` atau `Kelembapan >= 80%` atau `Suhu > 40°C`)** ➔ Kanopi Servo **Tertutup (`0°`)** (Melindungi jemuran dari gelap, potensi hujan, atau panas berlebih).
3. **Hari Gelap / Malam (`LDR >= 2000`)** ➔ Lampu Teras (LED Kuning) **Menyala**.
4. **Gerakan Terdeteksi (`PIR == HIGH`)** ➔ LED Merah **Menyala** + **Buzzer Berbunyi**.
5. **Kendali Manual (`auto == 0`)** ➔ Aktif otomatis saat pengguna mengirim perintah kontrol dari *Web Dashboard*, dan dapat dikembalikan ke mode otomatis (`auto == 1`) lewat *website* maupun tombol fisik (`GPIO 32`).

---

## 📦 Pustaka (Library) yang Dibutuhkan
Proyek ini menggunakan **PlatformIO IDE** di Visual Studio Code dengan daftar *library* berikut pada `platformio.ini`:
- `PubSubClient` (oleh Nick O'Leary) — Komunikasi protokol MQTT
- `ArduinoJson` (oleh Benoit Blanchon) — Serialisasi dan parsing payload JSON
- `DHT sensor library` & `Adafruit Unified Sensor` (oleh Adafruit) — Pembacaan sensor DHT22
- `ESP32Servo` (oleh Kevin Harrington) — Kontrol sudut motor servo pada ESP32
- `LiquidCrystal_I2C` (oleh Frank de Brabander) — Antarmuka layar LCD 16x2 I2C

---

## ⚙️ Cara Instalasi & Penggunaan
1. Susun rangkaian pada **Wokwi Simulator** (`diagram.json`) sesuai definisi pin pada kode `src/main.cpp`:
   - `PIN_DHT` ➔ `GPIO 15`
   - `PIN_LDR` ➔ `GPIO 34`
   - `PIN_PIR` ➔ `GPIO 13`
   - `PIN_BTN` ➔ `GPIO 32`
   - `PIN_SERVO` ➔ `GPIO 14`
   - `PIN_LED_Y` ➔ `GPIO 27` | `PIN_LED_R` ➔ `GPIO 33`
   - `PIN_BUZZER` ➔ `GPIO 25`
   - `SDA` / `SCL` ➔ Pin I2C default ESP32 (`GPIO 21` / `GPIO 22`)
2. Buka folder proyek di **Visual Studio Code** dengan ekstensi **PlatformIO IDE** dan **Wokwi Simulator**.
3. Sesuaikan alamat broker dan kredensial **EMQX Cloud** pada `src/main.cpp` (Port MQTTS `8883` untuk ESP32, topik `smarthome/telemetry` dan `smarthome/control`).
4. Klik ikon **Build (✓)** pada PlatformIO hingga menghasilkan `firmware.bin`, lalu buka `diagram.json` dan tekan tombol **Play (▶)** untuk memulai simulasi.
5. Jalankan `web/index.html` melalui **Live Server** (atau browser), hubungkan ke broker EMQX menggunakan port WebSocket Secure (`8084`), dan pantau **Serial Monitor** pada kecepatan `115200 baud`.

---

## 💡 Catatan Tambahan
- **Kesesuaian Ambang Batas Sensor:** Nilai ambang batas cahaya (`LDR_DARK_THRESHOLD = 2000`) pada `src/main.cpp` diselaraskan dengan konstanta `LDR_TH = 2000` pada `web/script.js` agar status *Terang/Gelap* antara perangkat dan *Web Dashboard* selalu sinkron.
- **Konfigurasi Buzzer PWM:** Buzzer menggunakan `ledcSetup(7, 1000, 8)` pada channel PWM 7 agar tidak terjadi konflik timer dengan pustaka `ESP32Servo`.