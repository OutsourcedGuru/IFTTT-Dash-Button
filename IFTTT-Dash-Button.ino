/*
** --------------------------------------------------------------------------
** Libraries
** --------------------------------------------------------------------------
*/

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>

/*
** --------------------------------------------------------------------------
** Configuration settings
**
** Connect CONFIG_PIN(GPIO_13) to GND during startup to enter config mode:
**    1. Connect to 'BRB-IFTTT' WiFi Access Point, password '123987654'
**    2. Visit http://192.168.4.1 to open the configuration page or
**       http://192.168.4.1/edit to see the filesystem.
**    3. After setting your values, click 'Save' then 'Restart'
**    4. Your BRB is now configured.
** --------------------------------------------------------------------------
*/

String ssid;
String password;
String host;
String url;
int    wc_p;     // time in seconds to connect to wifi before giving up
int    gr_p;     // attempts to perform GET request before giving up
bool   s_vcc;    // send VCC voltage as a parameter in the url request
bool   is_ip;    // indicated host is IP address rather than a hostname
String vcc_parm; // parameter to pass VCC voltage as

/*
** --------------------------------------------------------------------------
** System variables
** --------------------------------------------------------------------------
*/

int get_timeout =    60000;   // time in milliseconds to wait for a GET request
#define NOT_DEBUG             // Enable debug or show indicator lights
int failCount =      0;
ADC_MODE(ADC_VCC);
bool su_mode =       true;
#define CONFIG_PIN 13
ESP8266WebServer server(80);
File fsUploadFile;
const char *APssid = "BRB-IFTTT";
const char *APpass = "123987654";

void setup() {
  // Start serial monitor, SPIFFS and config pin
  Serial.begin(115200);
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  delay(10);
  SPIFFS.begin();
  Serial.println();
  Serial.println("BRB booting...");
  Serial.println("SPIFFS content: ");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }

  // Read/parse config.json from SPIFFS
  readConfig();

  // Read the config GPIO pin
  su_mode = !digitalRead(CONFIG_PIN);
  if (su_mode) {
    // Start config mode
#ifdef NOT_DEBUG
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
#endif
    Serial.println("Entering config mode");
    // Start wi-fi access point
    Serial.println("Configuring access point...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(APssid, APpass);
    IPAddress myIP = WiFi.softAPIP();
    Serial.println("AP IP address: " + myIP);
    // Start HTTP server
    // Edit pages
    server.on("/list", HTTP_GET, handleFileList);
    server.on("/edit", HTTP_GET, []() {
      if (!handleFileRead("/edit.htm")) {
        server.send(404, "text/plain", "FileNotFound");
      }
    });
    server.on("/edit", HTTP_PUT, handleFileCreate);
    server.on("/edit", HTTP_DELETE, handleFileDelete);
    server.on("/edit", HTTP_POST, []() { server.send(200, "text/plain", ""); }, handleFileUpload);
    // Pages from SPIFFS
    server.onNotFound([]() {
      if (!handleFileRead(server.uri())) {
        server.send(404, "text/plain", "FileNotFound");
      }
    });
    server.begin();
    Serial.println("HTTP server started");
  } else {
    // Start normal mode
    // Connect to existing wi-fi
    Serial.println(); Serial.println();
    Serial.println("Connecting: " + ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      // We are taking too long to connect to the wi-fi...
      failCount++;
      if (failCount == wc_p * 2) {
        Serial.println("Session aborted after several tries connecting to wi-fi.");
        Serial.println("Entering deep sleep.");
        delay(20);
        fail();
        ESP.deepSleep(0);
      }
    }
    Serial.println("");
    Serial.println("Wi-fi connected with IP address: " + WiFi.localIP());
    failCount = 0;
  }
}

void loop() {
  if (su_mode) {
    server.handleClient();
  } else {
    //if we have tried too many times to make a GET Request give up.
    ++failCount;
    if (failCount == gr_p + 1) {
      Serial.println("Session aborted after several tries doing GET request.");
      Serial.println("Entering deep sleep.");
      delay(20);
      fail();
      ESP.deepSleep(0);
    }

    Serial.println("Attempt " + failCount + " connecting to " + host);
    // Try to connect to the host with TCP
    WiFiClient client;
    const int httpPort = 80;
    if (is_ip) {
      IPAddress addr;
      if (addr.fromString(host)) {
        if (!client.connect(addr, httpPort)) {
          // Try again if the connection fails
          Serial.println("Error: Connection to IP failed [is_ip=true]");
          delay(10);
          return;
        }
      } else {
        Serial.println("Error: Failed to convert IP string to IP address.");
        while (1)
          ;
      }
    } else {
      if (!client.connect(host.c_str(), httpPort)) {
        // Try again if the connection fails
        Serial.println("Error: Connection failed [is_ip=false]");
        delay(10);
        return;
      }
    }

    // Create the path for the request
    if (s_vcc) {
      url +=            "?";
      url +=            vcc_parm;
      url +=            "=";
      uint32_t getVcc = ESP.getVcc();
      String VccVol =   String((getVcc / 1000U) % 10) + "." +
                        String((getVcc / 100U) % 10) +
                        String((getVcc / 10U) % 10) +
                        String((getVcc / 1U) % 10);
      url +=            VccVol;
    }

    // Make the GET request from the server
    Serial.println("Requesting URL: " + url);
    client.print(
      String("GET ") + url + " HTTP/1.1" + "\r\n" +
      "Host: " + host.c_str() +            "\r\n" +
      "Connection: close" +                "\r\n\r\n"
    );

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > get_timeout) {
        // Abort if the server takes too long to reply
        Serial.println(">>> Client timeout.");
        client.stop();
        Serial.println("Entering deep sleep.");
        ESP.deepSleep(0);
      }
    }

    // Print response to serial
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    // Complete request and close connection
    client.stop();
    Serial.println();
    Serial.println("Closing connection...");

    // Enter deep sleep
    Serial.println("Entering deep sleep.");
    delay(100);
    success();
    ESP.deepSleep(0);
  }
}

/*
** --------------------------------------------------------------------------
** Universal Functions
** --------------------------------------------------------------------------
*/

void fail() {
  // Something has gone wrong, blink an indicator on the LED six times.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);   digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);   digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);   digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);   digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(250);   digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(500);   digitalWrite(LED_BUILTIN, HIGH);
} // fail()

void success() {
  // Everything worked, blink an indicator on the LED twice (long/short).
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);   delay(1500);  digitalWrite(LED_BUILTIN, HIGH);  delay(250);
  digitalWrite(LED_BUILTIN, LOW);   delay(350);   digitalWrite(LED_BUILTIN, HIGH);
} // success()

void readConfig() {
  // Read config.json and load configuration into variables
  File configFile = SPIFFS.open("/config.jsn", "r");
  if (!configFile) {
    Serial.println("Error: Failed to open config file");
    while (1)
      ;
  }
  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Error: Config file size is too large");
    while (1)
      ;
  }
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(buf.get());
  if (!json.success()) {
    Serial.println("Error: Failed to parse config file");
    while (1)
      ;
  }

  ssid =     (const char *)json["ssid"];
  password = (const char *)json["pass"];
  host =     (const char *)json["host"];
  url =      (const char *)json["uri"];
  wc_p =     json["wc_p"];
  gr_p =     json["gr_p"];
  s_vcc =    json["s_vcc"];
  is_ip =    json["is_ip"];
  vcc_parm = (const char *)json["vcc_p"];

  Serial.println("Parsed JSON config.");
  Serial.println("Loaded ssid: " +                     ssid);
  Serial.println("Loaded password: " +                 password);
  Serial.println("Loaded host: " +                     host);
  Serial.println("Loaded IsIP: " +                     is_ip);
  Serial.println("Loaded uri: " +                      url);
  Serial.println("Loaded WiFi Connect Persistance: " + wc_p);
  Serial.println("Loaded GET Request Persistance: " +  gr_p);
  Serial.println("Loaded Send VCC: " +                 s_vcc);
  Serial.println("Loaded VCC Param.: " +               vcc_parm);
  Serial.println();
} // readConfig()

/*
** --------------------------------------------------------------------------
** Config mode functions
** --------------------------------------------------------------------------
*/

// Edit functions
String formatBytes(size_t bytes) {
  if (bytes < 1024)                    return String(bytes) + "B";
  if (bytes < (1024 * 1024))           return String(bytes / 1024.0) + "KB";
  if (bytes < (1024 * 1024 * 1024))    return String(bytes / 1024.0 / 1024.0) + "MB";
  return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
} // formatBytes()

String getContentType(String filename) {
  if (server.hasArg("download"))       return "application/octet-stream";
  if (filename.endsWith(".htm"))       return "text/html";
  if (filename.endsWith(".html"))      return "text/html";
  if (filename.endsWith(".css"))       return "text/css";
  if (filename.endsWith(".js"))        return "application/javascript";
  if (filename.endsWith(".png"))       return "image/png";
  if (filename.endsWith(".gif"))       return "image/gif";
  if (filename.endsWith(".jpg"))       return "image/jpeg";
  if (filename.endsWith(".ico"))       return "image/x-icon";
  if (filename.endsWith(".xml"))       return "text/xml";
  if (filename.endsWith(".pdf"))       return "application/x-pdf";
  if (filename.endsWith(".zip"))       return "application/x-zip";
  if (filename.endsWith(".gz"))        return "application/x-gzip";
  return "text/plain";
} // getContentType()

bool handleFileRead(String path) {
  Serial.println("handleFileRead: " + path);
  if ((path == "/") && (server.argName(0) == "restart" && server.arg(0) == "true")) {
    Serial.println(F("requested reset from admin page!"));
    server.send(200, "text/plain", "Restarting!");
#ifdef NOT_DEBUG
    digitalWrite(LED_BUILTIN, HIGH);
#endif
    delay(200);
    wdt_reset();
    ESP.restart();
    while (1) {
      wdt_reset();
    }
    //WiFi.forceSleepBegin(); wdt_reset(); ESP.restart(); while (1)wdt_reset();
  }
  if (path.endsWith("/")) path += "index.htm";

  String contentType =  getContentType(path);
  String pathWithGz =   path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz)) path += ".gz";

    File file =         SPIFFS.open(path, "r");
    size_t sent =       server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
} // handleFileRead()

void handleFileUpload() {
  if (server.uri() != "/edit") return;

  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;

    Serial.println("handleFileUpload Name: " + filename);
    fsUploadFile =    SPIFFS.open(filename, "w");
    filename =        String();
    return;
  }
  if (upload.status == UPLOAD_FILE_WRITE) {
    //Serial.print("handleFileUpload Data: "); Serial.println(upload.currentSize);
    if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);

    return;
  }
  if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) fsUploadFile.close();
    Serial.print("handleFileUpload Size: ");
    Serial.println(upload.totalSize);
    return;
  }
} // handleFileUpload()

void handleFileDelete() {
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");

  String path = server.arg(0);
  Serial.println("handleFileDelete: " + path);
  if (path == "/")           return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))  return server.send(404, "text/plain", "FileNotFound");

  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
} // handleFileDelete()

void handleFileCreate() {
  if (server.args() == 0)    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if (path == "/")           return server.send(500, "text/plain", "BAD PATH");
  if (SPIFFS.exists(path))   return server.send(500, "text/plain", "FILE EXISTS");

  File file = SPIFFS.open(path, "w");
  if (file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
} // handleFileCreate()

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") output += ',';

    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  output += "]";
  server.send(200, "text/json", output);
} // handleFileList()
