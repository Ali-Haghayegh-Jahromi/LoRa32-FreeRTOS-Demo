#include                      <WiFi.h>
#include                      <WebServer.h>
#include                      <Adafruit_GFX.h>
#include                      <Adafruit_SSD1306.h>
#include                      <LoRa.h>
#include                      <SPI.h>

// ===== WiFi & HTTP Server Settings =========================================================================================================================================

const char                    *ssid             = "TELUSBM9206";
const char                    *password         = "h5tvH3RXRkxK";
bool                          flagSendMessage   = false;

WebServer                     server(80);
IPAddress                     localIPAddress;

// ===== LoRa Configuration ==================================================================================================================================================

#define                       LORA_FREQUENCY    915E6                                     // Adjust frequency as per your region (915E6 for US, 868E6 for Europe)
#define                       LORA_SCK          5
#define                       LORA_MISO         19
#define                       LORA_MOSI         27
#define                       LORA_SS           18
#define                       LORA_RST          14
#define                       LORA_DIO0         26

int                           loraResetNum      = 0;
// ===== OLED Display Configuration ==========================================================================================================================================

#define                       SCREEN_WIDTH      128                                               // OLED display width, in pixels
#define                       SCREEN_HEIGHT     64                                                // OLED display height, in pixels
#define                       OLED_RESET        -1                                                // Reset pin (or -1 if sharing Arduino reset)
#define                       OLED_I2C_ADDRESS  0x3C                                              // Typical I2C address for SSD1306

Adafruit_SSD1306              display           (SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// ===== Global Variables ====================================================================================================================================================

volatile unsigned long long   uptimeSeconds     = 0;                                              // Uptime counter in seconds
String                        loraRxTerminal    = "";                                             // String holding recent LoRa received messages
String                        loraSendMsg       = "";
SemaphoreHandle_t             loraRxMutex;                                                        // Mutex protecting loraRxTerminal
SemaphoreHandle_t             spiMutex;                                                           // Mutex to protect SPI access

// ===== Function Prototypes =================================================================================================================================================

void                          setupWiFi         (void);
void                          setupHTTPServer   (void);
void                          handleRoot        (void);

// ===== LoRa Check & Reset Prototypes =======================================================================================================================================

uint8_t                       readLoRaRegister  (uint8_t addr);
void                          loraHardwareReset (void);
void                          loraCheckTask     (void *pvParameters);

// ===== FreeRTOS Task Functions =============================================================================================================================================

// Task 1: Update OLED with uptime and latest LoRa RX messages.
void displayTask(void *pvParameters) 
{
  // Initialize the OLED display.
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) 
  {
    Serial.println(F("SSD1306 allocation failed"));
    while(true)
      vTaskDelay(100);
  }

  display.clearDisplay();
  display.display();

  delay(500);  // Allow the display to settle.

  while(true) 
  {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize (1);
    display.setCursor   (0, 0);

    // Display uptime.
    char tmpBuf[21];

    // 1) Break total seconds into D:H:M:S
    unsigned long long  total    = uptimeSeconds;
    
    uint32_t            days     = total / 86400;      // 86400 s in a day
    total                        = total % 86400;
    uint8_t             hours    = total / 3600;       // 3600 s in an hour
    total                        = total % 3600;
    uint8_t             minutes  = total / 60;         // 60 s in a minute
    uint8_t             seconds  = total % 60;

    snprintf(tmpBuf, sizeof(tmpBuf), "%02uD, %02u:%02u:%02u",
            days, hours, minutes, seconds);

    // Display Uptime.
    display.print   ("Uptime: ");
    display.println (tmpBuf);
    display.println ("---------------------");

    // Display IP Address.
    display.print   ("IP ===> ");
    display.println (localIPAddress);
    display.println ("---------------------");
    display.println ("");

    // Display last LoRa RX info (limit to what fits).
    display.print   ("LoRa RX:");

    if(xSemaphoreTake(loraRxMutex, (TickType_t)10) == pdTRUE) 
    {
      int y         = display.getCursorY();
      int line      = 0;
      int start     = 0;
      int maxLines  = (SCREEN_HEIGHT - y) / 8;  // assuming ~8 pixels per line.

      while(line < maxLines && start >= 0) 
      {
        int end = loraRxTerminal.indexOf('\n', start);
        String lineStr;
        if(end == -1)
        {
          lineStr = loraRxTerminal.substring(start);
          start = -1;
        } 
        else 
        {
          lineStr = loraRxTerminal.substring(start, end);
          start = end + 1;
        }

        display.println(lineStr);

        line++;
      }
      xSemaphoreGive(loraRxMutex);
    }

    display.display();

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

//****************************************************************************************************************************************************************************
// Task 2: LoRa send task – transmits the defined message every 10 seconds.
void loraSendTask(void *pvParameters) 
{
  while (true) 
  {
    if(xSemaphoreTake(spiMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) 
    {
      Serial.print(F("Sending LoRa message: "));
      Serial.println(loraSendMsg);

      LoRa.beginPacket();
      LoRa.print      (loraSendMsg);
      LoRa.endPacket  ();

      flagSendMessage = false;

      xSemaphoreGive(spiMutex);
    } 
    else 
    {
      Serial.println(F("SPI timeout in send task, performing hardware reset."));

      loraHardwareReset();
      loraResetNum++;

      if(!LoRa.begin(LORA_FREQUENCY)) 
        Serial.println(F("LoRa re-init failed after reset in send task."));
      else 
        Serial.println(F("LoRa re-init succeeded in send task."));
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

//****************************************************************************************************************************************************************************
// Task 3: LoRa receive task – continuously checks for incoming LoRa packets.
void loraReceiveTask(void *pvParameters) 
{
  while(true) 
  {
    if(xSemaphoreTake(spiMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) 
    {
      int packetSize = LoRa.parsePacket();
      xSemaphoreGive(spiMutex);
      if(packetSize) 
      {
        String rxMsg = "";
        
        if(xSemaphoreTake(spiMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) 
        {
          while(LoRa.available()) 
            rxMsg += (char)LoRa.read();

          xSemaphoreGive(spiMutex);
        } 
        else 
        {
          Serial.println(F("SPI timeout in receive task while reading, performing hardware reset."));

          loraResetNum++;
          loraHardwareReset();

          if(!LoRa.begin(LORA_FREQUENCY)) 
            Serial.println(F("LoRa re-init failed after reset in receive task."));
          else 
            Serial.println(F("LoRa re-init succeeded in receive task."));

        }
        // Append received message with mutex protection.
        if(xSemaphoreTake(loraRxMutex, (TickType_t)10) == pdTRUE) 
        {
          loraRxTerminal = rxMsg + "\n";

          const int maxSize = 512;

          if(loraRxTerminal.length() > maxSize) 
            loraRxTerminal = loraRxTerminal.substring(loraRxTerminal.length() - maxSize);

          xSemaphoreGive(loraRxMutex);
        }

        Serial.print(F("Received LoRa packet: "));
        Serial.println(rxMsg);
      }
    }
    else 
    {
      Serial.println(F("SPI timeout in parsePacket, performing hardware reset."));

      loraResetNum++;
      loraHardwareReset();

      if(!LoRa.begin(LORA_FREQUENCY)) 
        Serial.println(F("LoRa re-init failed after reset in parsePacket."));
      else 
        Serial.println(F("LoRa re-init succeeded in parsePacket."));
    }

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

//****************************************************************************************************************************************************************************
// Task 4: HTTP server task – processes incoming client requests.
void httpServerTask(void *pvParameters) 
{
  while(true) 
  {
    server.handleClient();

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

//****************************************************************************************************************************************************************************
// Task 5: Uptime counter – increments uptimeSeconds every second.
void uptimeTask(void *pvParameters) 
{
  while(true) 
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if(!flagSendMessage)
      loraSendMsg = "Hello from ESP32 LoRa32: " + String(++uptimeSeconds);
  }
}

//****************************************************************************************************************************************************************************
// Task 6: LoRa Check Task – periodically verifies LoRa responsiveness via a known register.
void loraCheckTask(void *pvParameters) 
{
  const uint8_t expectedVersion = 0x12;  // Typical expected version for SX1276.

  while(true) 
  {
    uint8_t version = readLoRaRegister(0x42);

    if(version != expectedVersion) 
    {
      Serial.print  (F("LoRa check failed! Read version: 0x"));
      Serial.print  (version, HEX);
      Serial.println(F(" - performing hardware reset..."));

      loraHardwareReset();
      loraResetNum++;

      if(!LoRa.begin(LORA_FREQUENCY))
        Serial.println(F("LoRa re-init failed after reset in check task."));
      else 
        Serial.println(F("LoRa re-init succeeded in check task."));
    }
    // Check every 30 seconds (adjust if needed).
    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

// ===== Hardware Reset and Register Access =====
// Read a register from the LoRa module over SPI (with timeout protection).
uint8_t readLoRaRegister(uint8_t addr) 
{
  uint8_t value = 0xFF;

  if(xSemaphoreTake(spiMutex, 550 / portTICK_PERIOD_MS) == pdTRUE) 
  {
    digitalWrite(LORA_SS, LOW);

    SPI.transfer(addr & 0x7F);  // Clear MSB for reading.
    value = SPI.transfer(0x00);

    digitalWrite(LORA_SS, HIGH);

    xSemaphoreGive(spiMutex);

    Serial.print(F("LoRa OK: "));
    Serial.println(loraResetNum);
  }
  else 
  {
    Serial.println(F("SPI timeout in readLoRaRegister, performing hardware reset."));
    loraResetNum++;
    loraHardwareReset();

    if(!LoRa.begin(LORA_FREQUENCY)) 
      Serial.println(F("LoRa re-init failed after reset in readLoRaRegister."));
    else 
      Serial.println(F("LoRa re-init succeeded in readLoRaRegister."));
  }

  return value;
}

// Toggle the LoRa reset pin to perform a hardware reset.
void loraHardwareReset() 
{
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW);
  delay(200);  // Extended low time.
  digitalWrite(LORA_RST, HIGH);
  delay(200);  // Wait for the module to boot.
}

void setup() 
{
  Serial.begin(115200);
  Serial.println(F("Starting TTGO LoRa32 T3 multi-task demo..."));

  // Initialize LoRa module pins.
  LoRa.setPins  (LORA_SS,   LORA_RST,   LORA_DIO0);
  SPI.begin     (LORA_SCK,  LORA_MISO,  LORA_MOSI,  LORA_SS);

  if(!LoRa.begin(LORA_FREQUENCY)) 
  {
    Serial.println(F("LoRa init failed. Check wiring."));

    while(true);
      delay(1000);
  }
  Serial.println(F("LoRa init succeeded."));

  // Set up WiFi connection.
  setupWiFi();
  // Set up the HTTP server.
  setupHTTPServer();

  // Create a mutex for protecting the LoRa RX terminal string.
  loraRxMutex = xSemaphoreCreateMutex();
  // Create a mutex to protect SPI transactions.
  spiMutex    = xSemaphoreCreateMutex();

  // Create FreeRTOS tasks on the available cores.
  xTaskCreatePinnedToCore(loraSendTask,     "LoRaSendTask",     4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(loraReceiveTask,  "LoRaReceiveTask",  4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(uptimeTask,       "UptimeTask",       2048, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(httpServerTask,   "HTTPServerTask",   4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(loraCheckTask,    "LoRaCheckTask",    2048, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(displayTask,      "DisplayTask",      4096, NULL, 3, NULL, 1);

  vTaskDelete(NULL);
}

void loop() 
{
  // Empty: All functionality runs in FreeRTOS tasks.
}

// ===== WiFi Setup =====
void setupWiFi() 
{
  int retry = 0;
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED && retry < 20)
  {
    delay(500);
    Serial.print(F("."));
    retry++;
  }

  if(WiFi.status() == WL_CONNECTED) 
  {
    localIPAddress = WiFi.localIP();

    Serial.println(F("\nWiFi connected."));
    Serial.print  (F("IP Address: "));
    Serial.println(localIPAddress);
  } 
  else 
  {
    Serial.println(F("\nWiFi connection failed."));
  }
}

// ===== HTTP Server Setup & Handlers =====
void setupHTTPServer() 
{
  server.on("/", handleRoot);
  server.on("/setMsg", HTTP_POST, []()
  {
    if(server.hasArg("msg"))
    {
      flagSendMessage = true;
      loraSendMsg = server.arg("msg");
    }

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.begin();
  Serial.println(F("HTTP Server started."));
}

void handleRoot() 
{
  String html =   "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>LoRa32 Info</title></head><body>";
  html        +=  "<h1>TTGO LoRa32 T3 Demo</h1>";
  html        +=  "<p>Uptime: "           + String(uptimeSeconds) + " seconds.</p>";
  html        +=  "<p>Message to send: "  + loraSendMsg           + "</p>";
  html        +=  "<h2>LoRa Receive Terminal</h2>";

  if(xSemaphoreTake(loraRxMutex, (TickType_t)10) == pdTRUE)
  {
    html      +=  "<pre>" + loraRxTerminal + "</pre>";
    xSemaphoreGive(loraRxMutex);
  }

  html        +=  "<h2>Update Send Message</h2>";
  html        +=  "<form method='POST' action='/setMsg'>";
  html        +=  "New Message: <input type='text' name='msg'><input type='submit' value='Update'>";
  html        +=  "</form>";
  html        +=  "</body></html>";

  server.send(200, "text/html", html);
}
