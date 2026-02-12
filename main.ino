#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include <WiFiUdp.h>
#include <NTPClient.h>

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ----------------------------
// WIFI & TELEGRAM
// ----------------------------
const char* ssid = "WIFI_KAMU";
const char* password = "PASSWORD";

String BOTtoken = "TOKEN_BOT_KAMU";
String CHAT_ID = "ID_TELEGRAM_KAMU";

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

// ----------------------------
// PIN SETUP
// ----------------------------
#define PIR_PIN 13
#define FLASH_PIN 4
#define BUZZER_PIN 14
#define MANUAL_BUTTON 15

// ----------------------------
// NTP TIME
// ----------------------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // UTC+7 Indonesia

// ----------------------------
// JADWAL BUKA / SAHUR MUKOMUKO 2026
// Format: jam, menit
// ----------------------------

// Contoh jadwal (kamu bisa ubah kapan aja)
int imsak_jam = 04, imsak_menit = 50;
int subuh_jam = 05, subuh_menit = 00;
int maghrib_jam = 18, maghrib_menit = 15;  // Buka puasa
int isya_jam = 19, isya_menit = 25;
int dzuhur_jam = 12, dzuhur_menit = 20;
int ashar_jam = 15, ashar_menit = 35;

// ----------------------------
// CAMERA CONFIG
// ----------------------------
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// -------------------------------------
// FUNGSI BUZZER BIKIN LAGU 30 DETIK
// -------------------------------------
void playTone(int freq, int dur){
  tone(BUZZER_PIN, freq, dur);
  delay(dur);
}

void playSong30s(){
  unsigned long start = millis();
  int melody[] = {523,587,659,698,784,880,988,1046};
  int len = 8;

  while (millis() - start < 30000) {
    for(int i=0;i<len;i++){
      playTone(melody[i],120);
    }
  }
  noTone(BUZZER_PIN);
}

// ----------------------------
// KIRIM FOTO KE TELEGRAM
// ----------------------------
void sendPhoto(){
  camera_fb_t * fb = NULL;
  digitalWrite(FLASH_PIN, HIGH);
  delay(200);

  fb = esp_camera_fb_get();
  digitalWrite(FLASH_PIN, LOW);

  if(!fb){
    bot.sendMessage(CHAT_ID, "Gagal ambil foto");
    return;
  }

  bot.sendPhoto(CHAT_ID, "image.jpg", fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// ----------------------------
// CEK WAKTU ADZAN / IMSAK
// ----------------------------
void checkSchedule(){
  int jam = timeClient.getHours();
  int menit = timeClient.getMinutes();

  // Imsak
  if(jam == imsak_jam && menit == imsak_menit){
    bot.sendMessage(CHAT_ID, "⏰ *Imsak!*");
    playSong30s();
  }

  // Subuh
  if(jam == subuh_jam && menit == subuh_menit){
    bot.sendMessage(CHAT_ID, "🕌 Adzan Subuh");
    playSong30s();
  }

  // Dzuhur
  if(jam == dzuhur_jam && menit == dzuhur_menit){
    bot.sendMessage(CHAT_ID, "🕌 Adzan Dzuhur");
    playSong30s();
  }

  // Ashar
  if(jam == ashar_jam && menit == ashar_menit){
    bot.sendMessage(CHAT_ID, "🕌 Adzan Ashar");
    playSong30s();
  }

  // Maghrib (BUKA PUASA)
  if(jam == maghrib_jam && menit == maghrib_menit){
    bot.sendMessage(CHAT_ID, "🌙 *Waktunya buka puasa!*");
    playSong30s();
  }

  // Isya
  if(jam == isya_jam && menit == isya_menit){
    bot.sendMessage(CHAT_ID, "🕌 Adzan Isya");
    playSong30s();
  }
}

// ----------------------------
// SETUP
// ----------------------------
void setup(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  pinMode(PIR_PIN, INPUT);
  pinMode(FLASH_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MANUAL_BUTTON, INPUT_PULLUP);

  Serial.begin(115200);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_camera_init(&config);

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
  }

  clientTCP.setInsecure();
  timeClient.begin();
}

// ----------------------------
// LOOP
// ----------------------------
unsigned long lastCheck = 0;

void loop(){
  timeClient.update();

  // Cek jadwal adzan & imsak tiap 10 detik
  if(millis() - lastCheck > 10000){
    checkSchedule();
    lastCheck = millis();
  }

  // Foto manual via tombol
  if(digitalRead(MANUAL_BUTTON) == LOW){
    sendPhoto();
    delay(500);
  }

  // Foto otomatis via PIR
  if(digitalRead(PIR_PIN) == HIGH){
    bot.sendMessage(CHAT_ID, "Gerakan terdeteksi!");
    sendPhoto();
    delay(5000);
  }

  delay(50);
}
