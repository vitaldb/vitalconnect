#include "Vital.h"
#include <user_interface.h>

os_timer_t myTimer; // 1-second timer

#define LED D6
boolean led_status = false;

void setup() {
  pinMode(LED, OUTPUT);
  
  vital_setup();
  
  os_timer_setfn(&myTimer, timerCallback, NULL);
  os_timer_arm(&myTimer, 1000, true);
}

SerialConfig serial_cfg(String bits) {
  if(bits == "8N1") return SERIAL_8N1;
  if(bits == "8N2") return SERIAL_8N2;
  if(bits == "8O1") return SERIAL_8O1;
  if(bits == "8O2") return SERIAL_8O2;
  if(bits == "8E1") return SERIAL_8E1;
  if(bits == "8E2") return SERIAL_8E2;
  if(bits == "7N1") return SERIAL_7N1;
  if(bits == "7N2") return SERIAL_7N2;
  if(bits == "7O1") return SERIAL_7O1;
  if(bits == "7O2") return SERIAL_7O2;
  if(bits == "7E1") return SERIAL_7E1;
  if(bits == "7E2") return SERIAL_7E2;
  if(bits == "6N1") return SERIAL_6N1;
  if(bits == "6N2") return SERIAL_6N2;
  if(bits == "6O1") return SERIAL_6O1;
  if(bits == "6O2") return SERIAL_6O2;
  if(bits == "6E1") return SERIAL_6E1;
  if(bits == "6E2") return SERIAL_6E2;
  if(bits == "5N1") return SERIAL_5N1;
  if(bits == "5N2") return SERIAL_5N2;
  if(bits == "5O1") return SERIAL_5O1;
  if(bits == "5O2") return SERIAL_5O2;
  if(bits == "5E1") return SERIAL_5E1;
  if(bits == "5E2") return SERIAL_5E2;  
  return SERIAL_8N1; // default
}

const size_t bufLen = 16384; // MTU=1460
volatile uint8_t buf[bufLen];
volatile size_t bufPos = 0;

bool is_converting = false; // 마지막 상태가 서비스 중

// 연결이 되지 않았으면 불이 켜짐
// 데이터가 넘어오지 않으면 불이 꺼짐
// 데이터를 중개중이면 불이 깜박임

// os timer
void timerCallback(void *pArg) {
  if(!vital_is_conn()) {
    digitalWrite(LED, LOW);
    return;
  }
  
  if (bufPos == 0) {
    digitalWrite(LED, HIGH);
    return;
  }
  
  led_status = !led_status;
  digitalWrite(LED, led_status ? HIGH : LOW);

  // 모여진 데이터를 보냄. 아두이노는 어차피 싱글 스레드라 보내는 동안은 시리얼을 읽지 못함
  vital_send((const char*)buf, bufPos);
  bufPos = 0;
} 

void loop() {
  if (!vital_is_conn()) { // 아직 vr에서 접속이 안된 상태
    if(is_converting) { // 직전까지 서빙 중인데 끊겼음
      is_converting = false;
      Serial.swap(); // rs232 포트를 사용 안하고 다시 usb를 사용함
      Serial.println("return back to atmode");
      delay(100);
      return; // 다음번엔 is_converting가 false 이므로 다시 이쪽으로 오지는 않는다.
    }

    // vr 접속을 최대 1초간 기다리면서 시리얼이 있으면 at모드를 처리함
    if(!vital_wait_conn()) {
      delay(100);
      return; // vr 접속이 안되면 될때 까지 이리로 옴
    }
  }

  if (!is_converting) { // vr에서 접속이 되었으나 아직 converting을 시작하지 못함
    // baud rate 등을 기다리고 있는 상태임 -> vr을 통해 보내오는 데이터를 읽음
    String cmd = vital_recv_until('\n');
    int pos = cmd.indexOf(','); // 한줄을 읽었는데 
    if(pos == -1) return; // vr에서 연결 명령을 보내지 않아서 "" 이거나 , 가 없으면. 다시 기다림

    // vr에서 포트 오픈 명령이 넘어왔다.
    // 57600,0,8,1
    // baud rate, parity, data bit, stop bit
    int baud = cmd.substring(0,pos).toInt();
    String bits = cmd.substring(pos+1,pos+4);

    Serial.println("swapping");
    
    Serial.begin(baud, serial_cfg(bits));
    Serial.flush(); // 시리얼 포트를 초기화
    Serial.swap(); // at 모드를 중단하고 rs232 사용을 시작함
    while(Serial.available()) // 일단 현재 있는 것을 다 지움
      Serial.read();
    bufPos = 0;
    is_converting = true; // 다시 들어와도 바로 아래로 내려감
  }

  // converting 중임
  while(Serial.available()) { // 시리얼이 있으면 최우선으로 읽어들임
    buf[bufPos++] = Serial.read(); // 보내는건 타이머에서 보낸다.
    if(bufPos == bufLen) timerCallback(NULL); // 버퍼 오버플로우. 있을 수 없는 일
  }

  if(vital_has_data()) { // vr에서 보내온 데이터가 있다면
    Serial.write(vital_recv()); // 중개함
  }
}

