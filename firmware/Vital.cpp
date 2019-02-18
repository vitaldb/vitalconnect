#include "Vital.h"

char ap_ssid[33] = {0, };
char ap_pass[33] = {0, };
char devid[33] = {0, };
const unsigned int udp_local_port = 4401;
const unsigned int udp_remote_port = 4400;
const unsigned int tcp_port = 4300;
WiFiUDP udp_server;
WiFiServer vital_server(tcp_port);
WiFiClient vital_client;

String atmode_line;

void send_variable(String valname, const char* val) {
  Serial.write("get ");
  Serial.write(valname.c_str());
  Serial.write('=');
  Serial.write(val);
  Serial.write('\n');
}

void parse_atcmd(String line) {
  Serial.println("atcmd : " + line);
  int spos = line.indexOf(' '); // eg. set ssid=vitalnet
  String cmd, par;
  if(spos == -1) {
    cmd = line;
  } else {
    cmd = line.substring(0, spos);
    par = line.substring(spos + 1);
  }
  
  if(cmd  == "get") {
    send_variable("ssid", ap_ssid);
    send_variable("pass", ap_pass);
    send_variable("devid", devid);
  } else if(cmd == "set") {
    int sepos = par.indexOf('=');
    if(sepos > 0) {
      String varname = par.substring(0, sepos);
      String val = par.substring(sepos+1);
      if(varname == "ssid") {
        val.toCharArray(ap_ssid, 32);
        for (int i = 0; i < 32; i++) EEPROM.write(i, ap_ssid[i]);
        EEPROM.commit();
        send_variable(varname, ap_ssid);
      } else if(varname == "pass") {
        val.toCharArray(ap_pass, 32);
        for (int i = 0; i < 32; i++) EEPROM.write(32 + i, ap_pass[i]);
        EEPROM.commit();
        send_variable(varname, ap_pass);
      } else if(varname == "devid") {
        val.toCharArray(devid, 32);
        for (int i = 0; i < 32; i++) EEPROM.write(64 + i, devid[i]);
        EEPROM.commit();
        send_variable(varname, devid);
      } 
    }
  }
}

void do_atmode() {
  // set & get settings via usb port
  while (Serial.available()) {
    char ch = Serial.read();
    if(ch == '\n') {
      atmode_line.trim();
      parse_atcmd(atmode_line);
      atmode_line = "";
    } else {
      atmode_line += ch;
    }
  }
}

void vital_setup() {
  // atmode로 시작
  // wifi에 접속되고 start 명령을 받으면 새로운 baud rate로 시작하면서 atmode에서 나옴
  Serial.begin(115200); 

  // read the eeprom data
  Serial.println("Startup");
  Serial.println("Reading EEPROM");
  
  EEPROM.begin(96);
  delay(10);
  for (int i = 0; i < 32; i++) ap_ssid[i] = EEPROM.read(i);
  if (ap_ssid[0] == 0xFF) strcpy(ap_ssid, "vitalnet");
  for (int i = 0; i < 32; i++) ap_pass[i] = EEPROM.read(32 + i);
  if (ap_pass[0] == 0xFF) strcpy(ap_pass, "vitalpass");
  for (int i = 0; i < 32; i++) devid[i] = EEPROM.read(64 + i);
  if (devid[0] == 0xFF) strcpy(devid, "serial_0001");
  Serial.print("ssid: ");
  Serial.println(ap_ssid);
  Serial.print("ap_pass: ");
  Serial.println(ap_pass);
  Serial.print("devid: ");
  Serial.println(devid);
}

bool vital_is_conn() {
  return vital_client.connected();
}

bool last_conn = false;

bool vital_wait_conn() {
  if(WiFi.status() != WL_CONNECTED) { // wifi 접속이 안되었으면
    last_conn = false; // 마지막 상태를 끊긴 상태로
    Serial.println("connecting wifi");
    WiFi.begin(ap_ssid, ap_pass); // 10초간 접속 시도
    unsigned int connectionTimeout = 10000;
    unsigned int timeAvailable = 0;
    while(WiFi.status() != WL_CONNECTED) {
      if (Serial.available()) do_atmode();
      delay(500);
      timeAvailable += 500;
      if (timeAvailable >= connectionTimeout) {
        Serial.println("failed to connect wifi");
        return false; // 다시 시도
      }
    }
  }

  if (Serial.available()) { // 접속이 안된 상태에서 usb에 읽을게 있으면? 한 줄씩 읽고 파징. do atmode
    do_atmode();
    return false;
  }

  if (!last_conn) { // 최초로 접속됨
    last_conn = true;
    udp_server.begin(udp_local_port); // 서버를 시작
    vital_server.begin();
  }
  
  // usb에 읽을게 없음. wifi 접속을 기다림
  Serial.println("waiting for vital_client");
  
  // 접속이 들어오는지 확인
  vital_client = vital_server.available();
  if (!vital_client) {
    Serial.println("broadcasting devid");
    IPAddress localip = WiFi.localIP();
    localip[3] = 255;
    udp_server.beginPacket(localip, udp_remote_port);
    udp_server.write("vital");
    udp_server.write(devid);
    udp_server.endPacket();  
    delay(1000); // 1 second
    return false;
  }

  Serial.println("vital_client connected");
  unsigned int connectionTimeout = 1000; // wait 1 second
  unsigned int timeAvailable = 0;
  while (!vital_client.available()) { // tcp client 접속을 기다림
    delay(10);
    timeAvailable += 10;
    if (timeAvailable >= connectionTimeout) {
      vital_client.stop();
      return false;
    }
  }

  // client connected
  // 일반적인 tcp는 패킷을 여러개씩 모아서 보낸다. 이를 방지
  vital_client.setNoDelay(true);
  
  return true;
  
  // 접속 후의 보내는 코드는 각자 loop 에서 구현해야함
}

void vital_send(const char* buf, size_t len) {
  if(len == 0) return;

  // write 에러는 체크 안함
  vital_client.write((const uint8_t*)buf, len);

  // 보내지는게 없으면 vr 에서 자동으로 끊을거임
}

bool vital_has_data() {
  return vital_client.available();
}

uint8_t vital_recv() {
  return vital_client.read();
}

String vital_recv_until(char ch) {
  return vital_client.readStringUntil('\n');
}

