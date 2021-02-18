// Set debug
#define DEBUG true

#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>

// pins
#define PIN12 12
#define PIN13 13
#define FLASH_LED_PIN 4

// Initialize Telegram BOT
const String BOTtoken = "1506076448:AAGX91T49oMHYEnEXsWVY6D07Jem5hYenuk";  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
const String CHAT_ID = "998429121";

bool flashState = LOW;
bool sendPhoto = false;

WiFiClientSecure client;
WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, client);


// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

bool outState12 = LOW;
bool outState13 = LOW;

#include "camera_pins.h"

const char* ssid = "aburumaisha";
const char* password = "thxtoalloh";

void startCameraServer();



// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {

  if (DEBUG) {
    Serial.println("handleNewMessages");
    Serial.println(String(numNewMessages));
  }

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;

    if(DEBUG) {
      Serial.println(text);
    }

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands to control your outputs.\n\n";
      welcome += "/on1 to turn GPIO 0 ON \n";
      welcome += "/off1 to turn GPIO 0 OFF \n";
      welcome += "/on2 to turn GPIO 2 ON \n";
      welcome += "/off2 to turn GPIO 2 OFF \n";
      welcome += "/state to request current GPIO state \n";
      welcome += "/flash to turn FLASH ON, click again to shutdown \n";
      welcome += "/photo to request upload photo to telegram";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      bot.sendMessage(chat_id, "Change flash LED state to "+String(flashState));
    }

    if (text == "/photo") {
      sendPhoto = true;
    }

    if(text.equals("/on1")) {
      bot.sendMessage(chat_id, "PIN 0 state set to ON", "");
      digitalWrite(PIN12, HIGH);
      outState12 = true;
    }

    if(text.equals("/off1")){
      bot.sendMessage(chat_id, "PIN 0 state set to OFF", "");
      digitalWrite(PIN12, LOW);
      outState12 = false;
    }

    if (text.equals("/on2")) {
      bot.sendMessage(chat_id, "PIN 2  set to ON", "");
      digitalWrite(PIN13, HIGH);
      outState13 = true;
    }
    
    if (text.equals("/off2")) {
      bot.sendMessage(chat_id, "PIN 2 set to OFF", "");
      digitalWrite(PIN13, LOW);
      outState13 = false;
    }
    
    if (text.equals("/state")) {
      String message = "";
      message += "PIN 12 is "+String(outState12)+" \n";
      message += "PIN 13 is "+String(outState13)+" \n";
      bot.sendMessage(chat_id, message, "");
    }
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);
  
  #if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
  #endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

  #if defined(CAMERA_MODEL_M5STACK_WIDE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  #endif

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  
  
  Serial.println("Connect to " + String(myDomain));


  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    String head = "--Fajarlabs\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID;
    head += "\r\n--Fajarlabs\r\nContent-Disposition: form-data; name=\"caption\"; \r\n\r\nANDI";
    head += "\r\n--Fajarlabs\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Fajarlabs--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=Fajarlabs");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 60000;   // timeout 60 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

void loop() {
  if (sendPhoto) {
    Serial.println("Preparing photo");
    sendPhotoTelegram(); 
    sendPhoto = false; 
  }
  
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      if(DEBUG){
        Serial.println("got response");
      }
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}
