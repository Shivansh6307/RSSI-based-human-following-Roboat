/*
  RSSI HUMAN-FOLLOWING ROBOT — RECEIVER NODE
  ============================================
  Flash this SAME code to all 4 ESP32 receiver boards.
  Only change NODE_ID (0-3) for each board before uploading.

  NODE_ID mapping (physical placement on robot):
    0 = Front-Left
    1 = Front-Right
    2 = Back-Left
    3 = Back-Right

  What this does:
  - Scans WiFi for the phone's hotspot SSID
  - Reads RSSI (signal strength in dBm)
  - Smooths it with an exponential moving average
  - Sends {node_id, smoothed_rssi} to the master ESP32 via ESP-NOW
*/

#include <WiFi.h>
#include <esp_now.h>

// ------------- CONFIG: EDIT THESE -------------
#define NODE_ID 0                          // CHANGE per board: 0, 1, 2, or 3
const char* TARGET_SSID = "YOUR_PHONE_HOTSPOT_NAME";  // phone hotspot SSID to track

// MAC address of the MASTER ESP32 (get this by running get_mac_address.ino on the master board)
uint8_t masterMAC[] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};  // <-- REPLACE with your master's MAC

const float ALPHA = 0.3;      // smoothing factor (0.1 = very smooth/slow, 0.5 = responsive/jumpy)
const int SCAN_INTERVAL_MS = 300;  // how often to scan
// -----------------------------------------------

typedef struct {
  uint8_t node_id;
  float rssi;
} rssi_packet_t;

rssi_packet_t packet;
float smoothedRSSI = -75.0;  // starting estimate
bool firstReading = true;

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: uncomment to debug send status
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sent OK" : "Send FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add master as peer!");
    return;
  }

  Serial.print("Receiver node ");
  Serial.print(NODE_ID);
  Serial.println(" ready. Scanning for target SSID...");
}

int scanForTargetRSSI() {
  int n = WiFi.scanNetworks(false, false, false, 300); // fast scan, 300ms per channel
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == TARGET_SSID) {
      int rssi = WiFi.RSSI(i);
      WiFi.scanDelete();
      return rssi;
    }
  }
  WiFi.scanDelete();
  return -100; // not found — treat as very far / out of range
}

void loop() {
  int rawRSSI = scanForTargetRSSI();

  if (firstReading) {
    smoothedRSSI = rawRSSI;
    firstReading = false;
  } else {
    smoothedRSSI = ALPHA * rawRSSI + (1 - ALPHA) * smoothedRSSI;
  }

  packet.node_id = NODE_ID;
  packet.rssi = smoothedRSSI;

  esp_now_send(masterMAC, (uint8_t *)&packet, sizeof(packet));

  Serial.print("Node ");
  Serial.print(NODE_ID);
  Serial.print(" | raw RSSI: ");
  Serial.print(rawRSSI);
  Serial.print(" dBm | smoothed: ");
  Serial.print(smoothedRSSI);
  Serial.println(" dBm");

  delay(SCAN_INTERVAL_MS);
}
