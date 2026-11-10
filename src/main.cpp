#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <DallasTemperature.h>
#include <MakerindoWiFiManager.h>
#include <OneWire.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_system.h>
#include <math.h>

#include "secret.h"

#ifndef USE_DUMMY_DATA
#define USE_DUMMY_DATA 0
#endif

namespace {

constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;
constexpr uint8_t ONE_WIRE_PIN = 13;
constexpr uint8_t FLOW_PIN = 4;
constexpr uint8_t RTC_I2C_ADDRESS = 0x68;

constexpr uint32_t TELEMETRY_INTERVAL_MS = 1000;
constexpr uint32_t ADS_RETRY_INTERVAL_MS = 5000;
constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000;
constexpr uint32_t TEMPERATURE_CONVERSION_MS = 750;
constexpr char WIFI_PREFERENCES_NAMESPACE[] = "lobsense-wifi";
constexpr int32_t RTC_UTC_OFFSET_SECONDS = 7 * 60 * 60;
constexpr float PH_SLOPE = -5.70f;
constexpr float PH_INTERCEPT = 21.69f;
constexpr float FLOW_PULSES_PER_LITER = 450.0f;

Adafruit_ADS1115 ads;
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature waterTemperatureSensor(&oneWire);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
MakerindoWiFiManager wifiManager;

portMUX_TYPE flowPulseMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t flowPulseCount = 0;

bool adsAvailable = false;
bool temperatureConversionPending = false;
bool waterTemperatureValid = false;
float waterTemperatureC = 0.0f;
uint32_t temperatureConversionStartedAt = 0;
uint32_t lastAdsRetryAt = 0;
uint32_t lastTelemetryAt = 0;
uint32_t lastMqttRetryAt = 0;
char mqttClientId[48];

void IRAM_ATTR countFlowPulse() {
  portENTER_CRITICAL_ISR(&flowPulseMux);
  ++flowPulseCount;
  portEXIT_CRITICAL_ISR(&flowPulseMux);
}

uint32_t takeFlowPulses() {
  portENTER_CRITICAL(&flowPulseMux);
  const uint32_t pulses = flowPulseCount;
  flowPulseCount = 0;
  portEXIT_CRITICAL(&flowPulseMux);
  return pulses;
}

bool initializeAds() {
  if (!ads.begin(0x48, &Wire)) {
    return false;
  }

  ads.setGain(GAIN_ONE);
  return true;
}

bool isValidAdcReading(int16_t reading) {
  return reading > 0 && reading < 32767;
}

uint8_t bcdToDecimal(uint8_t value) {
  return static_cast<uint8_t>((value >> 4) * 10 + (value & 0x0F));
}

bool readRtcRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(RTC_I2C_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(
                                               RTC_I2C_ADDRESS,
                                               static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  constexpr uint8_t DAYS_PER_MONTH[] = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};
  if (month == 2 &&
      (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
    return 29;
  }
  return DAYS_PER_MONTH[month - 1];
}

bool isLeapYear(uint16_t year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

bool readRtcUnixTimestamp(uint32_t &timestamp) {
  Wire.beginTransmission(RTC_I2C_ADDRESS);
  Wire.write(static_cast<uint8_t>(0));
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(RTC_I2C_ADDRESS, static_cast<uint8_t>(7)) != 7) {
    return false;
  }

  const uint8_t secondsRegister = Wire.read();
  const uint8_t minutesRegister = Wire.read();
  const uint8_t hoursRegister = Wire.read();
  Wire.read();
  const uint8_t dayOfMonthRegister = Wire.read();
  const uint8_t monthRegister = Wire.read();
  const uint8_t yearRegister = Wire.read();

  uint8_t rtcStatus = 0;
  if (!readRtcRegister(0x0F, rtcStatus) || (rtcStatus & 0x80) != 0) {
    return false;
  }

  const uint8_t second = bcdToDecimal(secondsRegister & 0x7F);
  const uint8_t minute = bcdToDecimal(minutesRegister & 0x7F);
  uint8_t hour = 0;
  if ((hoursRegister & 0x40) != 0) {
    const uint8_t hour12 = bcdToDecimal(hoursRegister & 0x1F);
    hour = static_cast<uint8_t>((hour12 % 12) +
                                ((hoursRegister & 0x20) != 0 ? 12 : 0));
  } else {
    hour = bcdToDecimal(hoursRegister & 0x3F);
  }

  const uint8_t dayOfMonth = bcdToDecimal(dayOfMonthRegister & 0x3F);
  const uint8_t month = bcdToDecimal(monthRegister & 0x1F);
  const uint16_t year =
      static_cast<uint16_t>(2000 + bcdToDecimal(yearRegister));
  if (second > 59 || minute > 59 || hour > 23 || month < 1 || month > 12 ||
      dayOfMonth < 1 || dayOfMonth > daysInMonth(year, month)) {
    return false;
  }

  int64_t daysSinceEpoch = 0;
  for (uint16_t calendarYear = 1970; calendarYear < year; ++calendarYear) {
    daysSinceEpoch += isLeapYear(calendarYear) ? 366 : 365;
  }
  for (uint8_t calendarMonth = 1; calendarMonth < month; ++calendarMonth) {
    daysSinceEpoch += daysInMonth(year, calendarMonth);
  }
  daysSinceEpoch += dayOfMonth - 1;

  const int64_t utcTimestamp =
      daysSinceEpoch * 86400 + hour * 3600 + minute * 60 + second -
      RTC_UTC_OFFSET_SECONDS;
  if (utcTimestamp < 0 ||
      utcTimestamp > static_cast<int64_t>(UINT32_MAX)) {
    return false;
  }

  timestamp = static_cast<uint32_t>(utcTimestamp);
  return true;
}

void formatNullableFloat(char *buffer, size_t bufferSize, bool valid,
                         float value, uint8_t decimals) {
  if (!valid || !isfinite(value)) {
    snprintf(buffer, bufferSize, "null");
    return;
  }

  snprintf(buffer, bufferSize, "%.*f", decimals, value);
}

#if USE_DUMMY_DATA
float randomFloat(float minimum, float maximum) {
  constexpr long RANDOM_SCALE = 10000;
  const float fraction =
      static_cast<float>(random(0, RANDOM_SCALE + 1)) / RANDOM_SCALE;
  return minimum + fraction * (maximum - minimum);
}
#endif

void serviceWaterTemperature(uint32_t now) {
  if (temperatureConversionPending) {
    if (static_cast<uint32_t>(now - temperatureConversionStartedAt) <
        TEMPERATURE_CONVERSION_MS) {
      return;
    }

    const float reading = waterTemperatureSensor.getTempCByIndex(0);
    waterTemperatureValid =
        isfinite(reading) && reading != DEVICE_DISCONNECTED_C &&
        reading != 85.0f && reading >= -55.0f && reading <= 125.0f;
    if (waterTemperatureValid) {
      waterTemperatureC = reading;
    }
    temperatureConversionPending = false;
  }

  waterTemperatureSensor.requestTemperatures();
  temperatureConversionStartedAt = now;
  temperatureConversionPending = true;
}

void sendTelemetry(uint32_t now, uint32_t elapsedMs) {
  char timestampField[32];
  char latitudeField[16];
  char longitudeField[16];
  char altitudeField[16];
  char temperatureNodeField[16];
  char ambientTemperatureField[16];
  char phField[16];
  char tdsField[16];
  char dissolvedOxygenField[16];
  char turbidityField[16];
  char flowRateField[16];
  char solarVoltageField[16];
  char solarCurrentField[16];
  char batteryVoltageField[16];
  char batteryCurrentField[16];
  char pumpStatusField[8];

  bool phValid = false;
  bool turbidityValid = false;
  float ph = 0.0f;
  float turbidityNtu = 0.0f;

#if USE_DUMMY_DATA
  uint32_t rtcTimestamp = 0;
  if (readRtcUnixTimestamp(rtcTimestamp)) {
    snprintf(timestampField, sizeof(timestampField), "%lu",
             static_cast<unsigned long>(rtcTimestamp));
  } else {
    snprintf(timestampField, sizeof(timestampField), "null");
    Serial.println("RTC tidak valid/tidak terbaca; timestamp dikirim null.");
  }
  const bool pumpRunning = ((now / 30000) % 2) == 0;
  formatNullableFloat(latitudeField, sizeof(latitudeField), true,
                      randomFloat(-8.653000f, -8.652860f), 6);
  formatNullableFloat(longitudeField, sizeof(longitudeField), true,
                      randomFloat(116.319450f, 116.319590f), 6);
  formatNullableFloat(altitudeField, sizeof(altitudeField), true,
                      randomFloat(0.8f, 1.6f), 1);
  formatNullableFloat(temperatureNodeField, sizeof(temperatureNodeField),
                      true, randomFloat(26.8f, 28.2f), 2);
  formatNullableFloat(ambientTemperatureField,
                      sizeof(ambientTemperatureField), true,
                      randomFloat(29.0f, 31.2f), 2);
  formatNullableFloat(phField, sizeof(phField), true,
                      randomFloat(7.5f, 8.1f), 2);
  formatNullableFloat(tdsField, sizeof(tdsField), true,
                      randomFloat(400.0f, 440.0f), 1);
  formatNullableFloat(dissolvedOxygenField, sizeof(dissolvedOxygenField),
                      true, randomFloat(5.7f, 6.5f), 2);
  formatNullableFloat(turbidityField, sizeof(turbidityField), true,
                      randomFloat(3.5f, 4.8f), 2);
  formatNullableFloat(flowRateField, sizeof(flowRateField), true,
                      pumpRunning ? randomFloat(0.2f, 0.35f)
                                  : randomFloat(0.0f, 0.03f),
                      2);
  formatNullableFloat(solarVoltageField, sizeof(solarVoltageField), true,
                      randomFloat(17.5f, 19.0f), 1);
  formatNullableFloat(solarCurrentField, sizeof(solarCurrentField), true,
                      randomFloat(1.1f, 1.8f), 2);
  formatNullableFloat(batteryVoltageField, sizeof(batteryVoltageField), true,
                      randomFloat(12.2f, 12.8f), 1);
  formatNullableFloat(batteryCurrentField, sizeof(batteryCurrentField), true,
                      randomFloat(0.3f, 0.9f), 2);
  snprintf(pumpStatusField, sizeof(pumpStatusField), "%u",
           pumpRunning ? 1U : 0U);
#else
  uint32_t rtcTimestamp = 0;
  if (readRtcUnixTimestamp(rtcTimestamp)) {
    snprintf(timestampField, sizeof(timestampField), "%lu",
             static_cast<unsigned long>(rtcTimestamp));
  } else {
    snprintf(timestampField, sizeof(timestampField), "null");
    Serial.println("RTC tidak valid/tidak terbaca; timestamp dikirim null.");
  }
  snprintf(latitudeField, sizeof(latitudeField), "null");
  snprintf(longitudeField, sizeof(longitudeField), "null");
  snprintf(altitudeField, sizeof(altitudeField), "null");
  snprintf(ambientTemperatureField, sizeof(ambientTemperatureField), "null");
  snprintf(tdsField, sizeof(tdsField), "null");
  snprintf(dissolvedOxygenField, sizeof(dissolvedOxygenField), "null");
  snprintf(solarVoltageField, sizeof(solarVoltageField), "null");
  snprintf(solarCurrentField, sizeof(solarCurrentField), "null");
  snprintf(batteryVoltageField, sizeof(batteryVoltageField), "null");
  snprintf(batteryCurrentField, sizeof(batteryCurrentField), "null");
  snprintf(pumpStatusField, sizeof(pumpStatusField), "null");

  formatNullableFloat(temperatureNodeField, sizeof(temperatureNodeField),
                      waterTemperatureValid, waterTemperatureC, 2);

  if (adsAvailable) {
    const int16_t phAdc = ads.readADC_SingleEnded(0);
    const int16_t turbidityAdc = ads.readADC_SingleEnded(2);

    if (isValidAdcReading(phAdc)) {
      const float phVoltage = ads.computeVolts(phAdc);
      ph = PH_SLOPE * phVoltage + PH_INTERCEPT;
      phValid = isfinite(ph) && ph >= 0.0f && ph <= 14.0f;
    }

    if (isValidAdcReading(turbidityAdc)) {
      const float voltage = ads.computeVolts(turbidityAdc);
      turbidityNtu =
          -1120.4f * voltage * voltage + 5742.3f * voltage - 4352.9f;
      if (turbidityNtu < 0.0f) {
        turbidityNtu = 0.0f;
      }
      turbidityValid = isfinite(turbidityNtu);
    }
  }

  formatNullableFloat(phField, sizeof(phField), phValid, ph, 2);
  formatNullableFloat(turbidityField, sizeof(turbidityField), turbidityValid,
                      turbidityNtu, 2);

  const uint32_t pulses = takeFlowPulses();
  const float flowLitersPerMinute =
      elapsedMs > 0
          ? (static_cast<float>(pulses) * 60000.0f) /
                (static_cast<float>(elapsedMs) * FLOW_PULSES_PER_LITER)
          : 0.0f;
  formatNullableFloat(flowRateField, sizeof(flowRateField), true,
                      flowLitersPerMinute, 2);
#endif

  char payload[768];
  const int payloadLength = snprintf(
      payload, sizeof(payload),
      "{\"serial_number\":\"%s\",\"timestamp\":%s,"
      "\"cage_code\":\"%s\",\"latitude\":%s,\"longitude\":%s,"
      "\"altitude\":%s,\"raw_values\":{"
      "\"temperature_node\":%s,\"ambient_temperature\":%s,\"ph\":%s,"
      "\"tds\":%s,\"raw_dissolved_oxygen\":%s,\"turbidity\":%s,"
      "\"flow_rate\":%s,\"solar_voltage\":%s,\"solar_current\":%s,"
      "\"battery_voltage\":%s,"
      "\"battery_current\":%s,\"pump_status\":%s}}",
      WQMS_SERIAL_NUMBER, timestampField, WQMS_CAGE_CODE, latitudeField,
      longitudeField, altitudeField,
      temperatureNodeField, ambientTemperatureField, phField, tdsField,
      dissolvedOxygenField, turbidityField, flowRateField, solarVoltageField,
      solarCurrentField, batteryVoltageField, batteryCurrentField,
      pumpStatusField);

  if (payloadLength < 0 ||
      static_cast<size_t>(payloadLength) >= sizeof(payload)) {
    Serial.println("Telemetri tidak dikirim: ukuran payload melebihi buffer.");
    return;
  }

  if (!mqttClient.connected()) {
    Serial.println("Telemetri tidak dikirim: MQTT belum tersambung.");
    return;
  }

  if (!mqttClient.publish(MQTT_TOPIC, payload)) {
    Serial.println("Gagal mengirim telemetri ke broker MQTT.");
  }
}

void initializeMqtt() {
  mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  mqttClient.setBufferSize(768);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(2);

  uint8_t macAddress[6];
  esp_read_mac(macAddress, ESP_MAC_WIFI_STA);
  snprintf(mqttClientId, sizeof(mqttClientId), "%s-%02X%02X%02X",
           MQTT_CLIENT_ID_PREFIX, macAddress[3], macAddress[4],
           macAddress[5]);
}

void serviceMqtt(uint32_t now) {
  if (!wifiManager.isConnected()) {
    return;
  }

  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  if (static_cast<uint32_t>(now - lastMqttRetryAt) <
      MQTT_RETRY_INTERVAL_MS) {
    return;
  }
  lastMqttRetryAt = now;

  const bool hasCredentials = MQTT_USERNAME[0] != '\0';
  const bool connected = hasCredentials
                             ? mqttClient.connect(mqttClientId, MQTT_USERNAME,
                                                  MQTT_PASSWORD)
                             : mqttClient.connect(mqttClientId);
  if (connected) {
    Serial.println("MQTT tersambung.");
  } else {
    Serial.printf("MQTT gagal tersambung, state=%d\n", mqttClient.state());
  }
}

}  // namespace

void setup() {
  randomSeed(esp_random());

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);

  Serial.begin(115200);
  if (!wifiManager.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD,
                         WIFI_PREFERENCES_NAMESPACE)) {
    Serial.println("[WiFiManager] Portal tidak berhasil dimulai sepenuhnya.");
  }
  initializeMqtt();

  const uint32_t now = millis();
#if !USE_DUMMY_DATA
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), countFlowPulse, RISING);

  waterTemperatureSensor.begin();
  waterTemperatureSensor.setResolution(12);
  waterTemperatureSensor.setWaitForConversion(false);

  adsAvailable = initializeAds();
  lastAdsRetryAt = now;
  serviceWaterTemperature(now);
#endif
  lastTelemetryAt = now;
}

void loop() {
  const uint32_t now = millis();
  wifiManager.loop();
  serviceMqtt(now);

#if !USE_DUMMY_DATA
  serviceWaterTemperature(now);

  if (!adsAvailable &&
      static_cast<uint32_t>(now - lastAdsRetryAt) >= ADS_RETRY_INTERVAL_MS) {
    lastAdsRetryAt = now;
    adsAvailable = initializeAds();
  }
#endif

  const uint32_t elapsedMs = now - lastTelemetryAt;
  if (elapsedMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryAt = now;
    sendTelemetry(now, elapsedMs);
  }
}
