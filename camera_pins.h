#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32 //power pin
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0 //XCLK
#define SIOD_GPIO_NUM     26 //SDA
#define SIOC_GPIO_NUM     27 //SCL
#define Y9_GPIO_NUM       35 //D7
#define Y8_GPIO_NUM       34 //D6
#define Y7_GPIO_NUM       39 //D5
#define Y6_GPIO_NUM       36 //D4
#define Y5_GPIO_NUM       21 //D3
#define Y4_GPIO_NUM       19 //D2
#define Y3_GPIO_NUM       18 //D1
#define Y2_GPIO_NUM        5 //D0
#define VSYNC_GPIO_NUM    25 //VSYNC
#define HREF_GPIO_NUM     23 //HREF
#define PCLK_GPIO_NUM     22 //PCLK

// 4 for flash led or 33 for normal led
#define LED_GPIO_NUM       4 //DATA 1/FLASH LAMP

#else
#error "Camera model not selected"
#endif

//CLK = PIN 14
//CMD = PIN 15
//DATA0 = PIN 2
//DATA2 = PIN 12
//DATA3 = PIN 13