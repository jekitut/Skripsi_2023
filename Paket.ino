#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <AsyncElegantOTA.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Servo.h>
#include <SoftwareSerial.h>


#define token_bot_telegram "6033298272:AAFO9VUlXMT69wy44PV71aWTzRfQM1kpexs"


#define ssid_wifi "ESP32"
#define password_wifi "201969040035"


#define pin_barcode_scanner_TXD 16
#define pin_barcode_scanner_RXD 17
#define pin_ultrasonik_trig 18
#define pin_ultrasonik_echo 19
#define pin_relay 22
#define pin_servo 23
#define web_api_key_firebase "AIzaSyCNQr1YSrvFSDOWK9EVMUV7Tz0FcrbrvcA"
#define url_database_firebase "https://paket-irul-default-rtdb.asia-southeast1.firebasedatabase.app/"


unsigned long milis_terakhir;
int buka_pagar, buka_kurung, tutup_kurung, buka_siku, tutup_siku, tutup_pagar, start, end, startt, endd, a, cm, jumlah_pesan, id_pesan;
String db_paket, db_paket_sampai, db_parsing, db_isi_pesan, db_id_token, resi, status, nama_akun_telegram, id_akun_telegram, isi_pesan, pesan, pesan_tombol;


AsyncWebServer web_server(80);
FirebaseAuth fbauth;
FirebaseConfig fbconfig;
FirebaseData fbdata;
WiFiClientSecure client_secure;
UniversalTelegramBot bot(token_bot_telegram, client_secure);
Servo servo;
EspSoftwareSerial::UART uart;


void setup() {
  setup_pin();
  setup_wifi_ota_telegram_firebase();
}


void loop() {
  AsyncElegantOTA.loop();
  cek_jumlah_pesan();
  cek_barcode_scanner();
}


void setup_pin() {
  uart.begin(9600, EspSoftwareSerial::SWSERIAL_8N1, pin_barcode_scanner_TXD, pin_barcode_scanner_RXD);
  servo.attach(pin_servo);
  pinMode(pin_ultrasonik_trig, OUTPUT);
  pinMode(pin_ultrasonik_echo, INPUT);
  digitalWrite(pin_ultrasonik_trig, LOW);
}


void setup_wifi_ota_telegram_firebase() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_wifi, password_wifi);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  
  web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Halo IRUL, ESP32 OTA (Over The Air) sudah siap digunakan.");
  });
  
  AsyncElegantOTA.begin(&web_server);
  web_server.begin();
  client_secure.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  fbconfig.api_key = web_api_key_firebase;
  fbconfig.database_url = url_database_firebase;

  while (!Firebase.ready()) {
    if (Firebase.signUp(&fbconfig, &fbauth, "", "")) {
      fbconfig.token_status_callback = tokenStatusCallback;
      Firebase.begin(&fbconfig, &fbauth);
      Firebase.reconnectWiFi(true);
      get_database();
      nol();
	  
      while (start != -1 && end != -1) {
        db_parsing = db_paket.substring(start, end + 1);
        buka_kurung = db_parsing.indexOf("(");
        tutup_kurung = db_parsing.indexOf(")");
        status = db_parsing.substring(buka_kurung, tutup_kurung + 1);

        if (resi.indexOf(status) == -1) {
          pesan = "Silahkan ketik /start untuk melihat menu.";
          bot.sendMessage(db_parsing.substring(buka_kurung + 1, tutup_kurung), pesan);
        }
        
        resi += status;
        start = db_paket.indexOf("|", end + 1);
        end = db_paket.indexOf("|", end + 2);
      }
    }
  }
}


void cek_jumlah_pesan() {
  if (millis() - milis_terakhir >= 100) {
    jumlah_pesan = bot.getUpdates(bot.last_message_received + 1);
    
    while (jumlah_pesan) {
      cek_isi_pesan();
      jumlah_pesan = bot.getUpdates(bot.last_message_received + 1);
    }
    
    milis_terakhir = millis();
  }
}


void cek_isi_pesan() {
  for (a=0; a<jumlah_pesan; a++) {
    nol();
    nama_akun_telegram = bot.messages[a].from_name;
    id_akun_telegram = String(bot.messages[a].chat_id);
    id_pesan = bot.messages[a].message_id;
    isi_pesan = bot.messages[a].text;
    isi_pesan.trim();
    isi_pesan.toUpperCase();
    db_id_token = "(" + id_akun_telegram + ")[" + String(token_bot_telegram) + "]";
    db_isi_pesan = "|" + isi_pesan + db_id_token + nama_akun_telegram + "|";

    if (isi_pesan == "/IP_OTA") {
      pesan = "Upload program ESP32 >>> OTA (Over The Air)";
      pesan_tombol = "[[{ \"text\":\"Update Kode Pemrograman\",\"url\":\"http://" + WiFi.localIP().toString() + "/update\" }]]";
      bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol);
    } else if (isi_pesan == "MENU" || isi_pesan == "/START") {
      menu();
    } else if (isi_pesan == "CEK") {
      cek();
    } else if (isi_pesan == "TAMBAH") {
      status = "TAMBAH";
      pesan = "Masukkan nomor resi yang ingin ditambah:";
      bot.sendMessage(id_akun_telegram, pesan);
    } else if (status == "TAMBAH") {
      tambah();
    } else if (isi_pesan == "HAPUS") {
      hapus();
    } else if (isi_pesan.indexOf("HAPUS_RESI") != -1) {
      hapus_resi();
    } else if (isi_pesan == "AMBIL") {
      ambil();
    } else if (isi_pesan.indexOf("AMBIL_PAKET") != -1) {
      ambil_paket();
    } else {
      elsee();
    }
  }
}


void get_database() {
  Firebase.RTDB.getString(&fbdata, "paket/data_satu");
  db_paket = fbdata.stringData();
  Firebase.RTDB.getString(&fbdata, "paket/data_dua");
  db_paket_sampai = fbdata.stringData();
}


void nol() {
  resi = "";
  buka_pagar = 0;
  buka_kurung = 0;
  tutup_kurung = 0;
  buka_siku = 0;
  tutup_siku = 0;
  tutup_pagar = 0;
  start = 0;
  end = 0;
  startt = 0;
  endd = 0;
  start = db_paket.indexOf("|");
  end = db_paket.indexOf("|", start + 1);
  startt = db_paket_sampai.indexOf("|");
  endd = db_paket_sampai.indexOf("|", startt + 1);
}


void menu() {
  pesan = "Menu yang tersedia:";
  pesan_tombol = "[[{ \"text\":\"Cek Nomor Resi\",\"callback_data\":\"CEK\" }],";
  pesan_tombol += "[{ \"text\":\"Tambah Nomor Resi\",\"callback_data\":\"TAMBAH\" }],";
  pesan_tombol += "[{ \"text\":\"Hapus Nomor Resi\",\"callback_data\":\"HAPUS\" }],";
  pesan_tombol += "[{ \"text\":\"Ambil Paket\",\"callback_data\":\"AMBIL\" }]]";

  if (isi_pesan == "MENU") {
    bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol, id_pesan);
  } else {
    bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol);
  }
}


void cek() {
  while (start != -1 && end != -1) {
    db_parsing = db_paket.substring(start, end + 1);
    buka_pagar = db_parsing.indexOf("|");
    buka_kurung = db_parsing.indexOf("(");

    if (db_parsing.indexOf(db_id_token) != -1) {
      resi += db_parsing.substring(buka_pagar + 1, buka_kurung) + "\n";
    }

    start = db_paket.indexOf("|", end + 1);
    end = db_paket.indexOf("|", end + 2);
  }

  pesan = "List nomor resi yang tersimpan:\n\n" + resi;
  pesan_tombol = "[[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"MENU\" }]]";
  bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol, id_pesan);
}


void tambah() {
  if (db_paket.indexOf("|" + isi_pesan + "(") != -1) {
    pesan = "\xE2\x9D\x8C " + isi_pesan + " sudah terdaftar\n\n";
    pesan += "Klik tombol di bawah ini untuk melihat list nomor resi.";
    pesan_tombol = "[[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"CEK\" }]]";
  } else {
    pesan = "\xE2\x9C\x85 " + isi_pesan;
    pesan_tombol = "[[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"MENU\" }]]";
    Firebase.RTDB.setString(&fbdata, "paket/data_satu", db_paket + db_isi_pesan);
    get_database();
  }
  
  bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol);
  status = "";
}


void hapus() {
  pesan = "Pilih nomor resi yang mau dihapus:";
  pesan_tombol = "[";

  while (start != -1 && end != -1) {
    db_parsing = db_paket.substring(start, end + 1);
    buka_pagar = db_parsing.indexOf("|");
    buka_kurung = db_parsing.indexOf("(");

    if (db_parsing.indexOf(db_id_token) != -1) {
      pesan_tombol += "[{ \"text\":\"" + db_parsing.substring(buka_pagar + 1, buka_kurung) + "\",\"callback_data\":\"HAPUS_RESI" + db_parsing.substring(buka_pagar, buka_kurung + 1) + "\" }],";
    }
    
    start = db_paket.indexOf("|", end + 1);
    end = db_paket.indexOf("|", end + 2);
  }

  pesan_tombol += "[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"MENU\" }]]";
  bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol, id_pesan);
}


void hapus_resi() {
  isi_pesan.replace("HAPUS_RESI", "");

  while (start != -1 && end != -1) {
    db_parsing = db_paket.substring(start, end + 1);

    if (db_parsing.indexOf(isi_pesan) != -1) {
      pesan = "\xE2\x9C\x85 " + isi_pesan.substring(1, isi_pesan.indexOf("("));
      pesan_tombol = "[[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"MENU\" }]]";
      bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol, id_pesan);
      db_paket.replace(db_parsing, "");
      db_paket_sampai.replace(db_parsing, "");
      
      if (db_paket.indexOf("|") == -1) {
        Firebase.RTDB.deleteNode(&fbdata, "paket/data_satu");
      } else {
        Firebase.RTDB.setString(&fbdata, "paket/data_satu", db_paket);
      }

      if (db_paket_sampai.indexOf("|") == -1) {
        Firebase.RTDB.deleteNode(&fbdata, "paket/data_dua");
      } else {
        Firebase.RTDB.setString(&fbdata, "paket/data_dua", db_paket_sampai);
      }

      get_database();
    }

    start = db_paket.indexOf("|", end + 1);
    end = db_paket.indexOf("|", end + 2);
  }
}


void ambil() {
  pesan = "Pilih nomor resi paket yang mau diambil:";
  pesan_tombol = "[";

  while (startt != -1 && endd != -1) {
    db_parsing = db_paket_sampai.substring(startt, endd + 1);
    buka_pagar = db_parsing.indexOf("|");
    buka_kurung = db_parsing.indexOf("(");

    if (db_parsing.indexOf(db_id_token) != -1) {
      pesan_tombol += "[{ \"text\":\"" + db_parsing.substring(buka_pagar + 1, buka_kurung) + "\",\"callback_data\":\"AMBIL_PAKET" + db_parsing.substring(buka_pagar + 1, buka_kurung) + "\" }],";
    }

    startt = db_paket_sampai.indexOf("|", endd + 1);
    endd = db_paket_sampai.indexOf("|", endd + 2);
  }
  
  pesan_tombol += "[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"MENU\" }]]";
  bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol, id_pesan);
}


void ambil_paket() {
  isi_pesan.replace("AMBIL_PAKET", "");
  pinMode(pin_relay, OUTPUT);
  delay(500);
  servo.write(66);

  for (a=1; a<=7; a++) {
    bot.sendChatAction(id_akun_telegram, "typing");
    pesan = "\xE2\x9C\x85 Paket dengan nomor resi: " + isi_pesan + "\n";
    digitalWrite(pin_ultrasonik_trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(pin_ultrasonik_trig, LOW);
    cm = (pulseIn(pin_ultrasonik_echo, HIGH) / 2) / 29.1;
      
    if (cm >= 15) {
      pesan += "\xE2\x9C\x85 Status: Paket sudah diambil";
      bot.sendChatAction(id_akun_telegram, "typing");
      bot.sendChatAction(id_akun_telegram, "typing");
      break;
    }

    pesan += "\xE2\x9D\x8C Status: Paket belum diambil";
  }

  servo.write(0);
  delay(500);
  pinMode(pin_relay, LOW);
  pesan_tombol = "[[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"MENU\" }]]";
  bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol, id_pesan);
}


void elsee() {
  pesan = nama_akun_telegram + ", pesan yang anda masukkan tidak tersedia.\n\n";
  pesan += "Klik tombol di bawah ini untuk kembali ke menu.";
  pesan_tombol = "[[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"MENU\" }]]";
  bot.sendMessageWithInlineKeyboard(id_akun_telegram, pesan, "", pesan_tombol);
}


void cek_barcode_scanner() {
  if (uart.available()) {
    nol();
    resi = uart.readString();
    resi.trim();
    resi.toUpperCase();

    while (start != -1 && end != -1) {
      db_parsing = db_paket.substring(start, end + 1);
      buka_pagar = db_parsing.indexOf("|");
      buka_kurung = db_parsing.indexOf("(");
      tutup_kurung = db_parsing.indexOf(")");
      tutup_siku = db_parsing.indexOf("]");
      tutup_pagar = db_parsing.indexOf("|", buka_pagar + 1);

      if (db_parsing.indexOf("|" + resi + "(") != -1 && db_parsing.indexOf("[" + String(token_bot_telegram) + "]") != -1) {
        pinMode(pin_relay, OUTPUT);
        delay(500);
        servo.write(66);

        for (a=1; a<=15; a++) {
          pesan = "\xE2\x9C\x85 Paket atas nama: " + db_parsing.substring(tutup_siku + 1, tutup_pagar) + "\n";
          pesan += "\xE2\x9C\x85 Nomor resi: " + db_parsing.substring(buka_pagar + 1, buka_kurung) + "\n";
          delay(1000);
          digitalWrite(pin_ultrasonik_trig, HIGH);
          delayMicroseconds(10);
          digitalWrite(pin_ultrasonik_trig, LOW);
          cm = (pulseIn(pin_ultrasonik_echo, HIGH) / 2) / 29.1;
          
          if (cm <= 15) {
            pesan += "\xE2\x9C\x85 Status: Paket sudah sampai";
            delay(5000);
            break;
          }
          
          pesan += "\xE2\x9D\x8C Status: Paket tidak terdeteksi";
        }

        servo.write(0);
        delay(500);
        pinMode(pin_relay, LOW);

        if (db_paket_sampai.indexOf(db_parsing) == -1) {
          Firebase.RTDB.setString(&fbdata, "paket/data_dua", db_paket_sampai + db_parsing);
          get_database();
        }

        pesan_tombol = "[[{ \"text\":\"Ambil Paket Sekarang\",\"callback_data\":\"AMBIL_PAKET" + db_parsing.substring(buka_pagar + 1, buka_kurung) + "\" }],";
        pesan_tombol += "[{ \"text\":\"\xE2\xAC\x85\",\"callback_data\":\"MENU\" }]]";
        bot.sendMessageWithInlineKeyboard(db_parsing.substring(buka_kurung + 1, tutup_kurung), pesan, "", pesan_tombol);
        break;
      }

      start = db_paket.indexOf("|", end + 1);
      end = db_paket.indexOf("|", end + 2);
    }
  }
}