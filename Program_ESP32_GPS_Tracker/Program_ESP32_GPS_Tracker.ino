//--------------------------------------------------------------------------------------------------------
// Library File
#include "TinyGPS++.h"
#include "SoftwareSerial.h"
#include "ArduinoJson.h"
#include "UrlEncode.h"

//--------------------------------------------------------------------------
// Difinisikan PIN
#define LED_SIM 25
#define LED_GPS 26
#define SIM_RX 21
#define SIM_TX 22
#define RLY_POWER 18
#define RLY_START 19
#define PC Serial
#define GPS Serial2

//--------------------------------------------------------------------------
// Difinisikan Nilai
#define HTTP_FAILED 1       // Gagal konek 3 kali check signal
#define SIM_DEBUG true      // Serial ke PC SIM command
#define GPS_DEBUG false     // Serial ke PC update GPS
#define DATA_DEBUG false    // Serial ke PC Semua Data Yang Perlu
#define GOOD_SIGNAL 15      // lebih dari 15 = Signal Bagus
#define LOOPING_INTERVAL 2  // Kirim 2 menit sekali
#define ALERT_INTERVAL 1    // Kirim 1 menit sekali
//--------------------------------------------------------------------------
// Difinisikan Logika
#define ON 1
#define OFF 0
#define LN String(char(10))
//--------------------------------------------------------------------------
// Telegram Mode
#define READ_MESSAGE 0
#define SEND_MESSAGE 1
#define SEND_LOCATION 2

// Mode Alat
#define MODE_AWAL 0
#define MODE_NORMAL 1
#define MODE_JAGA 2

// Step Start Program
#define STEP_AWAL 1
#define STEP_GPS 2
#define STEP_RADIUS 3
#define STEP_MODE 4

//--------------------------------------------------------------------------
const String host_url = "http://geotracking.online/telegram.php";         // Host
const String bot_token = "6570175654:AAGciJTy_x41tPnaNJaZu6xdL-1pEcxRaO0";  // Bot Key
const String chat_id = "5260723107";                                        // Chat ID
int maks_jarak = 20;                                                        // jarak maksimal alarm
//--------------------------------------------------------------------------
int thn, bln, tgl, jam, mnt, dtk, interval_loop;
int step_cmd, telegram_mode, start_step, start_mode, message_looping;
int interval_sim, sim_signal, http_failed;
long jarak_awal, jarak_realtime, radius, offset;
double gps_latitude, gps_longitude, user_latitude, user_longitude;
bool start, response_cmd, good_signal, blink_sim, blink_gps, alert;
unsigned long response_timeout, interval_start;
String map_url, payload, response, json, message, send_message, setmsg;

//--------------------------------------------------------------------------
TinyGPSPlus gps;
StaticJsonDocument<500> doc;
SoftwareSerial SIM(SIM_RX, SIM_TX);
//--------------------------------------------------------------------------------------------------------
void setup() {
  delay(1000);
  // Serial Kom
  PC.begin(9600);
  SIM.begin(9600);
  GPS.begin(9600, SERIAL_8N1, 16, 17);

  // Pin Mode
  pinMode(LED_SIM, OUTPUT);
  pinMode(LED_GPS, OUTPUT);
  pinMode(RLY_POWER, OUTPUT);
  pinMode(RLY_START, OUTPUT);

  // Set Pin
  digitalWrite(LED_GPS, ON);
  digitalWrite(RLY_POWER, ON);

  // nilai variable awal
  startVariable();

  // Restart
  PC.println("System Restart");
  setmsg = "*System Restart*" + LN;
  setmsg += "- Untuk Start System, perintah /mulai" + LN;
  setmsg += "- Untuk Stop System, perintah /berhenti" + LN;
  sendMessage(setmsg);
}
//--------------------------------------------------------------------------------------------------------
void loop() {
  //--------------------------------------------------------------------------
  // Read Inputan
  // readCommandSIM(); // Coba AT Command Serial
  // readRawGPS(); // Baca Raw GPS
  SerialFakeGPS();
  getUpdateGPS();  // Baca GPS Long Lat
  //--------------------------------------------------------------------------
  // Jika Signal buruk
  if (!good_signal) {
    readSignal();
  }
  // Jika Signal Bagus
  else {
    // Baca SIM kirim telegram atau baca pesan
    if (telegram_mode == SEND_MESSAGE) sendTelegram("?token=" + bot_token + "&chat_id=" + chat_id + "&text=" + urlEncode(send_message));                                                                                            // Kirim Pesan Telegram
    else if (telegram_mode == SEND_LOCATION) sendTelegram("?token=" + bot_token + "&chat_id=" + chat_id + "&latitude=" + String(gps_latitude, 6) + "&longitude=" + String(gps_longitude, 6) + "&text=" + urlEncode(send_message));  // Kirim Pesan Telegram
    else readTelegram();                                                                                                                                                                                                            // Baca Pesan Telegram secara realtime
  }
  //--------------------------------------------------------------------------
  // Baca Pesan Masuk
  if (message != "") {
    // Jika pesan Mulai dan Step ke 1
    if (message == "/mulai" && start_step == 1) {
      start = true;
      start_step = STEP_GPS;
      gps_latitude = 0.0;
      gps_longitude = 0.0;

      PC.println("Mulai / Start System");
      PC.println();

      setmsg = "*Mulai / Start System*" + LN;
      setmsg += "- Kirim lokasi GPS Terkini anda untuk memperbarui Lokasi User" + LN + LN;
      setmsg += "Untuk menghentikan program gunakan perintah /berhenti " + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendMessage(setmsg);
    }
    // Jika pesan Berhenti
    else if (message == "/berhenti" && start_step > STEP_AWAL) {
      startVariable();

      PC.println("Berhenti / Stop System");
      PC.println();

      setmsg = "*Berhenti / Stop System*" + LN;
      setmsg += "- Untuk Start System kembali, gunakan perintah /mulai" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendMessage(setmsg);
    }
    // Jika pesan Radius
    else if (message != "" && start_step == STEP_RADIUS) {
      int read_radius = parsingRadius(message);
      PC.println(F("Parse Radius GPS"));
      PC.print(F("Radius Read: "));
      PC.print(read_radius);
      PC.println(F(" meter"));
      PC.println();
      if (read_radius > jarak_awal) {
        start_step = STEP_MODE;
        PC.println("Set Radius GPS Berhasil");
        PC.println();
        radius = read_radius;
        interval_start = millis();

        setmsg = "*Berhasil Set Radius GPS*" + LN;
        setmsg += "- Radius: " + String(radius) + " meter" + LN;
        setmsg += "Pilih Mode : " + LN;
        setmsg += "- /modenormal " + LN;
        setmsg += "- /modejaga " + LN + LN;
        setmsg += "Stop Program /berhenti" + LN;
        setmsg += "_Datetime: " + strDateTime() + "_";
      } else {
        PC.println("Set Radius GPS Gagal");
        PC.println();
        setmsg = "*Gagal Set Radius GPS*" + LN;
        setmsg += "- Jarak Radius Harus lebih dari >" + String(jarak_awal) + " meter" + LN;
        setmsg += "- Command Set Radius: radius=100 (meter)" + LN + LN;
        setmsg += "Stop Program /berhenti" + LN;
        setmsg += "_Datetime: " + strDateTime() + "_";
      }
      sendMessage(setmsg);
    }
    // Jika pesan Mode Normal
    else if (message == "/modenormal" && start_step == STEP_MODE) {
      PC.println("Set Mode Normal");
      PC.println();

      start_mode = MODE_NORMAL;
      message_looping = true;
      alert = false;
      interval_start = millis();

      setmsg = "*Berhasil Set Mode Normal*" + LN;
      setmsg += "- /updatelokasisaatini" + LN;
      setmsg += "- /pantau" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";

      sendMessage(setmsg);
    }
    // Jika pesan Mode Normal
    else if (message == "/modejaga" && start_step == STEP_MODE) {
      PC.println("Set Mode JAGA");
      PC.println();

      start_mode = MODE_JAGA;
      message_looping = true;
      alert = false;
      interval_start = millis();

      setmsg = "*Berhasil Set Mode Jaga*" + LN;
      setmsg += "- /updatelokasimotor" + LN;
      setmsg += "- /lacak" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendMessage(setmsg);
    }

    // Jika pesan Mode Normal && Step : Mode && pesan /updatelokasisaatini
    // Kirim Lokasi Motor dan matikan mode Looping
    else if (message == "/updatelokasisaatini" && start_mode == MODE_NORMAL && start_step == STEP_MODE) {
      PC.println("Update Lokasi Saat Ini");
      PC.println("Kirim Lokasi dan Matikan pesan Looping");
      PC.println();

      jarak_realtime = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);
      message_looping = false;
      interval_start = millis();

      setmsg = "*Update Lokasi Saat Ini*" + LN;
      setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
      setmsg += "- Looping Dimatikan" + LN;
      setmsg += "- /updatelokasisaatini" + LN;
      setmsg += "- /pantau" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendLocation(setmsg);
    }
    // Jika pesan Mode Normal && Step : Mode && pesan /pantau
    // Kirim Lokasi Motor dan matikan mode Looping
    else if (message == "/pantau" && start_mode == MODE_NORMAL && start_step == STEP_MODE) {
      PC.println("Update Lokasi Saat Ini");
      PC.println("Kirim Lokasi dan Aktifkan pesan Looping");
      PC.println();

      jarak_realtime = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);
      message_looping = true;
      interval_start = millis();

      setmsg = "*Update Lokasi Saat Ini*" + LN;
      setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
      setmsg += "- Looping Diaktifkan" + LN;
      setmsg += "- /updatelokasisaatini" + LN;
      setmsg += "- /pantau" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendLocation(setmsg);
    }

    // Jika pesan Mode Jaga && Step : Mode && pesan /updatelokasimotor
    // Kirim Lokasi Motor dan matikan mode Looping
    else if (message == "/updatelokasimotor" && start_mode == MODE_JAGA && start_step == STEP_MODE) {
      PC.println("Update Lokasi Saat Ini");
      PC.println("Kirim Lokasi dan Matikan pesan Looping");
      PC.println();

      message_looping = false;
      interval_start = millis();

      setmsg = "*Lacak Lokasi Saat Ini*" + LN;
      setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
      setmsg += "- Looping Dimatikan" + LN;
      setmsg += "- /updatelokasimotor" + LN;
      setmsg += "- /lacak" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendLocation(setmsg);
    }
    // Jika pesan Mode Normal && Step : Mode && pesan /pantau
    // Kirim Lokasi Motor dan matikan mode Looping
    else if (message == "/lacak" && start_mode == MODE_JAGA && start_step == STEP_MODE) {
      PC.println("Update Lokasi Saat Ini");
      PC.println("Kirim Lokasi dan Aktifkan pesan Looping");
      PC.println();

      message_looping = true;
      interval_start = millis();

      setmsg = "*Lacak Lokasi Saat Ini*" + LN;
      setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
      setmsg += "- Looping Diaktifkan" + LN;
      setmsg += "- /updatelokasimotor" + LN;
      setmsg += "- /lacak" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendLocation(setmsg);
    }

    message = "";
  }
  //--------------------------------------------------------------------------
  // Jika dalam kondisi Start
  if (start) {
    if (start_mode != MODE_AWAL) {
      // Cek Update Jarak Realtime dan Radius untuk mode jaga
      if (start_mode == MODE_JAGA && jarak_realtime >= radius) {
        alert = true;
      }
      // Jika Alert Aktif
      if (alert) {
        digitalWrite(RLY_POWER, OFF);
        message_looping = true;
        interval_loop = ALERT_INTERVAL;
      } else {
        interval_loop = LOOPING_INTERVAL;
      }

      // Interval Looping Kiri Pesan
      if (millis() - interval_start > 60000 * interval_loop && message_looping) {
        if (start_mode == MODE_NORMAL) {
          Serial.println(F("MODE: NORMAL"));
          Serial.println();
          setmsg = "*Update Lokasi Saat Ini*" + LN;
          setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
          setmsg += "- /updatelokasisaatini" + LN;
          setmsg += "- /pantau" + LN + LN;
          setmsg += "Stop Program /berhenti" + LN;
          setmsg += "_Datetime: " + strDateTime() + "_";
          sendLocation(setmsg);

        } else if (start_mode == MODE_JAGA) {

          Serial.println(F("MODE: JAGA"));
          Serial.println();
          if (alert) {
            setmsg = "*Warning! Motor melebihi Radius*" + LN;
            setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
            setmsg += "- Motor dimatikan" + LN + LN;
            setmsg += "Stop Program /berhenti" + LN;
            setmsg += "_Datetime: " + strDateTime() + "_";
          } else {
            setmsg = "*Lacak Lokasi Saat Ini*" + LN;
            setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
            setmsg += "- /updatelokasimotor" + LN;
            setmsg += "- /lacak" + LN + LN;
            setmsg += "Stop Program /berhenti" + LN;
            setmsg += "_Datetime: " + strDateTime() + "_";
          }
          sendLocation(setmsg);
        }

        interval_start = millis();
      }
    }
  }
  // Jika tidak start
  else {
    digitalWrite(RLY_POWER, ON);
  }
  //--------------------------------------------------------------------------
  // Jika gagal konek lebih dari
  if (http_failed >= HTTP_FAILED) {
    http_failed = 0;
    good_signal = false;
  }
  //--------------------------------------------------------------------------
  // LED Signal Kedip2
  if (blink_sim) digitalWrite(LED_SIM, ON);
  else digitalWrite(LED_SIM, OFF);
  //--------------------------------------------------------------------------
  // Debug Serial Data yang diperlukan
  debugData();
  // Update Jam ketika masih nunggu sync
  runningTime();
  // Interupt Kedip2 LED SIM (Kuning) jangan hapus
  updateBlink();
  // Delay agar tidak Stuck ESP32nya
  delay(2);
  //--------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------------------------------
void debugData() {
  static double interval_data;
  if (millis() - interval_data > 3000 && DATA_DEBUG) {
    PC.println(F("----------------------------------------"));
    PC.print(F("GPS Latitude           :"));
    PC.println(gps_latitude, 6);
    PC.print(F("GPS Longitude          :"));
    PC.println(gps_longitude, 6);
    PC.print(F("user Latitude          :"));
    PC.println(user_latitude, 6);
    PC.print(F("User Longitude         :"));
    PC.println(user_longitude, 6);
    PC.print(F("Jarak Awal User & Motor:"));
    PC.print(jarak_awal);
    PC.println(F(" meter"));
    PC.print(F("Jarak Realtime         :"));
    PC.print(jarak_realtime);
    PC.println(F(" meter"));
    PC.print(F("Radius                 :"));
    PC.print(radius);
    PC.println(F(" meter"));

    PC.println();
    interval_data = millis();
  }
}
//--------------------------------------------------------------------------------------------------------