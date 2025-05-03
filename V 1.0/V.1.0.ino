#include                    <FS.h>
#include                    <SD.h>
#include                    <SPI.h>
#include                    <WiFi.h>
#include                    <LoRa.h>
#include                    <WebServer.h>
#include                    <Adafruit_GFX.h>
#include                    <Adafruit_SSD1306.h>

// ===== WiFi & HTTP Server Settings ===================================================================
const char                  *ssid               = "ssid";
const char                  *password           = "password";
WebServer                   server              (80);
IPAddress                   localIPAddress;

// ===== LoRa Configuration ============================================================================
#define                     LORA_FREQUENCY      915E6    // 915 MHz for North America
#define                     LORA_SCK            5
#define                     LORA_MISO           19
#define                     LORA_MOSI           27
#define                     LORA_SS             18
#define                     LORA_RST            23
#define                     LORA_DIO0           26

SPIClass                    LoRaSPI             (VSPI);

// ===== SD-Card Configuration (T3 V1.6.1) ===============================================================
#define                     SDCARD_CS           13
#define                     SDCARD_MOSI         15
#define                     SDCARD_SCLK         14
#define                     SDCARD_MISO         2

SPIClass                    SDSPI               (HSPI);

// ===== OLED Display Configuration ======================================================================
#define                     SCREEN_WIDTH        128
#define                     SCREEN_HEIGHT       64
#define                     OLED_RESET          -1
#define                     OLED_I2C_ADDRESS    0x3C

Adafruit_SSD1306            display             (SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== FreeRTOS IPC Primitives =========================================================================
#define                     MAX_RX_LEN          128

static QueueHandle_t        xLoraQueue          = nullptr;
static SemaphoreHandle_t    loraRxMutex         = nullptr;
static SemaphoreHandle_t    spiMutex            = nullptr;

// ===== Global Variables ================================================================================
volatile unsigned long long uptimeSeconds       = 0;
String                      loraRxTerminal      = "";
String                      loraSendMsg         = "";

// ===== WiFi & HTTP Helpers ===============================================================================
void setupWiFi( void )
{
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);

  int retry = 0;

  while(WiFi.status() != WL_CONNECTED && retry++ < 20)
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(F("."));
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    localIPAddress = WiFi.localIP();
    Serial.println(F("\nWiFi connected"));
    Serial.print(F("IP: "));
    Serial.println(localIPAddress);
  }
  else
  {
    Serial.println(F("\nWiFi failed"));
  }
}

void setupHTTPServer( void )
{
  server.on("/", []() 
  {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>LoRa32 Info</title></head><body>";
    html += "<h1>TTGO LoRa32 T3 Demo</h1>";
    html += "<p>Uptime: " + String(uptimeSeconds) + "s</p>";
    html += "<p>Message to send: " + loraSendMsg + "</p>";
    html += "<h2>LoRa Receive Terminal</h2>";
    if(xSemaphoreTake(loraRxMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      html += "<pre>" + loraRxTerminal + "</pre>";
      xSemaphoreGive(loraRxMutex);
    }
    html += "<h2>Update Send Message</h2>";
    html += "<form method='POST' action='/setMsg'>";
    html += "New Message: <input type='text' name='msg'><input type='submit' value='Update'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/setMsg", HTTP_POST, []() 
  {
    if(server.hasArg("msg"))
    {
      loraSendMsg = server.arg("msg");
      xTaskNotifyGive( xTaskGetHandle("LoRaSendTask") );  // notify immediately
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.begin();

  Serial.println(F("HTTP Server started"));
}

// ===== Hardware Reset and Register Access ================================================================
uint8_t readLoRaRegister( uint8_t addr )
{
  uint8_t value = 0xFF;
  
  if(xSemaphoreTake(spiMutex, pdMS_TO_TICKS(550)) == pdTRUE)
  {
    // Begin a transaction on the VSPI bus
    LoRaSPI.beginTransaction(SPISettings(8'000'000, MSBFIRST, SPI_MODE0));

    digitalWrite(LORA_SS, LOW);
    LoRaSPI.transfer(addr & 0x7F);
    value = LoRaSPI.transfer(0x00);
    digitalWrite(LORA_SS, HIGH);

    LoRaSPI.endTransaction();


    xSemaphoreGive(spiMutex);
  }
  else
  {
    Serial.println(F("SPI timeout in readLoRaRegister, resetting"));
  
    loraHardwareReset();
  }
  return value;
}

void loraHardwareReset(void)
{
  digitalWrite(LORA_RST, LOW);
  vTaskDelay(pdMS_TO_TICKS(200));
  digitalWrite(LORA_RST, HIGH);
  vTaskDelay(pdMS_TO_TICKS(200));
}

bool loraInit() 
{
  if(xSemaphoreTake(spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) 
  {
    // configure the bus & pins
    LoRaSPI.begin (LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setSPI   (LoRaSPI);
    LoRa.setPins  (LORA_SS, LORA_RST, LORA_DIO0);

    // now actually initialize on the bus
    bool ok = LoRa.begin(LORA_FREQUENCY);
    xSemaphoreGive(spiMutex);

    if(!ok) 
    {
      Serial.println(F("LoRa re-init failed"));
      return false;
    }
    Serial.println(F("LoRa re-init succeeded"));
    return true;
  }
  Serial.println(F("Couldn’t lock SPI for LoRa.init"));
  return false;
}


// ===== SD Function =====================================================================================
void listDir( fs::FS &fs, const char *dirname, uint8_t levels )
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);

    if (!root) 
    {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) 
    {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) 
    {
        if (file.isDirectory()) 
        {
            Serial.print    ("  DIR : ");
            Serial.println  (file.name());

            if (levels) 
              listDir(fs, file.path(), levels - 1);
        } 
        else 
        {
            Serial.print    ("  FILE: ");
            Serial.print    (file.name());
            Serial.print    ("  SIZE: ");
            Serial.println  (file.size());
        }
        file = root.openNextFile();
    }
}

void createDir( fs::FS &fs, const char *path )
{
    Serial.printf("Creating Dir: %s\n", path);

    if (fs.mkdir(path)) 
    {
        Serial.println("Dir created");
    } 
    else 
    {
        Serial.println("mkdir failed");
    }
}

void removeDir( fs::FS &fs, const char *path )
{
    Serial.printf("Removing Dir: %s\n", path);

    if (fs.rmdir(path)) 
    {
        Serial.println("Dir removed");
    } 
    else 
    {
        Serial.println("rmdir failed");
    }
}

void readFile( fs::FS &fs, const char *path )
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);

    if (!file) 
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print(F("Read from file: "));

    while (file.available()) 
    {
        Serial.write(file.read());
    }
    file.close();
}

void writeFile( fs::FS &fs, const char *path, const char *message )
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file) 
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(message)) 
    {
        Serial.println("File written");
    } 
    else 
    {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile( fs::FS &fs, const char *path, const char *message )
{
    //Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);

    if (!file) 
    {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message)) 
    {
        //Serial.println("Message appended");
    } 
    else 
    {
        Serial.println("Append failed");
    }

    file.close();
}

void renameFile( fs::FS &fs, const char *path1, const char *path2 )
{
    Serial.printf("Renaming file %s to %s\n", path1, path2);

    if (fs.rename(path1, path2)) 
    {
        Serial.println("File renamed");
    } 
    else 
    {
        Serial.println("Rename failed");
    }
}

void deleteFile( fs::FS &fs, const char *path )
{
    Serial.printf("Deleting file: %s\n", path);

    if (fs.remove(path)) 
    {
        Serial.println("File deleted");
    } 
    else 
    {
        Serial.println("Delete failed");
    }
}

void testFileIO( fs::FS &fs, const char *path )
{
    static uint8_t  buf[512];
    File            file    = fs.open(path);
    size_t          len     = 0;
    uint32_t        start   = millis();
    uint32_t        end     = start;

    if (file) 
    {
        len = file.size();

        size_t flen = len;

        start = millis();

        while (len) 
        {
            size_t toRead = len;
            if (toRead > 512) 
            {
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;

        Serial.printf("%u bytes read for %u ms\n", flen, end);

        file.close();
    } 
    else 
    {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if (!file) 
    {
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;

    start = millis();

    for (i = 0; i < 2048; i++) 
    {
        file.write(buf, 512);
    }
    end = millis() - start;

    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);

    file.close();
}
// ===== FreeRTOS Task Functions =========================================================================

// Task 1: Update OLED with uptime and latest LoRa RX messages.
void displayTask( void *pvParameters )
{
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));

    for(;;) 
      vTaskDelay(100);
  }

  display.clearDisplay();
  display.display();

  vTaskDelay(pdMS_TO_TICKS(500));

  for(;;)
  {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize (1);
    display.setCursor   (0, 0);

    // Uptime
    char tmpBuf[21];
    unsigned long long total = uptimeSeconds;
    uint32_t days    = total / 86400;
    total            = total % 86400;
    uint8_t hours    = total / 3600;
    total            = total % 3600;
    uint8_t minutes  = total / 60;
    uint8_t seconds  = total % 60;
    snprintf(tmpBuf, sizeof(tmpBuf), "%02uD, %02u:%02u:%02u",
             days, hours, minutes, seconds);

    display.print   ("Uptime: ");
    display.println (tmpBuf);
    display.println ("---------------------");

    // IP
    display.print   ("IP ===> ");
    display.println (localIPAddress);
    display.println ("---------------------");
    display.println ();

    // LoRa RX
    display.print("LoRa RX:");
    if(xSemaphoreTake(loraRxMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      int y        = display.getCursorY();
      int line     = 0;
      int start    = 0;
      int maxLines = (SCREEN_HEIGHT - y) / 8;
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
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// Task 2: LoRa send task – transmits the defined message on demand and periodically.
void loraSendTask( void *pvParameters )
{
  const TickType_t  xDefaultPeriod = pdMS_TO_TICKS(1000);
  uint32_t          ulNotifiedValue;

  for(;;)
  {
    BaseType_t xResult = xTaskNotifyWait(
      0,              // clear none on entry
      ULONG_MAX,      // clear all on exit
      &ulNotifiedValue,
      xDefaultPeriod  // timeout = 1 s for default send
    );

    if(xSemaphoreTake(spiMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      if(xResult == pdTRUE)
      {
        Serial.print(F(">>> User message: "));
      }
      else
      {
        loraSendMsg = "Hello from ESP32 LoRa32: " + String(uptimeSeconds);

        //Serial.print(F(">>> Default message: "));
      }

      //Serial.println(loraSendMsg);

      LoRa.beginPacket();
      LoRa.print(loraSendMsg);
      LoRa.endPacket();

      xSemaphoreGive(spiMutex);
    }
    else
    {
      Serial.println(F("SPI timeout in send task, resetting LoRa"));

      loraHardwareReset();

      loraInit();
    }
  }
}

// Task 3: LoRa receive task – reads incoming packets and queues them + updates terminal.
void loraReceiveTask( void *pvParameters )
{
  char buf[MAX_RX_LEN];
  for(;;)
  {
    if(xSemaphoreTake(spiMutex, pdMS_TO_TICKS(700)) == pdTRUE)
    {
      int packetSize = LoRa.parsePacket();

      xSemaphoreGive(spiMutex);

      if(packetSize)
      {
        String rxMsg = "";
        if(xSemaphoreTake(spiMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
          while(LoRa.available())
            rxMsg += (char)LoRa.read();

          xSemaphoreGive(spiMutex);
        }
        else
        {
          Serial.println(F("SPI timeout during LoRa read, resetting"));

          loraHardwareReset();

          loraInit();
        }

        // Queue for SD logging:
        rxMsg.toCharArray(buf, sizeof(buf));
        xQueueSend(xLoraQueue, buf, 0);

        // Update on-screen terminal:
        if(xSemaphoreTake(loraRxMutex, pdMS_TO_TICKS(10)) == pdTRUE)
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
      Serial.println(F("SPI timeout on parsePacket, resetting"));

      loraHardwareReset();

      loraInit();
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// Task 4: HTTP server task – processes incoming client requests.
void httpServerTask( void *pvParameters )
{
  for(;;)
  {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Task 5: Uptime counter – increments uptimeSeconds every second.
void uptimeTask(void *pvParameters)
{
  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));

    uptimeSeconds++;

    // Only update sendMsg if no user-override pending:
    // (the send task uses a notify, so this flag is no longer needed)
  }
}

// Task 6: LoRa Check Task – periodically verifies LoRa responsiveness.
void loraCheckTask( void *pvParameters )
{
  const uint8_t expectedVersion = 0x12;
  for(;;)
  {
    Serial.println(F(""));
    Serial.print(F("LoRa check Task: "));

    uint8_t version = readLoRaRegister(0x42);

    if(version != expectedVersion)
    {
      Serial.printf("LoRa check failed (0x%02X), resetting\n", version);
      Serial.println(F(""));

      loraHardwareReset();

      loraInit();
    }
    else
      Serial.println(F("LoRa is OK"));

    Serial.println(F(""));

    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

// Task 7: SD-card logger – appends each queued LoRa line to /data.txt
void sdLogTask( void *pvParameters )
{
  // Ensure /data.txt exists
  if(!SD.exists("/data.txt"))
  {
    File f = SD.open("/data.txt", FILE_WRITE);

    if(f) 
      f.close();
    else 
      Serial.println(F("ERROR: could not create data.txt"));
  }

  char buf[MAX_RX_LEN];
  
  for(;;)
  {
    if(xQueueReceive(xLoraQueue, buf, portMAX_DELAY) == pdTRUE)
    {
      appendFile(SD, "/data.txt", buf);
      appendFile(SD, "/data.txt", "\n");

      //Serial.print(F("Logged: "));
      //Serial.println(buf);
    }
  }
}

// ===== setup() and loop() ===============================================================================
void setup()
{
  Serial.begin(115200);
  Serial.print(F(">> Press any key in your Serial Monitor to continue..."));

  /*
  
  // wait here until the user actually types something:
  while( Serial.available() == 0 ) 
    delay(10);
    
  */
  
  Serial.println(F("  "));
  Serial.println(F("Starting TTGO LoRa32 T3 V1.6.1 demo…"));

  // Initialize the second SPI for use with the SD card
  SDSPI.begin(SDCARD_SCLK, SDCARD_MISO, SDCARD_MOSI);

  if (!SD.begin(SDCARD_CS, SDSPI)) 
    Serial.println(F("Card Mount Failed"));
  else
    Serial.println(F("SD Mounted"));

  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) 
    Serial.println(F("No SD card attached"));

  Serial.print(F("SD Card Type: "));

  if (cardType == CARD_MMC) 
  {
      Serial.println(F("MMC"));
  }
  else if (cardType == CARD_SD) 
  {
      Serial.println(F("SDSC"));
  }
  else if (cardType == CARD_SDHC) 
  {
      Serial.println(F("SDHC"));
  }
  else 
  {
      Serial.println(F("UNKNOWN"));
  }

  pinMode(LORA_RST, OUTPUT);
  pinMode(LORA_SS, OUTPUT);

  // Create IPC primitives before touching the radio
  spiMutex     = xSemaphoreCreateMutex();
  loraRxMutex  = xSemaphoreCreateMutex();
  xLoraQueue   = xQueueCreate(10, MAX_RX_LEN);
  configASSERT(spiMutex && loraRxMutex && xLoraQueue);

  if(!loraInit())
  {
    Serial.println(F("LoRa init failed"));

    while(true) 
      vTaskDelay(10);
  }

  // WiFi + HTTP server
  setupWiFi();
  setupHTTPServer();

  // Spawn tasks (pin to whichever core you prefer)
  xTaskCreatePinnedToCore(displayTask,     "DisplayTask",    4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(loraSendTask,    "LoRaSendTask",   4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(loraReceiveTask, "LoRaReceiveTask",4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(httpServerTask,  "HTTPServerTask", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(loraCheckTask,   "LoRaCheckTask",  4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(sdLogTask,       "SDLogTask",      4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(uptimeTask,      "UptimeTask",     2048, NULL, 1, NULL, 0);

  // Delete setup()’s own task
  vTaskDelete(NULL);
}

void loop()
{
  // empty, all work in FreeRTOS tasks
}
