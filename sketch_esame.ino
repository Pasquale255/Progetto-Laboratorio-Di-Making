#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// === Wi-Fi e Bot Telegram
const char* ssid = "S24 Ultra di Pasquale";
const char* password = "hellomoto";
#define BOT_TOKEN "7654343936:AAF2IS9_stSJCBiVO-NfJzvWHvv_Ud7eEcs"
#define CHAT_ID "364212740"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
unsigned long lastTelegramCheck = 0;

// === Display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === Pin
const int pinCaldo = 5;
const int pinFreddo = 18;
const int ldrPin = 32;

// === PWM e soglia
const int freq = 5000, resolution = 8, sogliaLuce = 2000;

// === Stati della luce
enum Fase { ATTESA, FASE_A, CALDO_100, CALDO_66, CALDO_33, SPENTO, MANUALE_ON };
Fase fase = ATTESA;

unsigned long inizioFase = 0;
bool manuale = false;
String tipoLuce = "fredda"; // "fredda", "calda", "misto"
int intensita = 255;        // 255 forte, 170 media, 85 bassa

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display non trovato");
    while (true);
  }

  display.setTextSize(1);
  display.setTextColor(WHITE);
  printDisplay("Avvio...");
  delay(1000);

  ledcAttach(pinCaldo, freq, resolution);
  ledcAttach(pinFreddo, freq, resolution);
  pinMode(ldrPin, INPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }

  client.setInsecure();
  bot.sendMessage(CHAT_ID, "ESP32 pronto in modalità automatica.", "");
}

void loop() {
  checkTelegram();

  if (manuale && fase == MANUALE_ON) {
    applicaLuceManuale();
    return;
  }

  int luce = analogRead(ldrPin);
  unsigned long tempo = millis();
  unsigned long t = tempo - inizioFase;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Luce: "); display.println(luce);

  if (luce >= sogliaLuce) {
    fase = ATTESA;
    spegni();
    printDisplay("Luce OK - Spento");
    return;
  }

  switch (fase) {
    case ATTESA:
      fase = FASE_A;
      inizioFase = tempo;
      ledcWrite(pinCaldo, 255);
      ledcWrite(pinFreddo, 0);
      break;

    case FASE_A:
      mostraFase("FREDDO 100%", t);
      if (t >= 10000) {
        fase = CALDO_100;
        inizioFase = tempo;
        ledcWrite(pinCaldo, 0);
        ledcWrite(pinFreddo, 255);
      }
      break;

    case CALDO_100:
      mostraFase("CALDO 100%", t);
      if (t >= 10000) {
        fase = CALDO_66;
        inizioFase = tempo;
        ledcWrite(pinFreddo, 170);
      }
      break;

    case CALDO_66:
      mostraFase("CALDO 66%", t);
      if (t >= 10000) {
        fase = CALDO_33;
        inizioFase = tempo;
        ledcWrite(pinFreddo, 85);
      }
      break;

    case CALDO_33:
      mostraFase("CALDO 33%", t);
      if (t >= 10000) {
        fase = SPENTO;
        inizioFase = tempo;
        ledcWrite(pinFreddo, 0);
      }
      break;

    case SPENTO:
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Fase automatica");
      display.println("terminata.");
      display.println();
      display.println("Usa Telegram per");
      display.println("controllo manuale.");
      display.display();
      break;

    case MANUALE_ON:
      break;
  }

  display.display();
  delay(100);
}

void checkTelegram() {
  if (millis() - lastTelegramCheck < 1000) return;

  int msgCount = bot.getUpdates(bot.last_message_received + 1);
  while (msgCount) {
    for (int i = 0; i < msgCount; i++) {
      String msg = bot.messages[i].text;
      String chat_id = bot.messages[i].chat_id;
      if (chat_id != CHAT_ID) continue;

      // Mostra comando ricevuto sul display
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Comando Telegram:");
      display.println(msg);
      display.display();

      if (msg == "/luce_on") {
        manuale = true;
        fase = MANUALE_ON;
        applicaLuceManuale();
        bot.sendMessage(chat_id, "Luce accesa.", "");
      } else if (msg == "/luce_off") {
        manuale = true;
        fase = SPENTO;
        spegni();
        printDisplay("Luce spenta (manuale)");
        bot.sendMessage(chat_id, "Luce spenta.", "");
      } else if (msg == "/fredda") {
        tipoLuce = "fredda";
        bot.sendMessage(chat_id, "Luce fredda selezionata.", "");
      } else if (msg == "/calda") {
        tipoLuce = "calda";
        bot.sendMessage(chat_id, "Luce calda selezionata.", "");
      } else if (msg == "/misto") {
        tipoLuce = "misto";
        bot.sendMessage(chat_id, "Luce mista (fredda + calda) selezionata.", "");
      } else if (msg == "/forte") {
        intensita = 255;
        bot.sendMessage(chat_id, "Intensità: FORTE", "");
      } else if (msg == "/media") {
        intensita = 170;
        bot.sendMessage(chat_id, " Intensità: MEDIA", "");
      } else if (msg == "/bassa") {
        intensita = 85;
        bot.sendMessage(chat_id, "Intensità: BASSA", "");
      }
    }
    msgCount = bot.getUpdates(bot.last_message_received + 1);
  }
  lastTelegramCheck = millis();
}

void applicaLuceManuale() {
  printDisplay("Manuale: " + tipoLuce + "\nIntensità: " + String(intensita));

  if (tipoLuce == "fredda") {
    ledcWrite(pinFreddo, 0);
    ledcWrite(pinCaldo, intensita);
  } else if (tipoLuce == "calda") {
    ledcWrite(pinCaldo, 0);
    ledcWrite(pinFreddo, intensita);
  } else if (tipoLuce == "misto") {
    ledcWrite(pinCaldo, intensita / 2);
    ledcWrite(pinFreddo, intensita / 2);
  }
}

void mostraFase(String faseDescrizione, unsigned long t) {
  display.setCursor(0, 20);
  if (fase == FASE_A) display.println("Fase A: FREDDO");
  else display.println("Fase B: CALDO");

  display.print(faseDescrizione);
  display.print(" - ");
  display.print("Cambio in ");
  display.print(10 - t / 1000);
  display.println(" s");
}

void printDisplay(String testo) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(testo);
  display.display();
}

void spegni() {
  ledcWrite(pinCaldo, 0);
  ledcWrite(pinFreddo, 0);
}
