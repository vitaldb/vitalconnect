#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <EEPROM.h>

void vital_setup();

bool vital_wait_conn();
bool vital_is_conn();

void vital_send(const char* buf, size_t len);
bool vital_has_data();
uint8_t vital_recv();
String vital_recv_until(const char ch);

