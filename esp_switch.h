#include "esphome.h"
#include "espnow.h"
#include <array>
#include <cstdlib>
#include <map>
#include <string>

using mac_address_type = std::array<std::uint8_t, 6U>;

class ESPSwitch;
namespace {
static std::map<mac_address_type, ESPSwitch *> switches;
static bool esp_is_init{false};
} // namespace

void OnDataSent(std::uint8_t *mac, std::uint8_t status);
void OnDataRecv(std::uint8_t *mac, std::uint8_t *data, std::uint8_t length);

struct SwitchStatus {
    bool status{false};
};

/*
 * @brief Converts MAC address string in format "ab:cd:ef:12:34:56" to an array of six uint8_t that can be handled by
 * the interfaces of the ESP libraries.
 */
mac_address_type MacStringToIntegerArray(const std::string &mac_str) {
    mac_address_type mac_array = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    if (mac_str.size() < 17) {
        return mac_array;
    }
    for (uint8_t index{0U}; index < 17U; index += 3U) {
        mac_array[index / 3U] = strtoul(mac_str.substr(index, 2U).c_str(), nullptr, 16U);
    }
    return mac_array;
}

/*
 * @brief A class that connects to home assistant and can publish a state, received over ESP-Now through the OnDataRecv
 * callback
 */
class ESPSwitch : public Component, public BinarySensor {
    bool state{true};

  public:
    /*
     * @brief On construction add this switch to the global map using the mac as key so that it can be found again by
     * the OnData Recv. Going this way we can asynchronously handle the incoming data.
     */
    ESPSwitch(const std::string &mac) { switches[MacStringToIntegerArray(mac)] = this; }

    /*
     * @brief Wait until WiFi is properly configured to not screw up the ESP-Now init
     */
    float get_setup_priority() const override { return setup_priority::WIFI - 1; }

    /*
     * @brief Called after every boot and will init ESP-Now if not already done by a previous ESPSwitch instance.
     */
    void setup() override {
        Serial.begin(115200);
        if (!esp_is_init) {
            if (esp_now_init() != 0) {
                Serial.printf("Failed to initialize ESP-NOW!");
                return;
            }

            if (esp_now_set_self_role(ESP_NOW_ROLE_SLAVE) != 0) {
                Serial.printf("Failed to set self role!");
                return;
            }

            if (esp_now_register_recv_cb(OnDataRecv) != 0) {
                Serial.printf("Failed register OnDataRecv!");
                return;
            }
            esp_is_init = true;
        }
        Serial.printf("ESP-NOW was already initialized. Skipping.");
    }
};

/*
 * @brief On reception of an ESP-Now message, the corresponding switch is looked up in the global map and its state is
 * updated accordingly
 */
void OnDataRecv(std::uint8_t *mac, std::uint8_t *data, std::uint8_t length) {
    SwitchStatus switch_status;
    memcpy(&switch_status, data, length);
    mac_address_type mac_address{0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    std::copy(mac, mac + 6U, mac_address.begin());

    Serial.printf("SwitchStatus received: %s", switch_status.status ? "true" : "false");

    auto s = switches.find(mac_address);
    if (s != switches.end()) {
        s->second->publish_state(switch_status.status);
    } else {
        Serial.printf("You forgot to register a switch!");
    }
}
