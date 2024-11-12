#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ConfigPortal32.h>
#include <DHT.h>

#define DHTPIN 15  
#define DHTTYPE DHT22  

DHT dht(DHTPIN, DHTTYPE);

char* ssid_pfix = (char*)"iPhone (76)";
String user_config_html = ""
    "<p><input type='text' name='meta.influxdb_url' placeholder='InfluxDB Address'>"
    "<p><input type='text' name='meta.influxdb_token' placeholder='InfluxDB Token'>"
    "<p><input type='text' name='meta.influxdb_bucket' placeholder='InfluxDB Bucket'>"
    "<p><input type='text' name='meta.report_interval' placeholder='Report Interval (ms)'>";

char influxdb_url[100];
char influxdb_token[100];
char influxdb_bucket[50];
char influxdb_org[50] = "iotlab"; 
unsigned long report_interval = 10000; 
unsigned long last_report_time = 0;

void connectWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(115200);
    dht.begin();

    loadConfig();

 
    if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
        configDevice();
    }

    connectWiFi(); 


    if (cfg.containsKey("meta")) {
        strcpy(influxdb_url, (const char*)cfg["meta"]["influxdb_url"]);
        strcpy(influxdb_token, (const char*)cfg["meta"]["influxdb_token"]);
        strcpy(influxdb_bucket, (const char*)cfg["meta"]["influxdb_bucket"]);

        if (cfg["meta"].containsKey("report_interval")) {
            report_interval = atoi((const char*)cfg["meta"]["report_interval"]);
        }
    }

    Serial.println("Configuration loaded:");
    Serial.print("InfluxDB URL: ");
    Serial.println(influxdb_url);
    Serial.print("Token: ");
    Serial.println(influxdb_token);
    Serial.print("Bucket: ");
    Serial.println(influxdb_bucket);
    Serial.print("Organization: ");
    Serial.println(influxdb_org);
    Serial.print("Report Interval: ");
    Serial.println(report_interval);
}

void loop() {
    unsigned long currentMillis = millis();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Attempting to reconnect...");
        WiFi.reconnect();  
        delay(100);  
        if (WiFi.status() != WL_CONNECTED) {
            return; 
        }
        Serial.println("Reconnected to WiFi");
    }

    if (currentMillis - last_report_time >= report_interval) {
        last_report_time = currentMillis; 

        float h = dht.readHumidity();
        float t = dht.readTemperature();

        if (isnan(h) || isnan(t)) {
            Serial.println("Failed to read from DHT sensor!");
            return;
        }

        Serial.printf("Temperature: %.2f, Humidity: %.2f\n", t, h);

        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            String influx_data = "temperature,device=esp32 value=" + String(t) + "\n" +
                                 "humidity,device=esp32 value=" + String(h);

            String url = String(influxdb_url) + "/api/v2/write?bucket=" + influxdb_bucket + "&org=" + influxdb_org + "&precision=s";

            Serial.print("Attempting to connect to InfluxDB at: ");
            Serial.println(url);
            Serial.println("Sending data to InfluxDB:");
            Serial.println(influx_data);

            http.begin(url.c_str());
            http.addHeader("Content-Type", "text/plain");
            http.addHeader("Authorization", String("Token ") + influxdb_token);
            http.setTimeout(3000); 


            int httpResponseCode = http.POST(influx_data);

            if (httpResponseCode > 0) {
                Serial.printf("Data sent to InfluxDB - HTTP response code: %d\n", httpResponseCode);
            } else {
                Serial.printf("Failed to send data to InfluxDB - HTTP response code: %d\n", httpResponseCode);
                Serial.println("Possible issues: Check WiFi connection, InfluxDB URL, Token, or Network.");
            }

            http.end();
        } else {
            Serial.println("WiFi not connected. Skipping data upload.");
        }
    }
}
