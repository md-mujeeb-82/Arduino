#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
// #include "esp_log.h"
#include "esp_netif.h"

static const char *TAG = "wifi_sniffer";
static int8_t buffer[1000];

static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *payload = ppkt->payload;

    // Beacon frame check (0x80 subtype)
    if ((payload[0] & 0xF0) == 0x80) {
        uint8_t *bssid = (uint8_t *)&payload[16];
        int8_t rssi = ppkt->rx_ctrl.rssi;

        // ESP_LOGI(TAG,
        //          "Beacon from %02X:%02X:%02X:%02X:%02X:%02X | RSSI: %d | CH: %d",
        //          bssid[0], bssid[1], bssid[2],
        //          bssid[3], bssid[4], bssid[5],
        //          rssi,
        //          ppkt->rx_ctrl.channel);
    }
}

static void wifi_init_sniffer(void)
{
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb);
}

static void sendPacket() {
  esp_wifi_80211_tx(WIFI_IF_STA, buffer, sizeof(buffer), true);
}

static void channel_hop_task(void *arg)
{
    while (1) {
        for (int ch = 1; ch <= 13; ch++) {
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            sendPacket();
            // vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void setup() {
  Serial.begin(115200);
  // nvs_flash_init();
  wifi_init_sniffer();
  xTaskCreate(channel_hop_task, "channel_hop_task", 4096, NULL, 5, NULL);
}

void loop() {
  // Can stay empty
}
