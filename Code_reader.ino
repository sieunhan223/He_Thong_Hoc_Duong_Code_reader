#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <WiFiClientSecureBearSSL.h>
#include "LittleFS.h";

#define RST_PIN D3 // Configurable, see typical pin layout above
#define SS_PIN D4  // Configurable, see typical pin layout above
#define ken D8
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
MFRC522::MIFARE_Key key;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Search for parameter in HTTP POST request
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "terminal_id";
const char *PARAM_INPUT_4 = "URL_server";
const char *PARAM_INPUT_5 = "AP_name";
const char *PARAM_INPUT_6 = "AP_pass";
// Variables to save values from HTML form
String ssid, pass;
String terminal_id;
String URL_server;
String AP_name, AP_pass;
boolean restart = false, xacthuc = true;
// File paths to save input values permanently
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *terminal_idPath = "/terminal_id.txt";
const char *URL_serverPath = "/URL_server.txt";
const char *AP_namePath = "/AP_name.txt";
const char *AP_passPath = "/AP_pass.txt";

// http://192.168.4.1/

// Root certificate for howsmyssl.com
const char IRG_Root_X1 [] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ
RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD
VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX
DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y
ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy
VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr
mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr
IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK
mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu
XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy
dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye
jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1
BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3
DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92
9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx
jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0
Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz
ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS
R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp
-----END CERTIFICATE-----
)CERT";

// Create a list of certificates with the server certificate
X509List cert(IRG_Root_X1);

// Fingerprint (might need to be updated)
const uint8_t fingerprint[20] = {0x1a, 0x1b, 0xd6, 0x42, 0xec, 0x90, 0x99, 0x84, 0xf5, 0x3c, 0x1f, 0xee, 0x43, 0xd1, 0x36, 0x0f, 0xef, 0xb7, 0x3d, 0xb9};

AsyncWebServer server(80);

IPAddress localIP;

//*****************************************************************************************//

// hàm đọc:
void readBlock(byte block, byte len, byte *content, MFRC522::StatusCode &status)
{
    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)); // line 834
    if (status != MFRC522::STATUS_OK)
    {
        Serial.print(F("Authentication failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        xacthuc = false;
    }

    status = mfrc522.MIFARE_Read(block, content, &len);
    if (status != MFRC522::STATUS_OK)
    {
        Serial.print(F("Reading failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        xacthuc = false;
    }
}

// Read File from LittleFS
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
// Write file to LittleFS
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
    {
        Serial.println("- file written");
    }
    else
    {
        Serial.println("- frite failed");
    }
    file.close();
}

// Initialize LittleFS
void initFS()
{
    if (!LittleFS.begin())
    {
        Serial.println("An error has occurred while mounting LittleFS");
    }
    else
    {
        Serial.println("LittleFS mounted successfully");
    }
}

// Initialize WiFi
bool initWiFi()
{
    if (ssid == "")
    {
        Serial.println("Undefined SSID or IP address.");
        return false;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    Serial.println("Connecting to WiFi...");
    int dem = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        if (dem > 10)
        {
            break;
        }
        Serial.print(".");
        dem = dem + 1;
        for (int i = 0; i < 3; i++)
        {
            digitalWrite(ken, HIGH);
            delay(50);
            digitalWrite(ken, LOW);
        }
        delay(500);
    }
    lcd.clear();
    lcd.setCursor(1, 1);
    lcd.print("THIET BI DIEM DANH");
    lcd.setCursor(1, 2);
    lcd.print("CONG TY DIEN TU C&T");
    Serial.print("\nConnected!, IP address: ");
    Serial.println(WiFi.localIP());
    return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------//
void setup()
{
    Serial.begin(112500); // Initialize serial communications with the PC
    initFS();             // InitFS
    SPI.begin();          // Init SPI bus
    mfrc522.PCD_Init();   // Init MFRC522 card

    pinMode(ken, OUTPUT); // Init ken
    lcd.init();           // Init lcd
    lcd.backlight();
    lcd.setCursor(5, 2);
    lcd.print("LOADING...");
    // đọc và lưa giá trị ở tệp
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    terminal_id = readFile(LittleFS, terminal_idPath);
    URL_server = readFile(LittleFS, URL_serverPath);
    AP_name = readFile(LittleFS, AP_namePath);
    AP_pass = readFile(LittleFS, AP_passPath);

    Serial.println(ssid);
    Serial.println(pass);
    Serial.println(terminal_id);
    Serial.println(URL_server);
    Serial.println(AP_name);
    Serial.println(AP_pass);

    initWiFi();

    // Kết nối web esp
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    Serial.print("Configuring access point...");
    WiFi.softAP(AP_name, AP_pass);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/device_config.html", "text/html"); });

    server.serveStatic("/", LittleFS, "/");

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
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
          }
          // HTTP POST terminal_id value
          if (p->name() == PARAM_INPUT_3) {
            terminal_id = p->value().c_str();
            Serial.print("Terminal ID set to: ");
            Serial.println(terminal_id);
            // Write file to save value
            writeFile(LittleFS, terminal_idPath, terminal_id.c_str());
          }
          // HTTP POST server value
          if (p->name() == PARAM_INPUT_4) {
            URL_server = p->value().c_str();
            Serial.print("server address set to: ");
            Serial.println(URL_server);
            // Write file to save value
            writeFile(LittleFS, URL_serverPath, URL_server.c_str());
          }
          // HTTP POST AP name value
          if (p->name() == PARAM_INPUT_5) {
            AP_name = p->value().c_str();
            Serial.print("AP name set to: ");
            Serial.println(AP_name);
            // Write file to save value
            writeFile(LittleFS, AP_namePath, AP_name.c_str());
          }
          // HTTP POST AP pass value
          if (p->name() == PARAM_INPUT_6) {
            AP_pass = p->value().c_str();
            Serial.print("AP pass set to: ");
            Serial.println(AP_pass);
            // Write file to save value
            writeFile(LittleFS, AP_passPath, AP_pass.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      restart = true;
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: http://192.168.4.1/"); });

    server.begin();
}
//*****************************************************************************************//
void loop()
{
    if (restart)
    {
        delay(5000);
        ESP.restart();
    }

    // khai báo các biến
    byte CardUID[18];
    String s_CardUID = "";
    String TerminalID = terminal_id;
    byte SSCID[18];
    String s_SSCID = "";
    byte Name1[18];
    String s_Name1 = "";
    byte Name2[18];
    String s_Name2 = "";

    // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.

    for (byte i = 0; i < 6; i++)
        key.keyByte[i] = 0xFF;

    // some variables we need
    byte block;
    byte len;
    String temp;
    MFRC522::StatusCode status;

    //-------------------------------------------

    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    if (!mfrc522.PICC_IsNewCardPresent())
        return;

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial())
        return;

    Serial.println(F("**Card Detected:**"));

    // Lưu UID Card vào biến
    for (byte i = 0; i < 4; i++)
    {
        CardUID[i] = mfrc522.uid.uidByte[i];
        if (CardUID[i] < 0x10)
            s_CardUID += "0" + String(CardUID[i]);
        else
            s_CardUID += String(CardUID[i]);
    }

    mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid)); // dump some details about the card

    // mfrc522.PICC_DumpToSerial(&(mfrc522.uid));      //uncomment this to see all blocks in hex

    //-------------------------------------------------------------------------------------------------*
    // Xử lý biến SSCID
    Serial.print("block4: ");
    block = 4;
    len = 18;

    readBlock(block, len, SSCID, status);

    temp = String((char *)SSCID);
    for (int i = 0; i < 16; i++)
    {
        s_SSCID += temp[i];
    }
    // PRINT SSID
    for (uint8_t i = 0; i < 16; i++)
    {
        if (SSCID[i] != 32)
        {
            Serial.write(SSCID[i]);
        }
    }
    Serial.println("");

    //-------------------------------------------------------------------------------------------------*
    // Xử lý biến Name1
    Serial.print("block5: ");
    block = 5;
    len = 18;
    readBlock(block, len, Name1, status);

    temp = String((char *)Name1);

    for (int i = 0; i < 16; i++)
    {
        if (Name1[i] != 0x0)
        {
            s_Name1 += temp[i];
        }
    }
    // print Name1
    for (uint8_t i = 0; i < 16; i++)
    {
        Serial.write(Name1[i]);
    }
    Serial.println("");
    //-------------------------------------------------------------------------------------------------*
    // Xử lý biến Name2
    Serial.print("block6: ");
    block = 6;
    len = 18;

    readBlock(block, len, Name2, status);

    temp = String((char *)Name2);

    for (int i = 0; i < 16; i++)
    {
        if (Name2[i] != 0x0)
        {
            s_Name2 += temp[i];
        }
    }
    // PRINT Name2
    for (uint8_t i = 0; i < 16; i++)
    {
        if (Name2[i] != 0x0)
        {
            Serial.write(Name2[i]);
        }
    }
    Serial.println("");
    //-------------------------------------------------------------------------------------------------*
    Serial.println(F("\n**End Reading**\n"));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    //-------------------------------------------------------------------------------------------------*
    // đọc thẻ:
    lcd.clear();
    // In Name lên LCD
    String s_Name = s_Name1 + s_Name2;
    lcd.print(s_Name);
    Serial.println("");
    // In lớp lên LCD
    lcd.setCursor(0, 1);
    lcd.print("LOP: ");
    lcd.print("1A");

    // in trạng thái lên LCD
    lcd.setCursor(0, 2);
    lcd.print("ID: ");
    lcd.print(s_SSCID);

    if (!xacthuc)
    {
        lcd.clear();
        lcd.setCursor(8, 1);
        lcd.print("LOI");
        lcd.setCursor(1, 2);
        lcd.print("VUI LONG THU LAI!");
        xacthuc = true;
    }

    // kèn
    digitalWrite(ken, HIGH);
    delay(200);
    digitalWrite(ken, LOW);

    //-------------------------------------------------------------------------------------------------*
    // POST lên server
    if (xacthuc)
    {
        StaticJsonDocument<200> doc; // khai báo chỗ chứa dữ liệu
        String jsonData;
        doc["TerminalID"] = TerminalID;
        doc["CardUID"] = s_CardUID;
        doc["SSCID"] = s_SSCID;
        doc["Name"] = s_Name;
        serializeJsonPretty(doc, jsonData);
        WiFiClient client;
        HTTPClient http;

        // std::unique_ptr<BearSSL::WiFiClientSecure>clients(new BearSSL::WiFiClientSecure);

        // clients->setFingerprint(fingerprint);

        Serial.print("[HTTP] begin...\n");
        // configure traged server and url
        http.begin(client, URL_server + "/api/nfc-tab/test"); // HTTP  
        // http.begin(*clients, URL_server + "/api/nfc-tab/test"); // HTTPS
        http.addHeader("Content-Type", "application/json");

        Serial.print("[HTTP] POST...\n");
        // start connection and send HTTP header and body
        int httpCode = http.POST(jsonData);

        // httpCode will be negative on err or
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

                //-------------------------------------------------------------------------------------------------*
                // GET
                StaticJsonDocument<200> doc2;   // tạo doc chứa data
                deserializeJson(doc2, payload); // fix payload(dữ liệu từ server trả về) với doc2
                JsonObject obj = doc2.as<JsonObject>();
                String data = obj["Data"];   // lấy dữ liệu tên Data
                deserializeJson(doc2, data); // Vì Data là 1 JSON cho nên phải fix data thêm 1 lần nữa
                JsonObject obj2 = doc2.as<JsonObject>();
                const char *RealTime = obj2["Time"]; // lấy real time
                // in RealTime lên LCD
                lcd.setCursor(0, 3);
                for (int i = 0; i < 19; i++)
                {
                    if (RealTime[i] == 'T')
                        lcd.print(" ");
                    else
                        lcd.print(RealTime[i]);
                }
            }
            else
                Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    }
    else
        xacthuc = true;
    delay(300);
    //-------------------------------------------
}
