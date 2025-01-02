//--------------------------------------------------------------------------------------------------------
// Kumpulan Fungsi2 untuk memperpendek program
//--------------------------------------------------------------------------------------------------------
// GPS LEd
void ledGPS(int count, int wait) {
  if (count == 0) count = 2;
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_GPS, OFF);
    delay(wait);
    digitalWrite(LED_GPS, ON);
    delay(wait);
  }
  digitalWrite(LED_GPS, ON);

//--------------------------------------------------------------------------------------------------------

void startVariable() {
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
float getDistance(double lat1, double lon1, double lat2, double lon2) {
  if (lat1 == 0.0 or lon1 == 0.0 or lat2 == 0.0 or lon2 == 0.0) return 0.0;
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) + cos(radians(lat1)) * cos(radians(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  double d = 6371000 * c;
  return d;
}
//--------------------------------------------------------------------------------------------------------
//
void getUpdateGPS() {
  static double timer_gps;
  // Baca GPS Realtime
  while (GPS.available() > 0) {
    gps.encode(GPS.read());
    if (gps.location.isUpdated() && millis() - timer_gps > 2000) {
      gps_latitude = gps.location.lat();   // Baca Lintang
      gps_longitude = gps.location.lng();  // Baca Bujur
      jarak_realtime = getDistance(gps_latitude, gps_longitude, user_latitude, user_longitude);
      if (GPS_DEBUG) {
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
void sendMessage(String msg) {
  step_cmd = 1;
  send_message = msg;
  telegram_mode = SEND_MESSAGE;
}
//--------------------------------------------------------------------------------------------------------
void sendLocation(String msg) {
  step_cmd = 1;
  send_message = msg;
  telegram_mode = SEND_LOCATION;
}
//--------------------------------------------------------------------------------------------------------
void sendCommand(String cmd) {
  if (SIM_DEBUG) {
    PC.print(F("Command : "));
    PC.println(cmd);
  }
  SIM.println(cmd);
  response_cmd = true;
  response_timeout = millis();
}
//--------------------------------------------------------------------------------------------------------
int parsingRadius(String msg) {
  bool is_radius = false;
  String str = "";
  for (int i = 0; i < msg.length(); i++) {
    if (msg[i] == '=') {
      str = "";
    } else {
      str += msg[i];
      if (str == "Radius") {
        is_radius = true;
      }
    }
  }

  if (is_radius && str.toInt() >= 0) {
    return str.toInt();
  }
  return -1;
}
//--------------------------------------------------------------------------------------------------------
String parsingJSON(String data) {
  String payload = "";
  int len = data.length();
  bool start = false;
  for (int i = 0; i < len; i++) {
    if (!start) {
      if (data[i] == '$') {
        start = true;
      }
    } else {
      if (data[i] == '@') break;
      else if (data[i] == '\n') continue;
      else payload = payload + data[i];
    }
  }
  return payload;
}
//--------------------------------------------------------------------------------------------------------
//
void readRawGPS() {
  while (GPS.available()) {
    PC.print((char)GPS.read());
  }
}
//--------------------------------------------------------------------------------------------------------
void readCommandSIM() {
  while (PC.available()) {
    SIM.write(PC.read());
  }
  while (SIM.available()) {
    PC.write(SIM.read());
  }
}
//--------------------------------------------------------------------------------------------------------
void updateBlink() {
  static double timer_sim;
  if (interval_sim <= 0) blink_sim = 1000;
  if (millis() - timer_sim < interval_sim) blink_sim = true;
  else if (millis() - timer_sim >= interval_sim && millis() - timer_sim < interval_sim * 2) blink_sim = false;
  else if (millis() - timer_sim >= interval_sim * 2) timer_sim = millis();
}
//--------------------------------------------------------------------------------------------------------
String strNoLine(String str) {
  String strtrim = "";
  for (int i = 0; i < str.length(); i++) {
    if (str[i] != '\n' && str[i] != '\r') {
      strtrim += str[i];
    }
  }
  return strtrim;
}
//--------------------------------------------------------------------------------------------------------
String strDateTime() {
  char buff[20];
  sprintf(buff, "%02i/%02i/%04i %02i:%02i:%02i", tgl, bln, thn, jam, mnt, dtk);
  return String(buff);
}
//--------------------------------------------------------------------------------------------------------
void runningTime() {
  static double interval_time;
  if (millis() - interval_time > 1000) {
    dtk++;
    if (dtk >= 60) {
      dtk = 0;
      mnt++;
    }
    if (mnt >= 60) {
      mnt = 0;
      jam++;
    }
    if (jam >= 24) {
      jam = 0;
    }
    interval_time = millis();
  }
}

//--------------------------------------------------------------------------------------------------------
// Fake GPS untuk uji dengan Serial, agar mudah untuk uji program
// format: !lat!long! Contoh: !-8.036880!112.616305!
// format: !lat!long! Contoh: !-8.002000!112.629521!
void SerialFakeGPS() {
  // Jika Serial Input PC masuk
  if (PC.available() > 0) {
    String input = PC.readString();
    String parse[10];
    int x = 0;
    for (int i = 0; i < input.length(); i++) {
      if (input[i] == '!') {
        x++;
        parse[x] = "";
      } else {
        parse[x] += input[i];
      }
    }
    //Parse berhasil
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
String firebaseData() {
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
