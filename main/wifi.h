#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Connect to WiFi
void initWifi();

// Check if WiFi is connected or not
bool wifiConnected();

#ifdef __cplusplus
}
#endif