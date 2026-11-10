# Firmware Lobsense — ESP32-S3

Firmware PlatformIO untuk membaca sensor kualitas air dan mengirim payload JSON
parameter WQMS melalui MQTT. Firmware tidak menggunakan serial sebagai jalur
telemetri, sensor MPU, atau sensor salinitas fisik.

## Wi-Fi dan MQTT

Wi-Fi menggunakan library custom `MakerindoWiFiManager` dari project
`Firmware Agrisense V2.1`. Saat belum memiliki jaringan tersimpan atau gagal
tersambung, perangkat membuat access point `Lobsense-Setup`. Hubungkan
ponsel/komputer ke access point itu dan buka alamat portal yang tercetak di
Serial Monitor (umumnya `http://192.168.4.1`). Portal dapat memindai jaringan
yang tersedia, menerima SSID/password, dan menyimpan kredensial di Preferences
ESP32 untuk reboot berikutnya.

Salin `include/secret.example.h` menjadi `include/secret.h`, lalu sesuaikan
password access point, alamat/port broker, username/password MQTT, topic,
serial number, dan cage code. `include/secret.h` diabaikan Git. Konfigurasi awal
menggunakan broker publik `broker.emqx.io:1883` dan topic `lobsense/wqms`;
ganti dengan broker WQMS yang digunakan deployment. Ganti juga password AP
default `change-this`; broker publik tanpa TLS/autentikasi hanya untuk uji coba.

Firmware mempertahankan bentuk JSON telemetry yang sama dan mem-publish satu
payload per detik ke topic yang ditetapkan. Serial USB dipakai hanya untuk pesan diagnostik; payload telemetri tidak
dikirim lewat USB CDC maupun UART.

## Mode dummy untuk pengujian

Mode dummy aktif secara default pada `platformio.ini`
(`-DUSE_DUMMY_DATA=1`). Firmware mengirim nilai simulasi yang berubah setiap
detik melalui MQTT, tanpa memerlukan sensor terhubung. Nilai sensor dan telemetri
daya diacak di dalam rentang simulasi yang masuk akal; status pompa berganti
setiap 30 detik dan laju aliran mengikuti statusnya. Payload tidak menyertakan
field `data_source`; nilai dummy harus dibedakan dari pembacaan nyata melalui
konfigurasi firmware. Timestamp dikirim sebagai Unix timestamp dalam detik dari
RTC DS3231; jika RTC tidak terhubung atau waktunya belum valid, nilainya `null`.
Untuk kembali membaca sensor sungguhan, ubah flag menjadi `-DUSE_DUMMY_DATA=0`,
lalu build dan unggah ulang.

## Sensor dan pin

| Fungsi | Pin ESP32-S3 / kanal |
| --- | --- |
| I2C SDA / SCL | GPIO 8 / GPIO 9 |
| DS18B20 suhu air | GPIO 13 |
| Flow meter | GPIO 4 |
| ADS1115 pH | A0 |
| ADS1115 dissolved oxygen | A1 (belum dikonversi ke mg/L) |
| ADS1115 turbidity | A2 |
| Aerator | GPIO 5, aktif-HIGH |
| Aktuator pakan | GPIO 6, aktif-HIGH |
| RTC DS3231 | I2C, alamat `0x68` |

GPIO aktuator harus mengendalikan modul relay/MOSFET yang sesuai, bukan beban
aktuator secara langsung. Pastikan level aktif modul cocok dengan konfigurasi
aktif-HIGH di firmware.

Serial Monitor dapat digunakan untuk melihat diagnostik koneksi Wi-Fi dan MQTT;
serial tidak lagi menjadi jalur telemetri.

## Format payload MQTT

Setiap detik firmware mem-publish satu objek JSON ke topic MQTT, tanpa
`data_source` dan tanpa data MPU (`pitch`, `roll`, `yaw`):

```json
{"serial_number":"IOT-NODE-001","timestamp":1729504800,"cage_code":"CAGE-A01","latitude":-8.652930,"longitude":116.319520,"altitude":1.2,"raw_values":{"temperature_node":27.45,"ambient_temperature":30.10,"ph":7.82,"tds":420.5,"raw_dissolved_oxygen":6.15,"turbidity":4.10,"flow_rate":0.28,"solar_voltage":18.2,"solar_current":1.45,"battery_voltage":12.6,"battery_current":0.65,"pump_status":1}}
```

Dalam mode sensor (`USE_DUMMY_DATA=0`), timestamp, GPS, suhu ambient, TDS,
DO mg/L, dan telemetri daya/pompa yang belum memiliki pembacaan atau kalibrasi
di firmware akan dikirim sebagai `null`. Tegangan mentah probe DO tidak
dilabeli mg/L. EDGE/mini PC dapat mengisi timestamp atau data eksternal yang
tersedia.

## Kontrol aktuator

- Aerator GPIO 5 menyala bila nilai DO valid dalam mg/L kurang dari 5.0; bila
  nilai tidak valid atau sudah mencapai ambang, aerator dimatikan. Saat ini
  konversi sensor DO ADS1115 A1 belum dikalibrasi, sehingga mode sensor mengirim
  `null` dan kontrol aerator tetap mati sampai konversi mg/L tersedia. Nilai DO
  dummy 6.15 mg/L juga tidak menyalakan aerator.
- Aktuator pakan GPIO 6 menyala selama 3 detik pada pukul 08:00 dan 17:00 setiap
  hari. Jadwal menggunakan RTC DS3231; atur RTC ke waktu lokal WIB
  (`Asia/Jakarta`) dan pastikan flag oscillator-stop (OSF) sudah bersih. Jika
  RTC tidak terhubung atau waktunya belum valid, pakan tidak dijalankan.

## Kalibrasi

- Rumus pH menggunakan koefisien dari firmware sumber (`-5.70 * volt + 21.69`).
  Kalibrasikan ulang menggunakan larutan buffer dan probe yang dipakai.
- Turbidity menggunakan rumus yang sama dengan firmware sumber; hasil negatif
  dibatasi ke nol.
- Flow meter menggunakan asumsi 450 pulsa/liter. Sesuaikan
  `FLOW_PULSES_PER_LITER` dengan tipe dan kalibrasi flow meter sebenarnya.
- Nilai DO, TDS, lokasi, dan daya pada mode dummy adalah data simulasi dengan
  rentang bervariasi, bukan pembacaan sensor atau lokasi sebenarnya. Mode sensor
  mengirim `null` untuk nilai-nilai tersebut sampai input sensor, unit, dan
  kalibrasinya ditentukan. Khusus DO, kalibrasi mg/L dibutuhkan sebelum kontrol
  aerator mode sensor dapat beroperasi.

## Build

1. Salin `include/secret.example.h` ke `include/secret.h` dan sesuaikan
   konfigurasi.
2. Buka folder ini di VS Code dengan ekstensi PlatformIO terpasang.
3. Jalankan **PlatformIO: Build** untuk environment `esp32-s3-devkitc-1`.
4. Unggah firmware. Pantau status koneksi melalui Serial Monitor (115200 baud).

Library sensor akan diunduh PlatformIO sesuai `platformio.ini`.
