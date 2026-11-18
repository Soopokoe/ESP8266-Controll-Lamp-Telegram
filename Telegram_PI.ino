#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define WIFI_SSID     "Junan"
#define WIFI_PASSWORD "123456789"
#define BOT_TOKEN     "--"
#define CHAT_ID       "--" 

// Pin lampu
#define LAMPU1_PIN 5  // D1
#define LAMPU2_PIN 4  // D2

// Pin tombol fisik
#define BUTTON1_PIN 12 // D6 
#define BUTTON2_PIN 14 // D5 

WiFiClientSecure client;x
UniversalTelegramBot bot(BOT_TOKEN, client);

bool hasSentWelcome = false;
bool statusLampu1 = false;
bool statusLampu2 = false;

struct Schedule {
  uint8_t lampu;
  bool turnOn;
  unsigned long executeTime;
};
std::vector<Schedule> schedules;

unsigned long bot_lasttime = 0;

unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
bool lastButtonState1 = HIGH;
bool lastButtonState2 = HIGH;
const unsigned long debounceDelay = 50; 

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  client.setInsecure();

  pinMode(LAMPU1_PIN, OUTPUT);
  pinMode(LAMPU2_PIN, OUTPUT);
  digitalWrite(LAMPU1_PIN, HIGH);
  digitalWrite(LAMPU2_PIN, HIGH);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
}

void loop() {
  checkSchedules();
  checkButtons();

  if (millis() - bot_lasttime > 3000) {
    bot_lasttime = millis();
    int newMsg = bot.getUpdates(bot.last_message_received + 1);
    while (newMsg) {
      handleNewMessages(newMsg);
      newMsg = bot.getUpdates(bot.last_message_received + 1);
    }
  }
}

void checkButtons() {
  bool reading1 = digitalRead(BUTTON1_PIN);
  if (reading1 != lastButtonState1) {
    lastDebounceTime1 = millis();
  }
  if ((millis() - lastDebounceTime1) > debounceDelay) {
    if (reading1 == LOW) {  
      statusLampu1 = !statusLampu1;
      digitalWrite(LAMPU1_PIN, statusLampu1 ? LOW : HIGH);
      bot.sendMessage(CHAT_ID, "Lampu1 " + String(statusLampu1 ? "MENYALA (tombol)" : "MATI (tombol)"));
    }
  }
  lastButtonState1 = reading1;

  bool reading2 = digitalRead(BUTTON2_PIN);
  if (reading2 != lastButtonState2) {
    lastDebounceTime2 = millis();
  }
  if ((millis() - lastDebounceTime2) > debounceDelay) {
    if (reading2 == LOW) {  
      statusLampu2 = !statusLampu2;
      digitalWrite(LAMPU2_PIN, statusLampu2 ? LOW : HIGH);
      bot.sendMessage(CHAT_ID, "Lampu2 " + String(statusLampu2 ? "MENYALA (tombol)" : "MATI (tombol)"));
    }
  }
  lastButtonState2 = reading2;
}

void checkSchedules() {
  unsigned long now = millis();
  for (int i = (int)schedules.size() - 1; i >= 0; i--) {
    if (now >= schedules[i].executeTime) {
      uint8_t pin = (schedules[i].lampu == 1 ? LAMPU1_PIN : LAMPU2_PIN);
      digitalWrite(pin, schedules[i].turnOn ? LOW : HIGH);
      if (schedules[i].lampu == 1) statusLampu1 = schedules[i].turnOn;
      else statusLampu2 = schedules[i].turnOn;
      String msg = "Jadwal dieksekusi: Lampu" + String(schedules[i].lampu) + (schedules[i].turnOn ? " MENYALA" : " MATI");
      bot.sendMessage(CHAT_ID, msg);
      schedules.erase(schedules.begin() + i);
    }
  }
}

void handleNewMessages(int numNew) {
  for (int i = 0; i < numNew; i++) {
    String chat_id   = bot.messages[i].chat_id;
    String text      = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    if (!hasSentWelcome) {
      bot.sendMessage(chat_id, "Hi, " + from_name + "! ESP8266 terhubung.", "");
      hasSentWelcome = true;
    }

    if (text == "/start" || text == "/help") {
      String msg  = "Hi, " + from_name + "! Ini adalah sistem Pengontrol lampu.\n";
      msg += "Perintah:\n";
      msg += "/lampu1_on, /lampu1_off, /lampu2_on, /lampu2_off\n";
      msg += "/schedule lampu1 on 5\n";
      msg += "/schedule lampu2 off 15\n";
      msg += "/schedules → lihat jadwal\n";
      msg += "/status → lihat status lampu\n";
      msg += "/clear → hapus semua jadwal\n";
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text.startsWith("/schedule ")) {
      String cmd = text.substring(10);
      cmd.trim();
      int sp1 = cmd.indexOf(' ');
      int sp2 = cmd.indexOf(' ', sp1 + 1);
      if (sp1 < 0 || sp2 < 0) {
        bot.sendMessage(chat_id, "Format: /schedule lampu1 on <menit>", "");
        continue;
      }
      String lampuStr = cmd.substring(0, sp1);
      String modeStr  = cmd.substring(sp1 + 1, sp2);
      String menitStr = cmd.substring(sp2 + 1);
      int menit = menitStr.toInt();
      uint8_t lampu = (lampuStr == "lampu1") ? 1 : (lampuStr == "lampu2") ? 2 : 0;
      bool turnOn = (modeStr == "on");
      if (lampu == 0 || (modeStr != "on" && modeStr != "off") || menit <= 0) {
        bot.sendMessage(chat_id, "Format salah. Gunakan: /schedule lampu1 on <menit>", "");
        continue;
      }
      Schedule s = { lampu, turnOn, millis() + (unsigned long)menit * 60000UL };
      schedules.push_back(s);
      bot.sendMessage(chat_id, "Jadwal dibuat: lampu" + String(lampu) + (turnOn ? " ON " : " OFF ") + "dalam " + String(menit) + " menit", "");
    }
    else if (text == "/schedules") {
      if (schedules.empty()) {
        bot.sendMessage(chat_id, "Tidak ada jadwal.", "");
      } else {
        String msg = "Jadwal aktif:\n";
        for (size_t j = 0; j < schedules.size(); j++) {
          unsigned long rem = (schedules[j].executeTime - millis()) / 1000;
          int m = rem / 60, s = rem % 60;
          msg += String(j + 1) + ". Lampu" + String(schedules[j].lampu) + (schedules[j].turnOn ? " ON " : " OFF ") + "dalam " + String(m) + "m" + String(s) + "s\n";
        }
        bot.sendMessage(chat_id, msg, "");
      }
    }
    else if (text == "/clear") {
      schedules.clear();
      bot.sendMessage(chat_id, "Semua jadwal dihapus.", "");
    }
    else if (text == "/status") {
      String msg = "Status lampu:\n";
      msg += "Lampu1: " + String(statusLampu1 ? "MENYALA" : "MATI") + "\n";
      msg += "Lampu2: " + String(statusLampu2 ? "MENYALA" : "MATI");
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text == "/lampu1_on")  manualControl(1, true,  chat_id);
    else if (text == "/lampu1_off") manualControl(1, false, chat_id);
    else if (text == "/lampu2_on")  manualControl(2, true,  chat_id);
    else if (text == "/lampu2_off") manualControl(2, false, chat_id);
    else {
      bot.sendMessage(chat_id, "Perintah tidak dikenal. Gunakan /help", "");
    }
  }
}

void manualControl(uint8_t lampu, bool on, String chat_id) {
  uint8_t pin = (lampu == 1 ? LAMPU1_PIN : LAMPU2_PIN);
  bool isOn = (digitalRead(pin) == LOW);
  if (isOn == on) {
    bot.sendMessage(chat_id, "Lampu" + String(lampu) + (on ? " sudah MENYALA." : " sudah MATI."), "");
  } else {
    digitalWrite(pin, on ? LOW : HIGH);
    if (lampu == 1) statusLampu1 = on; else statusLampu2 = on;
    bot.sendMessage(chat_id, "Lampu" + String(lampu) + (on ? " MENYALA." : " MATI."), "");
  }
}
