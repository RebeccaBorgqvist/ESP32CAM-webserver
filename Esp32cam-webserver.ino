#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncEventSource.h> 
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include <EEPROM.h>
#include "time.h"

#include <PubSubClient.h>
#include <base64.h>
#include <libb64/cencode.h>

#define CAMERA_MODEL_AI_THINKER  // Has PSRAM
#define EEPROM_SIZE 1
#define LED_PIN 4

#include "wifi_credentials.h"
#include "mqtt_credentials.h"
#include "camera_pins.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);  // HTTP server on port 80

RTC_DATA_ATTR int bootCount = 0;
int pictureNumber = 0;
camera_fb_t* fb = NULL;  // Frame buffer for camera image

void setupCamera();
void connectToWiFi();
void connectToMQTT();
void captureImage();
void serveImage();
void incrementPictureNumber();
void setupServer();


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(LED_PIN, OUTPUT);
  setupCamera();
  connectToWiFi();
  connectToMQTT();
  incrementPictureNumber();
  setupServer();
}

void loop() {
  // Main loop will handle server and image capture
  if (millis() % 10000 == 0) {  // Every 10 seconds
    captureImage();
  }
}

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());  // Print the local IP address
}


void connectToMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  while (!mqttClient.connect("ESP32CAM", MQTT_USER, MQTT_PASSWORD)) {
    Serial.print("Connection to MQTT failed, return code: ");
    Serial.println(mqttClient.state());
    delay(5000);
  }
  Serial.println("Connected to MQTT");
}

void incrementPictureNumber() {
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1;
  EEPROM.write(0, pictureNumber);
  EEPROM.commit();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32-CAM Image</title>
  <script>
    function initEventSource() {
      var source = new EventSource('/events');
      source.addEventListener('image_update', function(e) {
        refreshImage();
      }, false);
    }
    
    function refreshImage() {
      var img = document.getElementById('cam-image');
      img.src = '/capture?' + new Date().getTime();
    }

    document.addEventListener('DOMContentLoaded', initEventSource);
  </script>
</head>
<body>
  <h1>ESP32-CAM Image Stream</h1>
  <img id="cam-image" src="/capture" style="width:600px;">
</body>
</html>
)rawliteral";


AsyncEventSource events("/events");

void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest* request) {
    serveImage(request);
  });

    server.addHandler(&events);

    server.begin();
    Serial.println("HTTP server started");
}

void serveImage(AsyncWebServerRequest *request) {
  if (fb) {
    request->send_P(200, "image/jpeg", fb->buf, fb->len);
  } else {
    request->send(404, "text/plain", "Image not available");
  }
}

void captureImage() {
  // digitalWrite(LED_PIN, HIGH);
  // delay(100);
  // digitalWrite(LED_PIN, LOW);
  if (fb) {
    esp_camera_fb_return(fb);
  }
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
  }
  events.send("new_image", "image_update", millis());
}

void publishImageUrl() {
  String imageUrl = "http://";
  imageUrl += WiFi.localIP().toString();
  imageUrl += "/capture";

  mqttClient.publish("ESP32CAM/URL", imageUrl.c_str());
}

void takePictureAndSleep() {
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);

  camera_fb_t* fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  const char* imageBuffer = reinterpret_cast<const char*>(fb->buf);

  if (mqttClient.publish("ESP32CAM/camera", imageBuffer, fb->len)) {
    Serial.println("Image published successfully");
    mqttClient.publish("ESP32CAM/status", "Picture sent");
    delay(1000);  // Delay to ensure message is sent
  } else {
    Serial.println("Fail to publish image");
  }


  esp_camera_fb_return(fb);

  esp_sleep_enable_timer_wakeup(60000000);
  esp_deep_sleep_start();
}
