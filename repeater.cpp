#if LWIP_FEATURES && !LWIP_IPV6

#define HAVE_NETDUMP 0

#include <ESP8266WiFi.h>
#include "LittleFS.h"
#include <EEPROM.h>
#include "wifi.h"
#include "settings.h"
#include <lwip/napt.h>
#include <lwip/dns.h>
#include <dhcpserver.h>

#define NAPT 1000
#define NAPT_PORT 10
typedef struct ap_settings_t {
    char    path[33];
    char    ssid[33];
    char    password[65];
    uint8_t channel;
    bool    hidden;
    bool    captive_portal;
} ap_settings_t;
#if HAVE_NETDUMP

#include <NetDump.h>



void dump(int netif_idx, const char* data, size_t len, int out, int success) {
  (void)success;
  Serial.print(out ? F("out ") : F(" in "));
  Serial.printf("%d ", netif_idx);

  // optional filter example: if (netDump_is_ARP(data))
  {
    netDump(Serial, data, len);
    //netDumpHex(Serial, data, len);
  }
}
#endif

ap_settings_t ap_settings;
bool state = false;
String wifi_ssid,wifi_pass;
void w(const char *path,String message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = LittleFS.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
  file.close();
}

String r(const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = LittleFS.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

void de(const char *path){
  LittleFS.remove(path);
}

namespace repeater {
      void connect(String ssid,String pass){
      w("/ssid.txt",ssid);
      w("/pass.txt",pass);
      WiFi.mode(WIFI_AP_STA);
      WiFi.begin(ssid.c_str(),pass.c_str());
      Serial.println("saving repeater config..");
        }
      void run() {
      
            #if HAVE_NETDUMP
              phy_capture = dump;
            #endif
        wifi::stopAP();
        WiFi.mode(WIFI_STA);
        wifi_ssid = r("/ssid.txt");
        wifi_pass = r("/pass.txt");
        de("/ssid.txt");
        de("/pass.txt");
        EEPROM.write(400,0);
        EEPROM.commit();
        if(wifi_pass.length() < 8)WiFi.begin(wifi_ssid.c_str());
        else WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        while (WiFi.status() != WL_CONNECTED) {
          Serial.print('.');
          delay(500);
        }
        
        Serial.printf("\nSTA: %s (dns: %s / %s)\n",
                      WiFi.localIP().toString().c_str(),
                      WiFi.dnsIP(0).toString().c_str(),
                      WiFi.dnsIP(1).toString().c_str());
      
        // give DNS servers to AP side
        dhcps_set_dns(0, WiFi.dnsIP(0));
        dhcps_set_dns(1, WiFi.dnsIP(1));
      
        WiFi.softAPConfig(  // enable AP, with android-compatible google domain
          IPAddress(172, 217, 28, 254),
          IPAddress(172, 217, 28, 254),
          IPAddress(255, 255, 255, 0));
          WiFi.softAP(ap_settings.ssid, ap_settings.password);
        Serial.printf("AP: %s\n", WiFi.softAPIP().toString().c_str());
      
        Serial.printf("Heap before: %d\n", ESP.getFreeHeap());
        err_t ret = ip_napt_init(NAPT, NAPT_PORT);
        Serial.printf("ip_napt_init(%d,%d): ret=%d (OK=%d)\n", NAPT, NAPT_PORT, (int)ret, (int)ERR_OK);
        if (ret == ERR_OK) {
          ret = ip_napt_enable_no(SOFTAP_IF, 1);
          Serial.printf("ip_napt_enable_no(SOFTAP_IF): ret=%d (OK=%d)\n", (int)ret, (int)ERR_OK);
          if (ret == ERR_OK) {
           Serial.println("Success..");
          }
        }
        Serial.printf("Heap after napt init: %d\n", ESP.getFreeHeap());
        if (ret != ERR_OK) {
          Serial.printf("NAPT initialization failed\n");
        }
      }
      bool status(){
        return state;
      }
      void update_status(bool s){
        state = s;
     }
}


#endif
