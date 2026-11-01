#include "improv_consume.h"

#include <WiFi.h>

#include "DisplayManager.h"
#include "ServerManager.h"
#include "SettingsManager.h"
#include "globals.h"

// Room for the longest Improv packet: 9 header bytes, up to 255 data bytes and the checksum.
uint8_t x_buffer[9 + 255 + 1];
size_t x_position = 0;

void set_state(improv::State state) {
    std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
    data.resize(11);
    data[6] = improv::IMPROV_SERIAL_VERSION;
    data[7] = improv::TYPE_CURRENT_STATE;
    data[8] = 1;
    data[9] = state;

    uint8_t checksum = 0x00;
    for (uint8_t d : data)
        checksum += d;
    data[10] = checksum;

    Serial.write(data.data(), data.size());
}

void send_response(std::vector<uint8_t>& response) {
    std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
    data.resize(9);
    data[6] = improv::IMPROV_SERIAL_VERSION;
    data[7] = improv::TYPE_RPC_RESPONSE;
    data[8] = response.size();
    data.insert(data.end(), response.begin(), response.end());

    uint8_t checksum = 0x00;
    for (uint8_t d : data)
        checksum += d;
    data.push_back(checksum);

    Serial.write(data.data(), data.size());
}

void set_error(improv::Error error) {
    std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
    data.resize(11);
    data[6] = improv::IMPROV_SERIAL_VERSION;
    data[7] = improv::TYPE_ERROR_STATE;
    data[8] = 1;
    data[9] = error;

    uint8_t checksum = 0x00;
    for (uint8_t d : data)
        checksum += d;
    data[10] = checksum;

    Serial.write(data.data(), data.size());
}

void onErrorCallback(improv::Error err) {
    // A garbled packet is reported back so the installer can resend it.
    set_error(err);
}

// Scans without leaving the setup network, so a phone joined to it stays connected. Hidden networks
// are left out.
void getAvailableWifiNetworks() {
    int networkNum = WiFi.scanNetworks();

    for (int id = 0; id < networkNum; ++id) {
        if (WiFi.SSID(id).isEmpty()) {
            continue;
        }
        std::vector<uint8_t> data = improv::build_rpc_response(
            improv::GET_WIFI_NETWORKS,
            {WiFi.SSID(id), String(WiFi.RSSI(id)),
             (WiFi.encryptionType(id) == WIFI_AUTH_OPEN ? "NO" : "YES")},
            false);
        send_response(data);
        delay(1);
    }
    // final response
    std::vector<uint8_t> data =
        improv::build_rpc_response(improv::GET_WIFI_NETWORKS, std::vector<std::string>{}, false);
    send_response(data);
}

std::vector<std::string> getLocalUrl() {
    return {// URL where user can finish onboarding or use device
            // Recommended to use website hosted by device
            String("http://" + ServerManager.myIP.toString()).c_str()};
}

bool onCommandCallback(improv::ImprovCommand cmd) {
    switch (cmd.command) {
        case improv::Command::GET_CURRENT_STATE: {
            if ((WiFi.status() == WL_CONNECTED && !ServerManager.isInAPMode)) {
                set_state(improv::State::STATE_PROVISIONED);
                std::vector<uint8_t> data =
                    improv::build_rpc_response(improv::GET_CURRENT_STATE, getLocalUrl(), false);
                send_response(data);
            } else {
                set_state(improv::State::STATE_AUTHORIZED);
            }

            break;
        }

        case improv::Command::WIFI_SETTINGS: {
            if (cmd.ssid.length() == 0) {
                set_error(improv::Error::ERROR_INVALID_RPC);
                break;
            }

            set_state(improv::STATE_PROVISIONING);

            SettingsManager.settings.ssid = cmd.ssid.c_str();
            SettingsManager.settings.wifi_password = cmd.password.c_str();
            SettingsManager.saveSettingsToFile();

            ServerManager.stop();

            ServerManager.setup();

            if (!ServerManager.isInAPMode) {
                set_state(improv::STATE_PROVISIONED);
                std::vector<uint8_t> data =
                    improv::build_rpc_response(improv::WIFI_SETTINGS, getLocalUrl(), false);
                send_response(data);
            } else {
                set_state(improv::STATE_STOPPED);
                set_error(improv::Error::ERROR_UNABLE_TO_CONNECT);
            }

            break;
        }

        case improv::Command::GET_DEVICE_INFO: {
            std::vector<std::string> infos = {
                // Firmware name
                "Diabetes Clock",
                // Firmware version
                VERSION,
                // Hardware chip/variant
                "Ulanzi TC001",
                // Device name
                std::string(SettingsManager.settings.hostname.c_str())};
            std::vector<uint8_t> data =
                improv::build_rpc_response(improv::GET_DEVICE_INFO, infos, false);
            send_response(data);
            break;
        }

        case improv::Command::GET_WIFI_NETWORKS: {
            getAvailableWifiNetworks();
            break;
        }

        default: {
            set_error(improv::ERROR_UNKNOWN_RPC);
            return false;
        }
    }

    return true;
}

// Reads every waiting byte, so the installer's Wi-Fi setup isn't slowed by the display loop. A packet
// ends with its checksum byte, at 9 + its data length; the next one starts again at position 0.
void checckForImprovWifiConnection() {
    while (Serial.available() > 0) {
        uint8_t b = Serial.read();
        bool lastByte = x_position >= 9 && x_position == 9 + x_buffer[8];

        if (parse_improv_serial_byte(x_position, b, x_buffer, onCommandCallback, onErrorCallback) &&
            !lastByte) {
            x_buffer[x_position++] = b;
        } else if (!lastByte && x_position > 0 && b == 'I') {
            // A broken packet followed by the start of a new one.
            x_buffer[0] = b;
            x_position = 1;
        } else {
            x_position = 0;
        }
    }
}
