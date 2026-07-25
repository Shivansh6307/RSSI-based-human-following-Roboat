/*
  UTILITY: Get MAC address of an ESP32
  ======================================
  Flash this to each board FIRST (before the real firmware) to find its MAC.
  You need the MASTER's MAC address to hardcode into all 4 receiver nodes.

  Steps:
  1. Upload this sketch to the board you want to identify as MASTER
  2. Open Serial Monitor at 115200 baud
  3. Copy the printed MAC address
  4. Paste it into masterMAC[] in receiver_node.ino (all 4 receivers)
*/

#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFi.mode(WIFI_STA);
  Serial.print("This board's MAC address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // nothing
}
