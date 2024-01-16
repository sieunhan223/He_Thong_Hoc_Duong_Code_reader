#include <LiquidCrystal.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <PCF8574.h>
#include <PN532_HSU.h>
#include <PN532.h>
#include <ESP32Time.h>

#define LCD_8BITMODE 0x10
#define LCD_SCREEN 0
#define SOUND 1
#define LED_2 2
#define LED_3 3
#define LED_4 4
#define LED_5 5

// Init for ESP32 Timer
ESP32Time rtc(3600); // offset in seconds GMT+1
const char *API_Time = "http://worldtimeapi.org/api/timezone/Asia/Ho_Chi_Minh";
String timer;

// init lcd
PCF8574 pcf(0x27);
LiquidCrystal lcd(32, 33, 25, 26, 27, 14, 13, 15, 2, 4);
// pn532
PN532_HSU pn532hsu(Serial2);
PN532 nfc(pn532hsu);
uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t checkCard;
// khai báo các biến
uint8_t CardUID[] = {0, 0, 0, 0, 0, 0, 0};
uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
String TerminalID = "";
String SSCID = "";
String Name_st = "", Name_display = "";
String s_CardUID = "";
// Tham số để tìm kiếm từ HTML form
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "terminal_id";
const char *PARAM_INPUT_4 = "URL_server";
const char *PARAM_INPUT_5 = "AP_name";
const char *PARAM_INPUT_6 = "AP_pass";
const char *PARAM_INPUT_7 = "jsonFile";
// Biến lưu các giá trị từ HTML form
String ssid, pass;
String terminal_id;
String URL_server;
String AP_name, AP_pass;
String jsonFile;
// Các biến kiểm tra
boolean restart = false, valid = true;
String checkSSCID = "";
// Các đường dẫn chứa giá trị
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *terminal_idPath = "/terminal_id.txt";
const char *URL_serverPath = "/URL_server.txt";
const char *AP_namePath = "/AP_name.txt";
const char *AP_passPath = "/AP_pass.txt";
const char *Students_list_path = "/students_list.json";

// http://192.168.4.1/ : AP IP Address

AsyncWebServer server(80);
WiFiClient client;
HTTPClient http;

IPAddress localIP;
//*****************************************************************************************//

// Khởi tạo SPIFS
void initSPIFFS()
{
  if (!SPIFFS.begin())
    Serial.println("An error has occurred while mounting SPIFFS");
  else
    Serial.println("SPIFFS mounted successfully");
}
// Hàm đọc file
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }
  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

// Hàm viết file
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
    Serial.println("- file written");

  else

    Serial.println("- frite failed");

  file.close();
}

// Khởi tạo Wifi
bool initWiFi()
{
  if (ssid == "")
  {
    Serial.println("Undefined SSID or IP address.");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    for (int i = 0; i < 3; i++)
    {
      pcf.digitalWrite(LED_2, LOW);
      delay(50);
      pcf.digitalWrite(LED_2, HIGH);
      delay(50);
    }
    delay(500);
  }
  return true;
}

// setup PN532
void setup_PN532()
{
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata)
  {
    Serial.print("Didn't find PN53x board");
    delay(1000);
    ESP.restart();
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5");
  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.');
  Serial.println((versiondata >> 8) & 0xFF, DEC);
  // configure board to read RFID tags
  nfc.SAMConfig();
}

// read nfc
String ReadBlock(uint8_t *keya, uint8_t block)
{
  String s_data;
  uint8_t success;
  Serial.println("Found an ISO14443A card");
  Serial.print("  UID Length: ");
  Serial.print(uidLength, DEC);
  Serial.println(" bytes");
  Serial.print("  UID Value: ");
  // Xử lý biến CardUID thành chuỗi:
  s_CardUID = "";
  for (int i = 0; i < uidLength; i++)
  {
    s_CardUID += String(CardUID[i], HEX);
    s_CardUID += ' ';
  }
  Serial.println(s_CardUID);
  Serial.println("");

  if (uidLength == 4)
  {
    // Xác thực Key A
    Serial.println("Seems to be a Mifare Classic card (4 byte UID)");
    Serial.println("Trying to authenticate block 4 with default KEYA value");
    success = nfc.mifareclassic_AuthenticateBlock(CardUID, uidLength, block, 0, keya);
    if (success)
    {
      Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
      uint8_t data[16];
      // Đọc thẻ
      success = nfc.mifareclassic_ReadDataBlock(block, data);

      if (success)
      {
        Serial.print("Reading Block...");
        for (int i = 0; i < 16; i++)
          s_data += (char)data[i];
        Serial.println("Done.");
        valid = true;
        return s_data;
      }

      else
        Serial.println("Ooops ... unable to read the requested block.  Try another key?");
    }
    else
      Serial.println("Ooops ... authentication failed: Try another key?");
  }
  valid = false;
  return "";
}

// Hàm đọc file JSON
String readFileJson(fs::FS &fs, const char *path, String sscid)
{
  File file = fs.open(path, "r");
  if (!file || file.isDirectory())
  {
    Serial.println("- empty file or failed to open file");
    return "";
  }

  DynamicJsonDocument doc(13000);
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  String ten = doc[sscid];
  file.close();
  return ten;
}

// Xử lý tên trên 14 ký tự:
String overName(String name)
{
  int len_ten = name.length();
  int index_cuoi = name.lastIndexOf(' ');
  String temp;
  temp += name[0];
  temp += '.';
  for (int i = 1; i < len_ten; i++)
  {
    if ((name[i] == ' ') && (index_cuoi != i))
    {
      temp += name[i + 1];
      temp += '.';
    }
    else if ((name[i] == ' ') && (index_cuoi == i))
    {
      temp += name.substring(index_cuoi + 1, len_ten);
      break;
    }
  }
  return temp;
}

// Xử lấy lấy thông tin thời gian từ API:
String GetTimeFromAPI()
{
  int checkAPI = http.begin(API_Time);
  if (checkAPI)
  {
    int cod = http.GET();
    if (cod == HTTP_CODE_OK)
    {
      // Init for Json
      String data = http.getString();
      DynamicJsonDocument doc(1000);
      DeserializationError error = deserializeJson(doc, data);
      const char *dataTime = doc["datetime"];
      // format date time:
      String time = "";
      for (int i = 0; i < strlen(dataTime); i++)
      {
        if (dataTime[i] == 'T')
          time += "|";
        else if (dataTime[i] == '.')
          break;
        else if (dataTime[i] == '-')
          time += " ";
        else
          time += dataTime[i];
      }
      return time;
    }
    else
      Serial.printf("date time-[HTTP] GET... failed, error: %s\n", http.errorToString(cod).c_str());
    http.end();
  }
  return GetTimeFromAPI();
}

void SetBeginTime()
{
  String beginTime = GetTimeFromAPI();
  Serial.println(beginTime);
  rtc.setTime(beginTime.substring(17, 19).toInt(), beginTime.substring(14, 16).toInt(), beginTime.substring(11, 13).toInt() - 1, beginTime.substring(8, 10).toInt(), beginTime.substring(5, 7).toInt(), beginTime.substring(0, 4).toInt()); // 17th Jan 2021 15:24:30
}

// In thông báo lỗi:
void errorDisplay()
{
  // Nếu bị lỗi:
  lcd.clear();
  lcd.setCursor(8, 1);
  lcd.print("LOI");
  lcd.setCursor(1, 2);
  lcd.print("VUI LONG THU LAI!");
}
//------------------------------------------
// kèn:
void KenKeu()
{
  // kèn
  pcf.digitalWrite(SOUND, LOW);
  pcf.digitalWrite(LED_3, HIGH);
  delay(200);
  pcf.digitalWrite(SOUND, HIGH);
  pcf.digitalWrite(LED_3, LOW);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------//
void setup()
{
  Serial.begin(115200);
  nfc.begin();
  setup_PN532();
  initSPIFFS();

  // Init PCF
  pcf.begin();
  for (int i = 0; i < 6; i++)
    pcf.pinMode(i, OUTPUT);
  pcf.digitalWrite(SOUND, HIGH); // Lỗi đi dây
  pcf.digitalWrite(LCD_SCREEN, HIGH);

  // LOADING
  lcd.begin(20, 4);
  lcd.clear();
  lcd.setCursor(5, 2);
  lcd.print("LOADING...");

  // đọc và lưu giá trị ở tệp
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  terminal_id = readFile(SPIFFS, terminal_idPath);
  URL_server = readFile(SPIFFS, URL_serverPath);
  AP_name = readFile(SPIFFS, AP_namePath);
  AP_pass = readFile(SPIFFS, AP_passPath);
  jsonFile = readFile(SPIFFS, Students_list_path);

  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(terminal_id);
  Serial.println(URL_server);
  Serial.println(AP_name);
  Serial.println(AP_pass);
  Serial.println(jsonFile);
  TerminalID = terminal_id;

  // Kích hoạt chế độ AP
  if (!WiFi.softAP(AP_name.c_str(), AP_pass.c_str()))
  {
    log_e("Soft AP creation failed.");
    ESP.restart();
  }
  localIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(localIP);

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/device_config.html", "text/html"); });

  server.serveStatic("/", SPIFFS, "/");

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        // HTTP POST ssid value
        if (p->name() == PARAM_INPUT_1) {
          ssid = p->value().c_str();
          Serial.print("SSID set to: ");
          Serial.println(ssid);
          // Write file to save value
          writeFile(SPIFFS, ssidPath, ssid.c_str());
        }
        // HTTP POST pass value
        if (p->name() == PARAM_INPUT_2) {
          pass = p->value().c_str();
          Serial.print("Password set to: ");
          Serial.println(pass);
          // Write file to save value
          writeFile(SPIFFS, passPath, pass.c_str());
        }
        // HTTP POST terminal_id value
        if (p->name() == PARAM_INPUT_3) {
          terminal_id = p->value().c_str();
          Serial.print("Terminal ID set to: ");
          Serial.println(terminal_id);
          // Write file to save value
          writeFile(SPIFFS, terminal_idPath, terminal_id.c_str());
        }
        // HTTP POST server value
        if (p->name() == PARAM_INPUT_4) {
          URL_server = p->value().c_str();
          Serial.print("server address set to: ");
          Serial.println(URL_server);
          // Write file to save value
          writeFile(SPIFFS, URL_serverPath, URL_server.c_str());
        }
        // HTTP POST AP name value
        if (p->name() == PARAM_INPUT_5) {
          AP_name = p->value().c_str();
          Serial.print("AP name set to: ");
          Serial.println(AP_name);
          // Write file to save value
          writeFile(SPIFFS, AP_namePath, AP_name.c_str());
        }
        // HTTP POST AP pass value
        if (p->name() == PARAM_INPUT_6) {
          AP_pass = p->value().c_str();
          Serial.print("AP pass set to: ");
          Serial.println(AP_pass);
          // Write file to save value
          writeFile(SPIFFS, AP_passPath, AP_pass.c_str());
        }
        // HTTP POST Studen List value
        // if (p->name() == PARAM_INPUT_7) {
        //     File file = SPIFFS.open("/students_list.json", "w");
        //     // Đọc dữ liệu từ tệp tải lên và lưu vào tệp /data.json
        //     size_t fileSize = p->size(); // Kích thước tệp
        //     uint8_t buffer[fileSize];
        //     p->value().getBytes(buffer, fileSize);
        //     file.write(buffer, fileSize);
        //     file.close();
        //     // Lưu dữ liệu vào tệp /data.json trong bộ nhớ

        //     if (!file) {
        //       Serial.println("Failed to open file for writing");
        //     } else {
        //       // Đặt nội dung vào một cấu trúc JSON
        //       DynamicJsonDocument doc(1024*20); // Thay đổi kích thước dựa trên kích thước của dữ liệu JSON
        //       DeserializationError error = deserializeJson(doc, buffer);
              
        //       if (error) {
        //         Serial.println("Failed to parse JSON");
        //       } else {
        //         // Ghi dữ liệu JSON vào tệp
        //         serializeJson(doc, file);
        //         file.close();
        //         Serial.println("JSON data saved successfully");
        //     }
        //   }
        // }
      }
    }
    restart = true;
    request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: http://192.168.4.1/"); });

  server.begin();

  // Kết nối WiFi
  if (initWiFi())
  {
    // setup RealTime
    delay(1000);
    SetBeginTime();
    lcd.clear();
    lcd.setCursor(1, 1);
    lcd.print("THIET BI DIEM DANH");
    lcd.setCursor(1, 2);
    lcd.print("CONG TY DIEN TU C&T");
    Serial.print("\nConnected!, IP address: ");
    Serial.println(WiFi.localIP());
    pcf.digitalWrite(LED_2, HIGH);
  }
}
//*****************************************************************************************//
void loop()
{
  if (restart)
  {
    delay(5000);
    ESP.restart();
  }
  // kiểm tra thẻ mới
  checkCard = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, CardUID, &uidLength);
  if (checkCard)
  {
    Serial.println(F("**Card Detected:**"));
    SSCID = ReadBlock(key, 4);
    Serial.println(F("\n**End Reading**\n"));
    Serial.println("Data: " + SSCID);

    if (!valid)
    {
      errorDisplay();
      KenKeu();
    }
    else
    {
      lcd.clear();
      // In tên lên lcd:
      lcd.setCursor(0, 0);
      lcd.print("Name: ");
      Name_st = readFileJson(SPIFFS, "/students_list.json", SSCID);
      if (Name_st.length() > 14)
      {
        Name_display = overName(Name_st);
        lcd.print(Name_display);
      }
      else
        lcd.print(Name_st);
      // in SSCID lên LCD
      lcd.setCursor(0, 1);
      lcd.print("ID: ");
      lcd.print(SSCID);
      // In Thời gian lên lcd:
      timer = rtc.getTime("%d/%m/%Y %H:%M:%S");
      lcd.setCursor(0, 2);
      lcd.print(timer);
      // in mã thiết bị lên LCD
      lcd.setCursor(0, 3);
      lcd.print("Device: ");
      lcd.print(TerminalID);

      KenKeu();

      // POST lên server
      StaticJsonDocument<200> doc; // khai báo chỗ chứa dữ liệu
      String jsonData;
      doc["TerminalID"] = TerminalID;
      doc["CardUID"] = s_CardUID;
      doc["SSCID"] = SSCID;
      doc["Name"] = Name_st;
      serializeJsonPretty(doc, jsonData);
      Serial.print("[HTTP] begin...\n");
      // configure traged server and url
      http.begin(client, URL_server + "/api/nfc-tab/test"); // HTTP
      // http.begin(*clients, URL_server + "/api/nfc-tab/test"); // HTTPS
      http.addHeader("Content-Type", "application/json");

      Serial.print("[HTTP] POST...\n");
      // start connection and send HTTP header and body
      int httpCode = http.POST(jsonData);

      // httpCode will be negative on error
      if (httpCode > 0)
      {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] POST... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
          const String &payload = http.getString();
          Serial.println("received payload:\n");
          Serial.println(payload);
          pcf.digitalWrite(LED_4, HIGH);
          delay(700);
          pcf.digitalWrite(LED_4, LOW);
        }
        else
        {
          pcf.digitalWrite(LED_5, HIGH);
          delay(700);
          pcf.digitalWrite(LED_5, LOW);
          Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
      }
      http.end();
    }
  }
}
