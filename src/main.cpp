#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include "OV7725Streamer.h"
#include "OV7725aiThinker.h"
#include "CRtspSession.h"

#define LED_BUILTIN 33

const char* ssid = "xxxx";
const char* password = "xxxx";

OV7725aiThinker cam;

WebServer server(80);
WiFiServer rtspServer(8554);
uint8_t newMACAddress[] = {0x30, 0xAE, 0xA4, 0x90, 0xDA, 0x21}; // 30-AE-A4-90-DA-20

void handle_jpg_stream(void) {
    Serial.println("handle_jpg_stream");
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    while (1) {
        cam.run();
        if (!client.connected())
            break;
        response = "--frame\r\n";
        response += "Content-Type: image/jpeg\r\n\r\n";
        server.sendContent(response);

        client.write((char *)cam.getfb(), cam.getSize());
        server.sendContent("\r\n");
        if (!client.connected())
            break;
    }
}

void handle_jpg(void) {
    Serial.println("handle_jpg_stream");
    WiFiClient client = server.client();

    cam.run();
    if (!client.connected()) {
        return;
    }
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-disposition: inline; filename=capture.jpg\r\n";
    response += "Content-type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    client.write((char *)cam.getfb(), cam.getSize());
}

void handleNotFound() {
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    server.send(200, "text/plain", message);
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {

        case SYSTEM_EVENT_STA_START:
            WiFi.setHostname("cam02");
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("STA connected");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.println("STA got IP");
            Serial.print("STA SSID: ");
            Serial.println(WiFi.SSID());
            Serial.print("STA IPv4: ");
            Serial.println(WiFi.localIP());
            Serial.print("MAC address: ");
            Serial.println(WiFi.macAddress());
            ledcSetup(0, 200, 8);
            ledcWrite(0, 245);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.print("STA Disconnected. ");
            Serial.println(info.disconnected.reason);
            WiFi.persistent(false);
            WiFi.disconnect(true);
            delay(500);
            ESP.restart();
            break;
        default:
            break;
    }
}

void connectWiFi() {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

    WiFi.onEvent(WiFiEvent);
    esp_wifi_set_mac(ESP_IF_WIFI_STA, &newMACAddress[0]);
    WiFi.begin(ssid, password);
    Serial.println(F("WiFi connexion ongoing"));
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    int camInit = cam.init(esp32cam_aithinker_config);
    Serial.printf("Camera init returned %d\n", camInit);

    connectWiFi();

    server.on("/", HTTP_GET, handle_jpg_stream);
    server.on("/jpg", HTTP_GET, handle_jpg);
    server.onNotFound(handleNotFound);
    server.begin();

    rtspServer.begin();

    pinMode(LED_BUILTIN, OUTPUT);
    // digitalWrite(LED_BUILTIN, HIGH); // led off

    ledcSetup(0, 10, 8);
    ledcAttachPin(LED_BUILTIN, 0);
    ledcWrite(0, 245);

    // ledc_timer_config_t ledc_timer;
    // ledc_channel_config_t ledc_channel;

    // ledc_timer.speed_mode   = LEDC_LOW_SPEED_MODE;
    // ledc_timer.timer_num    = LEDC_TIMER_0;
    // ledc_timer.duty_resolution = LEDC_TIMER_8_BIT;
    // ledc_timer.freq_hz      = 100;
    
    // ledc_channel.channel    = LEDC_CHANNEL_0;
    // ledc_channel.gpio_num   = LED_BUILTIN;
    // ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    // ledc_channel.timer_sel  = LEDC_TIMER_0;
    // ledc_channel.duty       = 100;
    
    // ledc_timer_config(&ledc_timer);
    // ledc_channel_config(&ledc_channel);
}

CStreamer *streamer;
CRtspSession *session;
WiFiClient client; // FIXME, support multiple clients

void loop() {

    server.handleClient();

    uint32_t msecPerFrame = 100;
    static uint32_t lastimage = millis();

    // If we have an active client connection, just service that until gone
    // (FIXME - support multiple simultaneous clients)
    if(session) {
        session->handleRequests(0); // we don't use a timeout here,
        // instead we send only if we have new enough frames

        uint32_t now = millis();
        if(now > lastimage + msecPerFrame || now < lastimage) { // handle clock rollover
            session->broadcastCurrentFrame(now);
            lastimage = now;

            // check if we are overrunning our max frame rate
            now = millis();
            printf("time between frame %d ms\n", now - lastimage);
        }

        if(session->m_stopped) {
            delete session;
            delete streamer;
            session = NULL;
            streamer = NULL;
        }
    } else {
        client = rtspServer.accept();

        if(client) {
            streamer = new OV7725Streamer(&client, cam);             // our streamer for UDP/TCP based RTP transport
            session = new CRtspSession(&client, streamer); // our threads RTSP session and state
        }
    }
}
