#include "Arduino.h"
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
#define HTTP_FAILED 1      // Gagal konek 3 kali check signal
#define SIM_DEBUG true     // Serial ke PC SIM command
#define GPS_DEBUG true    // Serial ke PC update GPS
#define DATA_DEBUG false   // Serial ke PC Semua Data Yang Perlu
#define GOOD_SIGNAL 15     // lebih dari 15 = Signal Bagus
#define LOOPING_INTERVAL 2 // Kirim 1 menit sekali
#define ALERT_INTERVAL 1   // Kirim 1 menit sekali
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
const String host_url = "HOST_URL";        // Host
const String bot_token = "Token_bot"; // Bot Key
const String chat_id = "IDBot";                                       // Chat ID
// int maks_jarak = 20;                                                       // jarak maksimal alarm
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
void debugData()
{
  static double interval_data;
  if (millis() - interval_data > 3000 && DATA_DEBUG)
  {
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
//--------------------------------------------------------------------------------------------------------
// Kumpulan Fungsi2 untuk memperpendek program
//--------------------------------------------------------------------------------------------------------
// GPS LEd
void ledGPS(int count, int wait)
{
  if (count == 0)
    count = 2;
  for (int i = 0; i < count; i++)
  {
    digitalWrite(LED_GPS, OFF);
    delay(wait);
    digitalWrite(LED_GPS, ON);
    delay(wait);
  }
  digitalWrite(LED_GPS, ON);
}
//---
//--------------------------------------------------------------------------------------------------------

void startVariable()
{
  step_cmd = 1;
  start = false;
  start_step = STEP_AWAL;
  start_mode = MODE_AWAL;
  interval_loop = LOOPING_INTERVAL;
  message_looping = true;
  alert = false;
}
//--------------------------------------------------------------------------------------------------------
// Fungsi untuk mengukur jarak
float getDistance(double lat1, double lon1, double lat2, double lon2)
{
  if (lat1 == 0.0 or lon1 == 0.0 or lat2 == 0.0 or lon2 == 0.0)
    return 0.0;
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) + cos(radians(lat1)) * cos(radians(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  double d = 6371000 * c;
  return d;
}
//--------------------------------------------------------------------------------------------------------
//
void getUpdateGPS()
{
  static double timer_gps;
  // Baca GPS Realtime
  while (GPS.available() > 0)
  {
    gps.encode(GPS.read());
    if (gps.location.isUpdated() && millis() - timer_gps > 2000)
    {
      gps_latitude = gps.location.lat();  // Baca Lintang
      gps_longitude = gps.location.lng(); // Baca Bujur
      jarak_realtime = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);
      if (GPS_DEBUG)
      {
        Serial.println(F("Update GPS"));
        Serial.print(F("Latitude : "));
        Serial.println(gps_latitude);
        Serial.print(F("Longitude: "));
        Serial.println(gps_longitude);
        Serial.println();
      }
      ledGPS(3, 80);
      timer_gps = millis();
    }
  }
}

//--------------------------------------------------------------------------------------------------------
void sendMessage(String msg)
{
  step_cmd = 1;
  send_message = msg;
  telegram_mode = SEND_MESSAGE;
}
//--------------------------------------------------------------------------------------------------------
void sendLocation(String msg)
{
  step_cmd = 1;
  send_message = msg;
  telegram_mode = SEND_LOCATION;
}
//--------------------------------------------------------------------------------------------------------
void sendCommand(String cmd)
{
  if (SIM_DEBUG)
  {
    PC.print(F("Command : "));
    PC.println(cmd);
  }
  SIM.println(cmd);
  response_cmd = true;
  response_timeout = millis();
}
//--------------------------------------------------------------------------------------------------------
int parsingRadius(String msg)
{
  bool is_radius = false;
  String str = "";
  for (int i = 0; i < msg.length(); i++)
  {
    if (msg[i] == '=')
    {
      str = "";
    }
    else
    {
      str += msg[i];
      if (str == "Radius")
      {
        is_radius = true;
      }
    }
  }

  if (is_radius && str.toInt() >= 0)
  {
    return str.toInt();
  }
  return -1;
}
//--------------------------------------------------------------------------------------------------------
String parsingJSON(String data)
{
  String payload = "";
  int len = data.length();
  bool start = false;
  for (int i = 0; i < len; i++)
  {
    if (!start)
    {
      if (data[i] == '$')
      {
        start = true;
      }
    }
    else
    {
      if (data[i] == '@')
        break;
      else if (data[i] == '\n')
        continue;
      else
        payload = payload + data[i];
    }
  }
  return payload;
}
//--------------------------------------------------------------------------------------------------------
//
void readRawGPS()
{
  while (GPS.available())
  {
    PC.print((char)GPS.read());
  }
}
//--------------------------------------------------------------------------------------------------------
//
void readCommandSIM()
{
  while (PC.available())
  {
    SIM.write(PC.read());
  }
  while (SIM.available())
  {
    PC.write(SIM.read());
  }
}
//--------------------------------------------------------------------------------------------------------
void updateBlink()
{
  static double timer_sim;
  if (interval_sim <= 0)
    blink_sim = 1000;
  if (millis() - timer_sim < interval_sim)
    blink_sim = true;
  else if (millis() - timer_sim >= interval_sim && millis() - timer_sim < interval_sim * 2)
    blink_sim = false;
  else if (millis() - timer_sim >= interval_sim * 2)
    timer_sim = millis();
}
//--------------------------------------------------------------------------------------------------------
String strNoLine(String str)
{
  String strtrim = "";
  for (int i = 0; i < str.length(); i++)
  {
    if (str[i] != '\n' && str[i] != '\r')
    {
      strtrim += str[i];
    }
  }
  return strtrim;
}
//--------------------------------------------------------------------------------------------------------
String strDateTime()
{
  char buff[20];
  sprintf(buff, "%02i/%02i/%04i %02i:%02i:%02i", tgl, bln, thn, jam, mnt, dtk);
  return String(buff);
}
//--------------------------------------------------------------------------------------------------------
void runningTime()
{
  static double interval_time;
  if (millis() - interval_time > 1000)
  {
    dtk++;
    if (dtk >= 60)
    {
      dtk = 0;
      mnt++;
    }
    if (mnt >= 60)
    {
      mnt = 0;
      jam++;
    }
    if (jam >= 24)
    {
      jam = 0;
    }
    interval_time = millis();
  }
}

//--------------------------------------------------------------------------------------------------------
// Fake GPS untuk uji dengan Serial, agar mudah untuk uji program
// format: !lat!long! Contoh: !-8.036880!112.616305!
// format: !lat!long! Contoh: !-8.002000!112.629521!
void SerialFakeGPS()
{
  // Jika Serial Input PC masuk
  if (PC.available() > 0)
  {
    String input = PC.readString();
    String parse[10];
    int x = 0;
    for (int i = 0; i < input.length(); i++)
    {
      if (input[i] == '!')
      {
        x++;
        parse[x] = "";
      }
      else
      {
        parse[x] += input[i];
      }
    }
    // Parse berhasil
    gps_latitude = parse[1].toDouble();
    gps_longitude = parse[2].toDouble();
    jarak_realtime = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);

    PC.println(F("------------------------------"));
    PC.print(F("PC Latitude    : "));
    PC.println(gps_latitude, 6);
    PC.print(F("PC Longitude   : "));
    PC.println(gps_longitude, 6);
    PC.print(F("Jarak Realtime : "));
    PC.print(jarak_realtime);
    PC.println(F(" meter"));
    PC.println();
  }
}
//--------------------------------------------------------------------------------------------------------
String firebaseData()
{
  String fb_json = "{";
  fb_json += "\"gps_lat\":" + String(gps_latitude, 6) + ",";
  fb_json += "\"gps_lon\":" + String(gps_longitude, 6) + ",";
  fb_json += "\"user_lat\":" + String(user_latitude, 6) + ",";
  fb_json += "\"user_lon\":" + String(user_longitude, 6) + ",";
  fb_json += "\"radius\":" + String(radius) + ",";
  fb_json += "\"jarak\":" + String(jarak_realtime) + "";
  fb_json += "}";
  return fb_json;
}
//--------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------
void sendTelegram(String parameter)
{
  String get_url = host_url + parameter;
  if (step_cmd == 1 && !response_cmd)
    sendCommand("AT+CIPSHUT");
  else if (step_cmd == 2 && !response_cmd)
    sendCommand("AT+SAPBR=0,1");
  else if (step_cmd == 3 && !response_cmd)
    sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  else if (step_cmd == 4 && !response_cmd)
    sendCommand("AT+SAPBR=3,1,\"APN\",\"internet\"");
  else if (step_cmd == 5 && !response_cmd)
    sendCommand("AT+SAPBR=1,1");
  else if (step_cmd == 6 && !response_cmd)
    sendCommand("AT+HTTPINIT");
  else if (step_cmd == 7 && !response_cmd)
    sendCommand("AT+HTTPPARA=\"CID\",1");
  else if (step_cmd == 8 && !response_cmd)
    sendCommand("AT+HTTPPARA=\"URL\",\"" + get_url + "\"");
  else if (step_cmd == 9 && !response_cmd)
    sendCommand("AT+HTTPACTION=0");
  else if (step_cmd == 10 && !response_cmd)
    sendCommand("AT+HTTPREAD");
  else if (step_cmd == 11 && !response_cmd)
    sendCommand("AT+HTTPTERM");
  // SETELAH COMMAND DIKIRIN, TUNGGU RESPONSE
  if (response_cmd)
  {
    if (SIM.available() > 0)
    {
      response = SIM.readString();
      if (response.indexOf("OK") or response.indexOf("ER"))
      {
        if (SIM_DEBUG)
        {
          PC.print(F("Response: "));
          PC.println(strNoLine(response));
        }
        if (step_cmd == 10)
        {
          json = parsingJSON(response);
          PC.print("JSON    : ");
          PC.println(json);
          DeserializationError error = deserializeJson(doc, json);
          if (!error)
          {
            PC.println(F("JSON PARSING BERHASIL"));
            http_failed = 0;
          }
          else
          {
            http_failed++;
          }
          PC.println();
          // BErhasil terkirim, kembali mode Baca
          telegram_mode = READ_MESSAGE;
        }
        response_cmd = false;
        step_cmd++;
      }
    }

    if (millis() - response_timeout > 10000)
    {
      response_cmd = false;
      step_cmd++;
    }
  }

  if (step_cmd > 11)
  {
    step_cmd = 1;
    response_cmd = false;
  }
}
//--------------------------------------------------------------------------------------------------------
void readTelegram()
{
  String firebase_json = firebaseData();
  String get_url = host_url + "?token=" + bot_token + "&limit=1&offset=" + String(offset) + "&firebase=" + urlEncode(firebase_json);

  if (step_cmd == 1 && !response_cmd)
    sendCommand("AT+CIPSHUT");
  else if (step_cmd == 2 && !response_cmd)
    sendCommand("AT+SAPBR=0,1");
  else if (step_cmd == 3 && !response_cmd)
    sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  else if (step_cmd == 4 && !response_cmd)
    sendCommand("AT+SAPBR=3,1,\"APN\",\"internet\"");
  else if (step_cmd == 5 && !response_cmd)
    sendCommand("AT+SAPBR=1,1");
  else if (step_cmd == 6 && !response_cmd)
    sendCommand("AT+HTTPINIT");
  else if (step_cmd == 7 && !response_cmd)
    sendCommand("AT+HTTPPARA=\"CID\",1");
  else if (step_cmd == 8 && !response_cmd)
    sendCommand("AT+HTTPPARA=\"URL\",\"" + get_url + "\"");
  else if (step_cmd == 9 && !response_cmd)
    sendCommand("AT+HTTPACTION=0");
  else if (step_cmd == 10 && !response_cmd)
    sendCommand("AT+HTTPREAD");
  else if (step_cmd == 11 && !response_cmd)
    sendCommand("AT+HTTPTERM");
  // SETELAH COMMAND DIKIRIN, TUNGGU RESPONSE
  if (response_cmd)
  {
    if (SIM.available() > 0)
    {
      response = SIM.readString();
      if (response.indexOf("OK") or response.indexOf("ER"))
      {
        if (SIM_DEBUG)
        {
          PC.print(F("Response: "));
          PC.println(strNoLine(response));
        }
        if (step_cmd == 10)
        {
          json = parsingJSON(response);
          PC.print("JSON    : ");
          PC.println(json);
          DeserializationError error = deserializeJson(doc, json);
          if (!error)
          {
            PC.println(F("JSON PARSING BERHASIL"));
            http_failed = 0;

            if (doc["ok"] == true)
            {
              PC.println(F("BERHASIL PARSING JSON"));
              String msg = doc["text"];
              long update_id = doc["update_id"];
              double _lat = doc["location"]["latitude"];
              double _lng = doc["location"]["longitude"];
              int _year = doc["datetime"]["year"];
              int _month = doc["datetime"]["month"];
              int _day = doc["datetime"]["day"];
              int _hour = doc["datetime"]["hour"];
              int _minute = doc["datetime"]["minute"];
              int _second = doc["datetime"]["second"];
              thn = _year;
              bln = _month;
              tgl = _day;
              jam = _hour;
              mnt = _minute;
              dtk = _second;
              // Jika Pesan Text Masuk
              if (update_id != 0 && msg != "")
              {
                offset = update_id + 1;
                message = msg;
                PC.println("UPDATE ID: " + String(update_id));
                PC.println("OFFSET ID: " + String(offset));
                PC.println("MESSAGE  : " + message);
              }
              // Jika GPS masuk
              else if (update_id != 0 && _lat != 0 && _lng != 0)
              {
                offset = update_id + 1;
                PC.println("UPDATE ID: " + String(update_id));
                PC.println("OFFSET ID: " + String(offset));
                // Jika Kondisi Start
                if (start && start_step == STEP_GPS)
                {
                  user_latitude = _lat;
                  user_longitude = _lng;
                  PC.println("User Latitude        : " + String(user_latitude, 6));
                  PC.println("User Longitude       : " + String(user_longitude, 6));
                  PC.println("Motor Latitude        : " + String(gps_latitude, 6));
                  PC.println("Motor Longitude       : " + String(gps_longitude, 6));
                  // Jika GPS tidak == 0.0
                  if (gps_latitude != 0.0 and gps_longitude != 0.0)
                  {
                    jarak_awal = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);
                    start_step = STEP_RADIUS;
                    PC.println("Jarak User & Motor : " + String(jarak_awal));
                    PC.println("Update Lokasi User");

                    setmsg = "*Update Lokasi Alat Berhasil*" + LN;
                    // setmsg += "- User Latitude: " + String(user_latitude, 6) + LN;
                    // setmsg += "- User Longitude: " + String(user_longitude, 6) + LN;
                    setmsg += "Jarak User dan Motor: " + String(jarak_awal) + " meter" + LN;
                    setmsg += "- Contoh Set Radius Command: *Radius=10* meter" + LN + LN;
                    // setmsg += "Silahkan Set Radius, untuk melanjutkan program. Contoh Set Radius Command: *Radius=10* meter" + LN + LN;
                    setmsg += "Untuk stop program /berhenti" + LN;
                    setmsg += "_Datetime: " + strDateTime() + "_";
                    sendLocation(setmsg);
                  }
                  else
                  {
                    setmsg = "*Gagal Lokasi User Berhasil*" + LN;
                    setmsg += "Motor tidak mendeteksi Sinyal Lokasi GPS" + LN + LN;
                    setmsg += "Stop Program /berhenti" + LN;
                    setmsg += "_Datetime: " + strDateTime() + "_";
                    sendMessage(setmsg);
                  }
                }
                // Jika Kondisi Belum Start
                else
                {
                  setmsg = "*Gagal Update Lokasi User*" + LN;
                  setmsg += "Silahkan Ikuti Urutan Step terlebih dahulu" + LN + LN;
                  setmsg += "Stop Program /berhenti" + LN;
                  setmsg += "_Datetime: " + strDateTime() + "_";

                  sendMessage(setmsg);
                }
              }
            }
          }
          else
          {
            PC.println(F("JSON PARSING GAGAL"));
            http_failed++;
          }
          PC.println();
        }
        if (step_cmd != 9)
        {
          response_cmd = false;
          step_cmd++;
        }
      }
    }
    if (millis() - response_timeout > 10000)
    {
      response_cmd = false;
      step_cmd++;
    }
  }
  if (step_cmd > 11)
  {
    step_cmd = 1;
    telegram_mode = READ_MESSAGE;
    response_cmd = false;
  }
}
//--------------------------------------------------------------------------------------------------------
// Check Connection Signal 
void readSignal()
{
  interval_sim = 200;
  if (step_cmd == 1 && !response_cmd)
    sendCommand("AT+CSQ");
  // SETELAH COMMAND DIKIRIM, TUNGGU RESPONSE
  if (response_cmd)
  {
    if (SIM.available() > 0)
    {
      response = SIM.readString();
      if (response.indexOf("+CSQ"))
      {
        String str_signal = String(response[8]) + String(response[9]);
        sim_signal = str_signal.toInt();
        PC.print("Signal  : ");
        PC.println(sim_signal);
        if (sim_signal > GOOD_SIGNAL)
        {
          good_signal = true;
          step_cmd = 1;
          response_cmd = false;
          interval_sim = 1000;
        }
        response_cmd = false;
        step_cmd++;
        PC.println();
      }
    }

    if (millis() - response_timeout > 3000)
    {
      response_cmd = false;
      step_cmd++;
    }
  }

  if (step_cmd > 1)
  {
    step_cmd = 1;
    response_cmd = false;
  }
}
//--------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------
void setup()
{
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
  setmsg += "_Status Relay On_" + LN;
  setmsg += "- Untuk Start System, perintah /mulai" + LN;
  sendMessage(setmsg);
}
//--------------------------------------------------------------------------------------------------------
void loop()
{
  //--------------------------------------------------------------------------
  // Read Inputan
  // readCommandSIM(); // Coba AT Command Serial
  // readRawGPS(); // Baca Raw GPS
  SerialFakeGPS();
  getUpdateGPS(); // Baca GPS Long Lat
  //--------------------------------------------------------------------------
  // Jika Signal buruk
  if (!good_signal)
  {
    readSignal();
  }
  // Jika Signal Bagus
  else
  {
    // Baca SIM kirim telegram atau baca pesan
    if (telegram_mode == SEND_MESSAGE)
      sendTelegram("?token=" + bot_token + "&chat_id=" + chat_id + "&text=" + urlEncode(send_message)); // Kirim Pesan Telegram
    else if (telegram_mode == SEND_LOCATION)
      sendTelegram("?token=" + bot_token + "&chat_id=" + chat_id + "&latitude=" + String(gps_latitude, 6) + "&longitude=" + String(gps_longitude, 6) + "&text=" + urlEncode(send_message)); // Kirim Pesan Telegram
    else
      readTelegram(); // Baca Pesan Telegram secara realtime
  }
  //--------------------------------------------------------------------------
  // Baca Pesan Masuk
  if (message != "")
  {
    // Jika pesan Mulai dan Step ke 1
    if (message == "/mulai" && start_step == 1)
    {
      start = true;
      start_step = STEP_GPS;
      gps_latitude = 0.0;
      gps_longitude = 0.0;

      PC.println("Mulai / Start System");
      PC.println();

      setmsg = "*Mulai / Start System*" + LN;
      // setmsg += "- Kirim lokasi GPS Terkini anda untuk memperbarui Lokasi User" + LN + LN;
      setmsg += "Kirim Lokasi Anda untuk melanjutkan program" + LN;
      setmsg += "- Gunakan Fitur *Send My Current Location* atau Kirim Lokasi Saya" + LN + LN;
      // setmsg += "Untuk menghentikan program gunakan perintah /berhenti " + LN;
      setmsg += "/berhenti : Untuk Menghentikan Program" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendMessage(setmsg);
    }
    // Jika pesan Berhenti
    else if (message == "/berhenti" && start_step > STEP_AWAL)
    {
      startVariable();

      PC.println("Berhenti / Stop System");
      PC.println();

      setmsg = "*Berhenti / Stop System*" + LN;
      setmsg += "- Untuk Start System kembali, gunakan perintah /mulai" + LN + LN;
      // setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendMessage(setmsg);
    }
    // Jika pesan Radius
    else if (message != "" && start_step == STEP_RADIUS)
    {
      int read_radius = parsingRadius(message);
      PC.println(F("Parse Radius GPS"));
      PC.print(F("Radius Read: "));
      PC.print(read_radius);
      PC.println(F(" meter"));
      PC.println();
      if (read_radius > jarak_awal)
      {
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
      }
      else
      {
        PC.println("Set Radius GPS Gagal");
        PC.println();
        setmsg = "*Gagal Set Radius GPS*" + LN;
        setmsg += "- Jarak Radius Harus lebih dari >" + String(jarak_awal) + " meter" + LN;
        setmsg += "- Command Set Radius: *Radius=10* (meter)" + LN + LN;
        setmsg += "Stop Program /berhenti" + LN;
        setmsg += "_Datetime: " + strDateTime() + "_";
      }
      sendMessage(setmsg);
    }
    // Jika pesan Mode Normal
    else if (message == "/modenormal" && start_step == STEP_MODE)
    {
      PC.println("Set Mode Normal");
      PC.println();

      start_mode = MODE_NORMAL;
      message_looping = true;
      alert = false;
      interval_start = millis();

      setmsg = "*Berhasil Set Mode Normal*" + LN;
      // setmsg += "- /updatelokasisaatini" + LN;
      // setmsg += "- /pantau" + LN + LN;
      setmsg += "_Mode Normal Aktif_" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";

      sendMessage(setmsg);
    }
    // Jika pesan Mode Normal
    else if (message == "/modejaga" && start_step == STEP_MODE)
    {
      PC.println("Set Mode JAGA");
      PC.println();

      start_mode = MODE_JAGA;
      message_looping = true;
      alert = false;
      interval_start = millis();

      setmsg = "*Berhasil Set Mode Jaga*" + LN;
      // setmsg += "- /updatelokasimotor" + LN;
      // setmsg += "- /lacak" + LN + LN;
      setmsg += "_Mode Jaga Aktif_" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendMessage(setmsg);
    }

    // Jika pesan Mode Normal && Step : Mode && pesan /updatelokasisaatini
    // Kirim Lokasi Motor dan matikan mode Looping
    else if (message == "/updatelokasisaatini" && start_mode == MODE_NORMAL && start_step == STEP_MODE)
    {
      PC.println("Update Lokasi Saat Ini");
      PC.println("Kirim Lokasi dan Matikan pesan Looping");
      PC.println();

      jarak_realtime = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);
      message_looping = false;
      interval_start = millis();

      setmsg = "*Update Lokasi Alat Saat Ini*" + LN;
      setmsg += "- Jarak User dan Alat: " + String(jarak_realtime) + " meter" + LN;
      setmsg += "_Looping Dihentikan_" + LN;
      // setmsg += "- /updatelokasisaatini" + LN;
      setmsg += "- /pantau" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendLocation(setmsg);
    }
    // Jika pesan Mode Normal && Step : Mode && pesan /pantau
    // Kirim Lokasi Motor dan matikan mode Looping
    else if (message == "/pantau" && start_mode == MODE_NORMAL && start_step == STEP_MODE)
    {
      PC.println("Update Lokasi Saat Ini");
      PC.println("Kirim Lokasi dan Aktifkan pesan Looping");
      PC.println();

      jarak_realtime = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);
      message_looping = true;
      interval_start = millis();

      // setmsg = "*Update Lokasi Saat Ini*" + LN;
      setmsg = "*Pemantauan Alat Aktif*" + LN;
      setmsg += "- Jarak User dan Alat: " + String(jarak_realtime) + " meter" + LN;
      setmsg += "_Looping Diaktifkan_" + LN;
      setmsg += "- /updatelokasisaatini" + LN;
      // setmsg += "- /pantau" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendLocation(setmsg);
    }

    // Jika pesan Mode Jaga && Step : Mode && pesan /updatelokasimotor
    // Kirim Lokasi Motor dan matikan mode Looping /updatelokasialat
    else if (message == "/updatelokasialat" && start_mode == MODE_JAGA && start_step == STEP_MODE)
    {
      PC.println("Update Lokasi Saat Ini");
      PC.println("Kirim Lokasi dan Matikan pesan Looping");
      PC.println();

      message_looping = false;
      interval_start = millis();

      setmsg = "*Update Lokasi Alat Saat Ini*" + LN;
      setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
      setmsg += "_Looping Dihentikan_" + LN;
      // setmsg += "- /updatelokasimotor" + LN;
      setmsg += "- /lacak" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendLocation(setmsg);
    }
    // Jika pesan Mode Normal && Step : Mode && pesan /pantau
    // Kirim Lokasi Motor dan matikan mode Looping
    else if (message == "/lacak" && start_mode == MODE_JAGA && start_step == STEP_MODE)
    {
      PC.println("Update Lokasi Saat Ini");
      PC.println("Kirim Lokasi dan Aktifkan pesan Looping");
      PC.println();

      message_looping = true;
      interval_start = millis();

      setmsg = "*Pelacakan Alat Aktif*" + LN;
      setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
      setmsg += "_Looping Diaktifkan_" + LN;
      setmsg += "- /updatelokasialat" + LN;
      // setmsg += "- /lacak" + LN + LN;
      setmsg += "Stop Program /berhenti" + LN;
      setmsg += "_Datetime: " + strDateTime() + "_";
      sendLocation(setmsg);
    }

    message = "";
  }
  //--------------------------------------------------------------------------
  // Jika dalam kondisi Start
  if (start)
  {
    if (start_mode != MODE_AWAL)
    {
      // Cek Update Jarak Realtime dan Radius untuk mode jaga
      if (start_mode == MODE_JAGA && jarak_realtime >= radius)
      {
        alert = true;
      }
      // Jika Alert Aktif
      if (alert)
      {
        digitalWrite(RLY_POWER, OFF);
        message_looping = true;
        interval_loop = ALERT_INTERVAL;
      }
      else
      {
        interval_loop = LOOPING_INTERVAL;
      }

      // Interval Looping Kirim Pesan
      if (millis() - interval_start > 60000 * interval_loop && message_looping)
      {
        if (start_mode == MODE_NORMAL)
        {
          Serial.println(F("MODE: NORMAL"));
          Serial.println();
          setmsg = "*Pemantauan Alat Aktif*" + LN;
          setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
          setmsg += "_Status Relay On_" + LN;
          setmsg += "- /updatelokasisaatini" + LN + LN;
          setmsg += "Stop Program /berhenti" + LN;
          setmsg += "_Datetime: " + strDateTime() + "_";
          sendLocation(setmsg);
        }
        else if (start_mode == MODE_JAGA)
        {

          Serial.println(F("MODE: JAGA"));
          Serial.println();
          if (alert)
          {
            setmsg = "*Warning! Motor melebihi Radius*" + LN;
            setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
            setmsg += "_Status Relay Off_" + LN + LN;
            setmsg += "Stop Program /berhenti" + LN;
            setmsg += "_Datetime: " + strDateTime() + "_";
          }
          else
          {
            setmsg = "*Pelacakan Alat Aktif*" + LN;
            setmsg += "- Jarak User dan Motor: " + String(jarak_realtime) + " meter" + LN;
            setmsg += "_Status Relay On_" + LN;
            setmsg += "- /updatelokasialat" + LN + LN;
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
  else
  {
    digitalWrite(RLY_POWER, ON);
  }
  //--------------------------------------------------------------------------
  // Jika gagal konek lebih dari
  if (http_failed >= HTTP_FAILED)
  {
    http_failed = 0;
    good_signal = false;
  }
  //--------------------------------------------------------------------------
  // LED Signal Kedip2
  if (blink_sim)
    digitalWrite(LED_SIM, ON);
  else
    digitalWrite(LED_SIM, OFF);
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