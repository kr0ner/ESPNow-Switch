
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

#include <chrono>
extern "C" {
#include "user_interface.h"
}

#define TOGGLE_SWITCH

struct SwitchStatus {
    bool status{false};
};

namespace {
SwitchStatus switch_status;
static constexpr uint8_t wifi_channel{11U};
static std::array<uint8_t, 6U> master{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
} // namespace

void OnDataSent(std::uint8_t * /*mac*/, std::uint8_t /*status*/) {}

void OnDataRecv(std::uint8_t * /*mac*/, std::uint8_t * /*data*/, std::uint8_t /*length*/) {}

void publish_state(SwitchStatus status) { esp_now_send(master.data(), (uint8_t *)&status, sizeof(SwitchStatus)); }

/*
 * @brief Function that resets WiFi settings to make ESP-NOW work correctly
 */
void WiFiReset() {
    //
    WiFi.persistent(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

/*
 * @brief Setup routine that is called on every boot. Will configure the ESP to communicate via ESP now with the master.
 */
void setup() {
    WiFiReset();
    wifi_set_channel(wifi_channel);
    WiFi.mode(WIFI_STA);
    WiFi.setOutputPower(20.5);

    if (esp_now_init() != 0) {
        return;
    }

    if (esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER) != 0) {
        return;
    }

    if (esp_now_register_send_cb(OnDataSent) != 0) {
        return;
    }

    if (esp_now_add_peer(master.data(), ESP_NOW_ROLE_CONTROLLER, wifi_channel, nullptr, 0) != 0) {
        return;
    }

    if (esp_now_register_recv_cb(OnDataRecv) != 0) {
        return;
    }

#ifdef TOGGLE_SWITCH
    /*
     * The toggle switch needs to know the previous state to work. State will be saved in EEPROM.
     * As always : do not toggle at a high frequency otherwise the EEPROm will fail (~100K writes only)
     */
    EEPROM.begin(512);
    switch_status.status = EEPROM.read(0x00);
    EEPROM.write(0x00, !switch_status.status);
    EEPROM.commit();
    EEPROM.end();
#else
    // ToDo: add instructions where to connect pin 3 to
    pinMode(3U, INPUT);
    switch_status.status = !(digitalRead(3U) == HIGH);
#endif
}

/*
 * @brief Loop is called constantly and sends out the current switch state until a certain timeout is reached. After
 * that the ESP goes to deep-sleep and will only wake-up on a falling edge on reset pin.
 */
void loop() {
    static auto timestamp = std::chrono::steady_clock::now();
    publish_state(switch_status);
    delay(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1)).count());
    if (std::chrono::steady_clock::now() - timestamp > std::chrono::seconds(15)) {
        ESP.deepSleep(0U);
    }
}