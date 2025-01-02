//--------------------------------------------------------------------------------------------------------
void sendTelegram(String parameter) {
  String get_url = host_url + parameter;
  if (step_cmd == 1 && !response_cmd) sendCommand("AT+CIPSHUT");
  else if (step_cmd == 2 && !response_cmd) sendCommand("AT+SAPBR=0,1");
  else if (step_cmd == 3 && !response_cmd) sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  else if (step_cmd == 4 && !response_cmd) sendCommand("AT+SAPBR=3,1,\"APN\",\"internet\"");
  else if (step_cmd == 5 && !response_cmd) sendCommand("AT+SAPBR=1,1");
  else if (step_cmd == 6 && !response_cmd) sendCommand("AT+HTTPINIT");
  else if (step_cmd == 7 && !response_cmd) sendCommand("AT+HTTPPARA=\"CID\",1");
  else if (step_cmd == 8 && !response_cmd) sendCommand("AT+HTTPPARA=\"URL\",\"" + get_url + "\"");
  else if (step_cmd == 9 && !response_cmd) sendCommand("AT+HTTPACTION=0");
  else if (step_cmd == 10 && !response_cmd) sendCommand("AT+HTTPREAD");
  else if (step_cmd == 11 && !response_cmd) sendCommand("AT+HTTPTERM");
  // SETELAH COMMAND DIKIRIN, TUNGGU RESPONSE
  if (response_cmd) {
    if (SIM.available() > 0) {
      response = SIM.readString();
      if (response.indexOf("OK") or response.indexOf("ER")) {
        if (SIM_DEBUG) {
          PC.print(F("Response: "));
          PC.println(strNoLine(response));
        }
        if (step_cmd == 10) {
          json = parsingJSON(response);
          PC.print("JSON    : ");
          PC.println(json);
          DeserializationError error = deserializeJson(doc, json);
          if (!error) {
            PC.println(F("JSON PARSING BERHASIL"));
            http_failed = 0;
          } else {
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

    if (millis() - response_timeout > 10000) {
      response_cmd = false;
      step_cmd++;
    }
  }

  if (step_cmd > 11) {
    step_cmd = 1;
    response_cmd = false;
  }
}
//--------------------------------------------------------------------------------------------------------
void readTelegram() {
  String firebase_json = firebaseData();
  String get_url = host_url + "?token=" + bot_token + "&limit=1&offset=" + String(offset) + "&firebase=" + urlEncode(firebase_json);

  if (step_cmd == 1 && !response_cmd) sendCommand("AT+CIPSHUT");
  else if (step_cmd == 2 && !response_cmd) sendCommand("AT+SAPBR=0,1");
  else if (step_cmd == 3 && !response_cmd) sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  else if (step_cmd == 4 && !response_cmd) sendCommand("AT+SAPBR=3,1,\"APN\",\"internet\"");
  else if (step_cmd == 5 && !response_cmd) sendCommand("AT+SAPBR=1,1");
  else if (step_cmd == 6 && !response_cmd) sendCommand("AT+HTTPINIT");
  else if (step_cmd == 7 && !response_cmd) sendCommand("AT+HTTPPARA=\"CID\",1");
  else if (step_cmd == 8 && !response_cmd) sendCommand("AT+HTTPPARA=\"URL\",\"" + get_url + "\"");
  else if (step_cmd == 9 && !response_cmd) sendCommand("AT+HTTPACTION=0");
  else if (step_cmd == 10 && !response_cmd) sendCommand("AT+HTTPREAD");
  else if (step_cmd == 11 && !response_cmd) sendCommand("AT+HTTPTERM");
  // SETELAH COMMAND DIKIRIN, TUNGGU RESPONSE
  if (response_cmd) {
    if (SIM.available() > 0) {
      response = SIM.readString();
      if (response.indexOf("OK") or response.indexOf("ER")) {
        if (SIM_DEBUG) {
          PC.print(F("Response: "));
          PC.println(strNoLine(response));
        }
        if (step_cmd == 10) {
          json = parsingJSON(response);
          PC.print("JSON    : ");
          PC.println(json);
          DeserializationError error = deserializeJson(doc, json);
          if (!error) {
            PC.println(F("JSON PARSING BERHASIL"));
            http_failed = 0;

            if (doc["ok"] == true) {
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
              if (update_id != 0 && msg != "") {
                offset = update_id + 1;
                message = msg;
                PC.println("UPDATE ID: " + String(update_id));
                PC.println("OFFSET ID: " + String(offset));
                PC.println("MESSAGE  : " + message);
              }
              // Jika GPS masuk
              else if (update_id != 0 && _lat != 0 && _lng != 0) {
                offset = update_id + 1;
                PC.println("UPDATE ID: " + String(update_id));
                PC.println("OFFSET ID: " + String(offset));
                // Jika Kondisi Start
                if (start && start_step == STEP_GPS) {
                  user_latitude = _lat;
                  user_longitude = _lng;
                  PC.println("User Latitude        : " + String(user_latitude, 6));
                  PC.println("User Longitude       : " + String(user_longitude, 6));
                  PC.println("Motor Latitude        : " + String(gps_latitude, 6));
                  PC.println("Motor Longitude       : " + String(gps_longitude, 6));
                  // Jika GPS tidak == 0.0
                  if (gps_latitude != 0.0 and gps_longitude != 0.0) {
                    jarak_awal = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);
                    start_step = STEP_RADIUS;
                    PC.println("Jarak User & Motor : " + String(jarak_awal));
                    PC.println("Update Lokasi User");

                    setmsg = "*Update Lokasi User Berhasil*" + LN;
                    // setmsg += "- User Latitude: " + String(user_latitude, 6) + LN;
                    // setmsg += "- User Longitude: " + String(user_longitude, 6) + LN;
                    setmsg += "- Jarak User dan Motor: " + String(jarak_awal) + " meter" + LN;
                    setmsg += "Set Radius Command: Radius=10 meter" + LN + LN;
                    setmsg += "Untuk stop program /berhenti" + LN;
                    setmsg += "_Datetime: " + strDateTime() + "_";
                    sendLocation(setmsg);
                  } else {
                    setmsg = "*Gagal Update Lokasi User*" + LN;
                    setmsg += "Motor tidak mendeteksi Sinyal Lokasi GPS" + LN + LN;
                    setmsg += "Stop Program /berhenti" + LN;
                    setmsg += "_Datetime: " + strDateTime() + "_";
                    sendMessage(setmsg);
                  }
                }
                // Jika Kondisi Belum Start
                else {
                  setmsg = "*Gagal Update Lokasi User*" + LN;
                  setmsg += "Silahkan Ikuti Urutan Step terlebih dahulu" + LN + LN;
                  setmsg += "Stop Program /berhenti" + LN;
                  setmsg += "_Datetime: " + strDateTime() + "_";

                  sendMessage(setmsg);
                }
              }
            }
          } else {
            PC.println(F("JSON PARSING GAGAL"));
            http_failed++;
          }
          PC.println();
        }
        if (step_cmd != 9) {
          response_cmd = false;
          step_cmd++;
        }
      }
    }
    if (millis() - response_timeout > 10000) {
      response_cmd = false;
      step_cmd++;
    }
  }
  if (step_cmd > 11) {
    step_cmd = 1;
    telegram_mode = READ_MESSAGE;
    response_cmd = false;
  }
}
//--------------------------------------------------------------------------------------------------------
// Check Connection Signal
void readSignal() {
  interval_sim = 200;
  if (step_cmd == 1 && !response_cmd) sendCommand("AT+CSQ");
  // SETELAH COMMAND DIKIRIM, TUNGGU RESPONSE
  if (response_cmd) {
    if (SIM.available() > 0) {
      response = SIM.readString();
      if (response.indexOf("+CSQ")) {
        String str_signal = String(response[8]) + String(response[9]);
        sim_signal = str_signal.toInt();
        PC.print("Signal  : ");
        PC.println(sim_signal);
        if (sim_signal > GOOD_SIGNAL) {
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

    if (millis() - response_timeout > 3000) {
      response_cmd = false;
      step_cmd++;
    }
  }

  if (step_cmd > 1) {
    step_cmd = 1;
    response_cmd = false;
  }
}
//--------------------------------------------------------------------------------------------------------