#pragma once

#include <Arduino.h>

namespace repeater {
    void connect(String ssid,String pass);
    void run();
    bool status();
    void update_status(bool s);
}
