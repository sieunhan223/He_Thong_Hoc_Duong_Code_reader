#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiClient.h>
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>


#define RST_PIN 3 
#define SS_PIN 4 
#define ken 8
MFRC522 mfrc522(SS_PIN, RST_PIN); 
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

// http://192.168.4.1/ : AP IP Address

AsyncWebServer server(80);
WiFiClient client;
HTTPClient http;

IPAddress localIP;

//*****************************************************************************************//

void splitName(String name, String lst_name[]){
  int sotu = 0;
  int index_dau = 0,index_cuoi = 0;
  for(int i = 0; i < name.length(); i++){
    if (name[i] == ' '){
      index_cuoi = name.indexOf(' ',index_cuoi+1);
      lst_name[sotu] = name.substring(index_dau,index_cuoi);
      index_dau = index_cuoi+1;
      sotu++;
    }
  }
  lst_name[sotu] = name.substring(name.lastIndexOf(' ')+1,name.length());
}

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

// Read File from SPIFFS
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
// Write file to SPIFFS
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

// Initialize SPIFFS
void initSPIFFS()
{
  if (!SPIFFS.begin())
    Serial.println("An error has occurred while mounting SPIFFS");
  else
    Serial.println("SPIFFS mounted successfully");
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
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
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
    initSPIFFS();       // Init initSPIFFS
    SPI.begin();      // Init SPI bus
    mfrc522.PCD_Init();   // Init MFRC522 card

    pinMode(ken, OUTPUT); // Init ken
    lcd.init();           // Init lcd
    lcd.backlight();
    lcd.setCursor(5, 2);
    lcd.print("LOADING...");
    // đọc và lưa giá trị ở tệp
    ssid = readFile(SPIFFS, ssidPath);
    pass = readFile(SPIFFS, passPath);
    terminal_id = readFile(SPIFFS, terminal_idPath);
    URL_server = readFile(SPIFFS, URL_serverPath);
    AP_name = readFile(SPIFFS, AP_namePath);
    AP_pass = readFile(SPIFFS, AP_passPath);

    Serial.println(ssid);
    Serial.println(pass);
    Serial.println(terminal_id);
    Serial.println(URL_server);
    Serial.println(AP_name);
    Serial.println(AP_pass);

    // Kích hoạt chế độ AP
    Serial.println("Setting AP (Access Point)");
    Serial.print("Configuring access point...");
    WiFi.softAP(AP_name.c_str(), AP_pass.c_str());
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    //Kết nối WiFi
    initWiFi();

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
    // key.keyByte[0] = 0x01;
    // key.keyByte[1] = 0x02;
    // key.keyByte[2] = 0x03;
    // key.keyByte[3] = 0x04;
    // key.keyByte[4] = 0x05;
    // key.keyByte[5] = 0x06;

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
    // In lên màn hình:
    lcd.clear();

    //Xử lý biến name
    String s_Name = s_Name1 + s_Name2;
    int len_name = s_Name.length();
    for (int i = len_name-1; i >= 0; i--){
        // Serial.print(i);
        // Serial.print("\t");
        // Serial.println(a[i]);
        if(s_Name[i] != ' '){
        s_Name = s_Name.substring(0,i+1);
        len_name = s_Name.length();
        // Serial.println(s_Name);
        // Serial.println(len_name);
        break;
        }
    }


    // in SSCID lên LCD
    lcd.setCursor(0, 1);
    lcd.print("ID: ");
    lcd.print(s_SSCID);

    //in mã thiết bị lên LCD
    lcd.rightToLeft();
    lcd.setCursor(0, 3);
    lcd.print(TerminalID);
    lcd.leftToRight();

    // Nếu bị lỗi:
    if (!xacthuc)
    {
        lcd.clear();
        lcd.setCursor(8, 1);
        lcd.print("LOI");
        lcd.setCursor(1, 2);
        lcd.print("VUI LONG THU LAI!");
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
                const char *data_Time = obj2["Time"]; // lấy real time
                const char* data_Name = obj2["Name"];// lấy Name

                //Xư lý Real Time
                String RealTime = String(data_Time); 
                String RealTime1 = RealTime.substring(0,10);
                String RealTime1_list[] = {RealTime1.substring(8,10),RealTime1.substring(5,7),RealTime1.substring(0,4)};
                RealTime1 = RealTime1_list[0] + "/" + RealTime1_list[1] + "/" + RealTime1_list[2];
                String RealTime2 = RealTime.substring(11,16);

                //Name
                String Name_API = String(data_Name);
                //Serial.println(Name_API);
                int len_name = Name_API.length();
                //In Name len LCD
                lcd.setCursor(0, 0);
                String lst[10] = {"","","","","","","","","","",};
                splitName(Name_API,lst);
                if(len_name > 21){
                    for (int i = 1; i < lst->length(); i ++){
                    if(lst[i+1] != ""){
                        lst[i] = lst[i].substring(0,1);
                    }
                    else
                        break;
                    }
                }
                for (int i = 0; i < lst->length(); i ++){
                    if ((lst[i].length() == 1) && (lst[i+1].length() == 1)){
                        Serial.print(lst[i]);
                        lcd.print(lst[i]);
                    }
                    else{
                        Serial.print(lst[i]+" ");
                        lcd.print(lst[i]);
                        lcd.print(" ");
                    }

                }
                
                // in RealTime lên LCD
                lcd.setCursor(0, 2);
                lcd.print(RealTime1);
                lcd.print(" ");
                lcd.print(RealTime2);
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
