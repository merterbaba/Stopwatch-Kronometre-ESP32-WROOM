#include <Wire.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== WiFi / NTP (isteğe bağlı) =====
// PC olmadan da doğru saat için 1 yap ve bilgileri doldur
#define USE_WIFI_NTP 0

#if USE_WIFI_NTP
#include <WiFi.h>
const char* WIFI_SSID = "WIFI_ADI";
const char* WIFI_PASS = "WIFI_SIFRE";
#endif

#define TZ_OFFSET_SEC 10800   // Türkiye UTC+3

#define SDA_PIN 26
#define SCL_PIN 27

#define START_BTN 12
#define RESET_BTN 14
#define MODE_BTN 25
#define BUZZER 13

Adafruit_SSD1306 display(128, 64, &Wire, -1);

bool running = false;

unsigned long startTime = 0;
unsigned long elapsedTime = 0;

bool lastStart = HIGH;
bool lastReset = HIGH;
bool lastMode = HIGH;

unsigned long lastStartChange = 0;
unsigned long lastResetChange = 0;
unsigned long lastModeChange = 0;
unsigned long lastSend = 0;

int theme = 1;
bool buzzerOn = true;

// ===== MODLAR =====
#define MODE_SW    0   // kronometre
#define MODE_TIMER 1   // zamanlayıcı
#define MODE_CLOCK 2   // saat

int mode = MODE_SW;

// ===== ZAMANLAYICI =====
bool finished = false;
unsigned long timerSetMs = 60000;

// Mod değişince diğer modun kaldığı yeri saklamak için
unsigned long savedSwElapsed = 0;
unsigned long savedTimerElapsed = 0;
bool savedTimerFinished = false;

// ===== SAAT =====
bool clockValid = false;
unsigned long clockBaseEpoch = 0;   // yerel saat (epoch saniye)
unsigned long clockBaseMillis = 0;
bool ntpSynced = false;

void setClock(unsigned long localEpoch) {
  clockBaseEpoch = localEpoch;
  clockBaseMillis = millis();
  clockValid = true;
}

// millis() taşmasına karşı her saatte bir tabanı kaydır
void updateClock() {
  if (!clockValid) return;
  while (millis() - clockBaseMillis >= 3600000UL) {
    clockBaseEpoch += 3600;
    clockBaseMillis += 3600000UL;
  }
}

unsigned long nowEpoch() {
  return clockBaseEpoch + (millis() - clockBaseMillis) / 1000;
}

unsigned long secondsOfDay() {
  return nowEpoch() % 86400UL;
}

#if USE_WIFI_NTP
void ntpTask() {
  static unsigned long last = 0;
  unsigned long interval = ntpSynced ? 600000UL : 3000UL;
  if (millis() - last < interval) return;
  last = millis();

  time_t t = time(nullptr);
  if (t > 1700000000) {   // saat internetten alınmış
    setClock((unsigned long)t + TZ_OFFSET_SEC);
    ntpSynced = true;
  }
}
#endif

void beep() {
  if (!buzzerOn) return;
  digitalWrite(BUZZER, HIGH);
  delay(30);
  digitalWrite(BUZZER, LOW);
}

void alarm() {
  if (!buzzerOn) return;
  for (int i = 0; i < 4; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
}

// v: 0-255. Kontrast + pre-charge ile geniş aralıkta parlaklık ayarı
void setBrightness(int v) {
  v = constrain(v, 0, 255);

  int contrast = (v * v) / 255;
  int phase2 = map(v, 0, 255, 1, 15);
  int precharge = (phase2 << 4) | 0x01;

  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
  display.ssd1306_command(SSD1306_SETPRECHARGE);
  display.ssd1306_command(precharge);
}

// Geçen süre (kronometre ve zamanlayıcıda ortak mantık)
unsigned long getElapsed() {
  return running ? millis() - startTime : elapsedTime;
}

// Ekranda gösterilen süre: kronometrede geçen, zamanlayıcıda kalan
unsigned long getTime() {
  unsigned long e = getElapsed();
  if (mode == MODE_SW) return e;
  if (mode == MODE_TIMER) return (e >= timerSetMs) ? 0 : timerSetMs - e;
  return 0;
}

// PC panele durum gönder: T:<ms>:<çalışıyor>:<mod>:<bitti>:<saat geçerli>
// Saat modunda ms = günün saniyesi * 1000
void sendStatus() {
  unsigned long v;
  if (mode == MODE_CLOCK) v = clockValid ? secondsOfDay() * 1000UL : 0;
  else v = getTime();

  Serial.print("T:");
  Serial.print(v);
  Serial.print(":");
  Serial.print(running ? 1 : 0);
  Serial.print(":");
  Serial.print(mode);
  Serial.print(":");
  Serial.print(finished ? 1 : 0);
  Serial.print(":");
  Serial.println(clockValid ? 1 : 0);
}

void startStopwatch() {
  if (mode == MODE_CLOCK) return;
  if (finished) {          // bittikten sonra başlat = baştan başla
    elapsedTime = 0;
    finished = false;
  }
  if (!running) {
    startTime = millis() - elapsedTime;
    running = true;
    beep();
    sendStatus();
  }
}

void stopStopwatch() {
  if (mode == MODE_CLOCK) return;
  if (running) {
    elapsedTime = millis() - startTime;
    running = false;
    beep();
    sendStatus();
  }
}

void resetStopwatch() {
  if (mode == MODE_CLOCK) return;
  running = false;
  elapsedTime = 0;
  finished = false;
  beep();
  sendStatus();
}

// Kronometre -> Zamanlayıcı -> Saat -> Kronometre
void toggleMode() {
  // Çalışıyorsa durdur, süreyi koru
  if (running) {
    elapsedTime = millis() - startTime;
    running = false;
  }

  // Şu anki modun durumunu sakla
  if (mode == MODE_TIMER) {
    savedTimerElapsed = elapsedTime;
    savedTimerFinished = finished;
  } else if (mode == MODE_SW) {
    savedSwElapsed = elapsedTime;
  }

  mode = (mode + 1) % 3;

  // Yeni modun kaldığı yeri yükle
  if (mode == MODE_TIMER) {
    elapsedTime = savedTimerElapsed;
    finished = savedTimerFinished;
  } else if (mode == MODE_SW) {
    elapsedTime = savedSwElapsed;
    finished = false;
  } else {
    elapsedTime = 0;
    finished = false;
  }

  beep();
  sendStatus();
}

void setTimerSeconds(long s) {
  s = constrain(s, 1, 5999);   // 00:01 - 99:59
  timerSetMs = (unsigned long)s * 1000UL;

  if (mode == MODE_TIMER) {
    // zamanlayıcıdayken yeni süre sıfırdan başlar
    running = false;
    elapsedTime = 0;
    finished = false;
  } else {
    // başka moddayken zamanlayıcının saklı durumu sıfırlanır
    savedTimerElapsed = 0;
    savedTimerFinished = false;
  }
  sendStatus();
}

// Yazıyı yatayda ortalayarak yazar
void printCentered(String s, int size, int y) {
  int w = s.length() * 6 * size;
  int x = (128 - w) / 2;
  if (x < 0) x = 0;
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(s);
}

String statusText() {
  if (finished) return "BITTI!";
  return running ? "CALISIYOR" : "DURDU";
}

void drawClock() {

  display.clearDisplay();
  display.setTextColor(WHITE);

  if (!clockValid) {
    printCentered("SAAT", 1, 0);
    printCentered("--:--:--", 2, 24);
    printCentered("PANELDEN BAGLAN", 1, 52);
    display.display();
    return;
  }

  time_t t = (time_t)nowEpoch();
  struct tm tmv;
  gmtime_r(&t, &tmv);

  char hm[8], ss[4], hms[12], date[16];
  snprintf(hm, sizeof(hm), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
  snprintf(ss, sizeof(ss), "%02d", tmv.tm_sec);
  snprintf(hms, sizeof(hms), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
  snprintf(date, sizeof(date), "%02d.%02d.%04d", tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);

  if (theme == 1) {
    // Tema 1: Klasik
    printCentered("SAAT", 1, 0);
    printCentered(String(hms), 2, 24);
    printCentered(String(date), 1, 50);
  }
  else if (theme == 2) {
    // Tema 2: Büyük saat + saniye çubuğu
    display.setTextSize(3);
    display.setCursor(0, 8);
    display.print(hm);

    display.setTextSize(2);
    display.setCursor(96, 16);
    display.print(ss);

    int dolu = tmv.tm_sec * 124 / 59;
    display.drawRect(0, 42, 128, 10, WHITE);
    display.fillRect(2, 44, dolu, 6, WHITE);

    printCentered(String(date), 1, 56);
  }
  else {
    // Tema 3: Çerçeveli
    display.drawRect(0, 0, 128, 64, WHITE);
    printCentered("SAAT", 1, 5);
    printCentered(String(hms), 2, 22);
    printCentered(String(date), 1, 50);
  }

  display.display();
}

void drawOLED(unsigned long currentTime) {

  if (mode == MODE_CLOCK) {
    drawClock();
    return;
  }

  int dakika = currentTime / 60000;
  int saniye = (currentTime / 1000) % 60;
  int salise = (currentTime % 1000) / 10;

  char timeStr[12];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", dakika, saniye, salise);

  String title = (mode == MODE_TIMER) ? "ZAMANLAYICI" : "KRONOMETRE";

  display.clearDisplay();
  display.setTextColor(WHITE);

  if (theme == 1) {
    // Tema 1: Klasik
    printCentered(title, 1, 0);
    printCentered(String(timeStr), 2, 24);
    if (finished) printCentered("BITTI!", 1, 50);
  }
  else if (theme == 2) {
    // Tema 2: Büyük dijital + ilerleme çubuğu
    char ms[6];
    snprintf(ms, sizeof(ms), "%02d:%02d", dakika, saniye);

    display.setTextSize(3);
    display.setCursor(0, 8);
    display.print(ms);

    char cs[4];
    snprintf(cs, sizeof(cs), "%02d", salise);
    display.setTextSize(2);
    display.setCursor(96, 16);
    display.print(cs);

    int dolu;
    if (mode == MODE_TIMER) {
      // Kalan süreyi gösteren çubuk (azalır)
      dolu = (unsigned long)currentTime * 124 / timerSetMs;
    } else {
      // Dakikayı dolduran çubuk
      dolu = (currentTime % 60000) * 124 / 60000;
    }
    display.drawRect(0, 42, 128, 10, WHITE);
    display.fillRect(2, 44, dolu, 6, WHITE);

    printCentered(statusText(), 1, 56);
  }
  else {
    // Tema 3: Çerçeveli + durum yazısı
    display.drawRect(0, 0, 128, 64, WHITE);
    printCentered(title, 1, 5);
    printCentered(String(timeStr), 2, 22);
    printCentered(statusText(), 1, 50);
  }

  display.display();
}

void handleCommand(String cmd) {

  cmd.trim();

  if (cmd == "START") {
    startStopwatch();
  }
  else if (cmd == "STOP") {
    stopStopwatch();
  }
  else if (cmd == "RESET") {
    resetStopwatch();
  }
  else if (cmd == "TOGGLE") {
    if (running) stopStopwatch();
    else startStopwatch();
  }
  else if (cmd == "STATUS") {
    sendStatus();
  }
  else if (cmd == "MODE") {
    toggleMode();
  }
  else if (cmd.startsWith("SETTIME:")) {
    setTimerSeconds(cmd.substring(8).toInt());
  }
  else if (cmd.startsWith("TIME:")) {
    long t = cmd.substring(5).toInt();
    if (t > 1700000000L) setClock((unsigned long)t);   // PC'den yerel saat
  }
  else if (cmd == "THEME1") {
    theme = 1;
  }
  else if (cmd == "THEME2") {
    theme = 2;
  }
  else if (cmd == "THEME3") {
    theme = 3;
  }
  else if (cmd == "BUZZER1") {
    buzzerOn = true;
  }
  else if (cmd == "BUZZER0") {
    buzzerOn = false;
  }
  else if (cmd.startsWith("BRIGHT:")) {
    setBrightness(cmd.substring(7).toInt());
  }
}

void setup() {

  Serial.begin(115200);
  Serial.setTimeout(20);

  pinMode(START_BTN, INPUT_PULLUP);
  pinMode(RESET_BTN, INPUT_PULLUP);
  pinMode(MODE_BTN, INPUT_PULLUP);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.display();

#if USE_WIFI_NTP
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
#endif
}

void loop() {

  // ===== PC PANEL KOMUTLARI =====

  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
  }

  // ===== SAAT =====

  updateClock();
#if USE_WIFI_NTP
  ntpTask();
#endif

  // ===== BUTONLAR =====

  bool startBtn = digitalRead(START_BTN);
  bool resetBtn = digitalRead(RESET_BTN);
  bool modeBtn = digitalRead(MODE_BTN);

  if (lastStart == HIGH && startBtn == LOW && millis() - lastStartChange > 40) {
    lastStartChange = millis();

    if (running) stopStopwatch();
    else startStopwatch();
  }

  if (lastReset == HIGH && resetBtn == LOW && millis() - lastResetChange > 40) {
    lastResetChange = millis();

    resetStopwatch();
  }

  if (lastMode == HIGH && modeBtn == LOW && millis() - lastModeChange > 40) {
    lastModeChange = millis();

    toggleMode();
  }

  lastStart = startBtn;
  lastReset = resetBtn;
  lastMode = modeBtn;

  // ===== ZAMANLAYICI BİTTİ Mİ? =====

  if (mode == MODE_TIMER && running && getElapsed() >= timerSetMs) {
    running = false;
    elapsedTime = timerSetMs;
    finished = true;
    drawOLED(0);        // ekranda 00:00:00 / BITTI! görünsün
    sendStatus();
    alarm();
  }

  // ===== PC'YE DURUM GÖNDER =====

  if (millis() - lastSend >= 100) {
    lastSend = millis();
    sendStatus();
  }

  // ===== OLED =====

  drawOLED(getTime());

  delay(10);
}
 
