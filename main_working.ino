#include <ModbusMaster.h>
#include <DataBinder.h>
#include <RTClib.h>
#include <Wire.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>


// Modbus and GSM-related pins
#define MODBUS_DIR_PIN 22
#define MODBUS_RX_PIN 25
#define MODBUS_TX_PIN 26
#define MODBUS_SERIAL_BAUD 19200
#define GSM_RX 33
#define GSM_TX 32
#define GSM_RESET 21
#define ANALOG_PIN_0 36

//OLED SCREEN related pins
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // Reset pin (or -1 if sharing Arduino reset pin)
#define OLED_I2C_ADDRESS 0x3C  // Default I2C address for OLED display

// WiFi and email
#define WIFI_SSID "TCN-NAS-SYSTEM"
#define WIFI_PASSWORD "@TCNGRIDAUTOMATION"
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "007walix@gmail.com"
#define AUTHOR_PASSWORD "pgelhnyxbeohzvno"
#define RECIPIENT_EMAIL "waletosin090@gmail.com"

// MQTT broker details
const char* mqttServer = "tcniot.org.ng"; // Use a free broker or your own
const int mqttPort = 1883;                   // Default MQTT port
const char* mqttUser = "testdevice";                   // Optional (use if required by the broker)
const char* mqttPassword = "test@2024";               // Optional (use if required by the broker)
const char* clientId = "station@2024";

const char* firmwareURL = "https://raw.githubusercontent.com/Esp32roject/station/main/main_working.ino.bin";

// MQTT topics
const char* publishTopic = "test/test";
const char* subscribeTopic = "esp32/receive";

// Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// Create an instance of the display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// SD card-related pins
int sck = 18;    // SCK pin
int miso = 19;   // MISO pin
int mosi = 23;   // MOSI pin
int cs = 15;      // Chip select (CS) pin
/////////////////////////////////////////////////////// Daily function parameter
unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long period = 86400000;  //the value is a number of milliseconds

/////////////////////////////////////////////


// Registers for voltage and frequency
uint16_t data_register[2] = {0x0C25, 0x0BDB};
bool sd_card_write_stop = false;
// Initialize objects
ModbusMaster node;
DataBinder dataBinder;
File myFile;
SMTPSession smtp;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
RTC_DS3231 rtc; 

String  ddateTime;

int analog_value = 0;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

String generateFilename() {
    DateTime now = rtc.now(); // Get current date
    char filename[13];
    snprintf(filename, sizeof(filename), "/%04d%02d%02d.csv", now.year(), now.month(), now.day());
    return String(filename);
}

// Pre- and Post-transmission for Modbus
void modbusPreTransmission() {
  delay(100);
  digitalWrite(MODBUS_DIR_PIN, HIGH);
}

void modbusPostTransmission() {
  digitalWrite(MODBUS_DIR_PIN, LOW);
  delay(100);
}

void updateSerial()
{
  delay(300);
  while (Serial.available()) 
  {
    Serial2.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  //Serial.println(Serial2.available());
  while(Serial2.available()) 
  {
   ddateTime = Serial2.readString();
    
     //Serial.write(ddateTime);//Forward what Software Serial received to Serial Port
     Serial.print(ddateTime);//Forward what Software Serial received to Serial Port
    
  }
  
  ddateTime.remove(0, 19);
  ddateTime.replace("/", "-");
  ddateTime.replace(",", " ");
  ddateTime.replace("+04 0", "");
}

void adjustRTCDateTime(String dateTime) {
 Serial.print(dateTime);//Forward what Software Serial received to Serial Port  
int year, month, day, hour, minute, second, millisecond;

String val1 = dateTime.substring(1, 5);
String val2 = dateTime.substring(6, 8);
String val3 = dateTime.substring(9, 11);
String val4 = dateTime.substring(12, 14);
String val5 = dateTime.substring(15, 17);
String val6 = dateTime.substring(18, 20);
String val7 = dateTime.substring(21, 27);

year = val1.toInt();
month = val2.toInt();
day = val3.toInt();
hour = val4.toInt();
minute = val5.toInt();
second = val6.toInt();
millisecond = val7.toInt();

  // Print extracted values (for debugging)
  Serial.println("Adjusting RTC with the following values:");
  Serial.print("Year: ");
  Serial.println(year);
  Serial.print("Month: ");
  Serial.println(month);
  Serial.print("Day: ");
  Serial.println(day);
  Serial.print("Hour: ");
  Serial.println(hour);
  Serial.print("Minute: ");
  Serial.println(minute);
  Serial.print("Second: ");
  Serial.println(second);


  // Adjust the RTC time
  if( year > 0 && month > 0 && day > 0){
 rtc.adjust(DateTime(year, month, day, hour, minute, second));
  Serial.println("RTC adjusted successfully.");
  }
  
  
}

void displayTime(void) {
  DateTime now = rtc.now();
     
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);

  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.print(':');
  Serial.print((currentMillis % 1000), DEC);
  Serial.println();
  
 // delay(1000);

}

void RTC_Check(){
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
 else{
 if (rtc.lostPower()) {
  
  Serial.println("RTC lost power, lets set the time!");
   // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
   Serial2.println("AT+QLTS=2"); // enable the PSUTTZ notification
   updateSerial();
  adjustRTCDateTime(ddateTime);
  ddateTime ="";
  }else{
            // Get network time from GSM module
          Serial.println("Getting network time from GSM module...");
          Serial2.println("AT+QLTS=2");
          updateSerial();
          adjustRTCDateTime(ddateTime);
          ddateTime ="";
          //adjustRTCDateTime(Serial2.readString());

          }
 }
}

// Send email with the previous day's log file
void sendEmail(const String &filePath) {
    if (!SD.exists(filePath)) {
        Serial.println("File not found on SD card, skipping email.");
        return;
    }

    Session_Config config;
    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;
    config.login.email = AUTHOR_EMAIL;
    config.login.password = AUTHOR_PASSWORD;

    SMTP_Message message;
    message.enable.chunking = true;
    message.sender.name = "ESP Log Sender";
    message.sender.email = AUTHOR_EMAIL;
    message.subject = "Daily Log File";
    message.addRecipient("Recipient", RECIPIENT_EMAIL);
    message.html.content = "Attached is the daily log file.";
    message.html.charSet = "utf-8";
    message.html.transfer_encoding = Content_Transfer_Encoding::enc_qp;

    SMTP_Attachment attachment;
    attachment.file.path = filePath;
    attachment.file.storage_type = esp_mail_file_storage_type_sd;
    attachment.descr.filename = filePath.substring(1);
    attachment.descr.mime = "text/plain";
    attachment.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
    message.addAttachment(attachment);
    
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
    message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

    if (!smtp.connect(&config)) {
        Serial.printf("SMTP connection failed: %s\n", smtp.errorReason().c_str());
        return;
    }

    if (smtp.isLoggedIn()) {
      Serial.println("Successfully logged in.");
    } else {
      Serial.println("Connection established, but not logged in.");
    }

    if (!MailClient.sendMail(&smtp, &message)) {
        Serial.printf("Failed to send email: %s\n", smtp.errorReason().c_str());
    } else {
        Serial.println("Email sent successfully!");
    }
}

// Callback function when a message is received
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  for (int j = 0; j < length; j++) {
    Serial.print((char)payload[j]);
  }
  Serial.println();
}

void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
  if (status.success()) {
    Serial.println("----------------");
    Serial.printf("Messages sent successfully: %d\n", status.completedCount());
    Serial.printf("Messages failed to send: %d\n", status.failedCount());
    Serial.println("----------------\n");

    struct tm dt;
    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);
      Serial.printf("Message No: %d\n", i + 1);
      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      Serial.printf("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\n", result.recipients.c_str());
      Serial.printf("Subject: %s\n", result.subject.c_str());
      Serial.println("----------------\n");
    }
    smtp.sendingResult.clear();
  }
}

void reconnect() {
  // Loop until we're reconnected to MQTT broker
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(clientId, mqttUser, mqttPassword)) {
      Serial.println("connected!");
      // Subscribe to a topic
      client.subscribe(subscribeTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(". Trying again in 5 seconds...");
      delay(5000);
    }
  }
}

void checkForUpdates() {
  Serial.println("Checking for updates...");
  HTTPClient http;
  http.begin(firmwareURL);

  // Send GET request
  int httpCode = http.GET();
  if (httpCode == 200) { // HTTP OK
    size_t contentLength = http.getSize();
    if (contentLength > 0) {
      if (Update.begin(contentLength)) { // Begin OTA
        WiFiClient* stream = http.getStreamPtr();
        size_t written = Update.writeStream(*stream);
        if (written == contentLength) {
          Serial.println("Firmware update completed successfully!");
          if (Update.end()) {
            Serial.println("Rebooting...");
            ESP.restart();
          } else {
            Serial.println("Update failed!");
          }
        } else {
          Serial.println("Firmware update failed. Not all bytes were written.");
        }
      } else {
        Serial.println("Not enough space for OTA update.");
      }
    }
  } else {
    Serial.printf("HTTP GET failed with code: %d\n", httpCode);
  }
  http.end();
}

void setup() {
  // Start Serial communication
  Serial.begin(115200);
  Serial.println("Serial communication started...");
  delay(50);

   // Initialize the I2C bus on the custom ESP32 pins
  Wire.begin(16, 17);  // SDA = 16, SCL = 17

  // Initialize the display with the custom I2C address
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);  // Don't proceed, loop forever
  }
 
 display.clearDisplay();  // Clear the buffer
 display.display();  // Update the screen with the new data
 
  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Configure the MQTT client
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);


  // Set up GSM
  Serial2.begin(115200, SERIAL_8N1, GSM_RX, GSM_TX);
  pinMode(GSM_RESET, OUTPUT);
  digitalWrite(GSM_RESET, HIGH);
  Wire.begin(16,17);
  RTC_Check();
  delay(1000);

  // Initialize Modbus communication
  pinMode(MODBUS_DIR_PIN, OUTPUT);
  digitalWrite(MODBUS_DIR_PIN, LOW);
  Serial1.begin(MODBUS_SERIAL_BAUD, SERIAL_8E1, MODBUS_RX_PIN, MODBUS_TX_PIN);
  Serial1.setTimeout(200);
  node.begin(1, Serial1); 
  
  // Initialize Modbus node
  node.preTransmission(modbusPreTransmission);
  node.postTransmission(modbusPostTransmission);

  // Initialize SPI with custom pins
  SPI.begin(sck, miso, mosi, cs);

  // Initialize SD card using the defined CS pin
  if (!SD.begin(cs)) {
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  
    // Check and perform OTA update
  checkForUpdates();
  Serial.println("Setup complete.");
}

int readSwitch(){
  analog_value = analogRead(ANALOG_PIN_0);

 
  return analog_value                                                                                                ; //Read analog
}

void loop() {

  

    // existing code...
    currentMillis = millis();
    
    if (currentMillis - startMillis >= period) {
        if(startMillis > currentMillis) {
            startMillis = 0;
        }
        RTC_Check();
        delay(300);
        startMillis = currentMillis;
    }

    DateTime now = rtc.now();
    
    // Check if SD card logging is enabled
    if(sd_card_write_stop == false) {
        String filename = generateFilename();  // Generate file name for the current date

        // Open file to log data
        File file = SD.open(filename.c_str(), FILE_APPEND);
        if (!file) {
            Serial.println("Failed to open file for writing");
            ESP.restart();
            return;
        }

        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println(F("Saving..."));
        display.display();

        long ms = currentMillis % 1000;

        // Format timestamp
        String timeStamp = String(now.year()) + "-" +
                           String(now.month()) + "-" +
                           String(now.day()) + " " +
                           String(now.hour()) + ":" +
                           String(now.minute()) + ":" +
                           String(now.second()) + ":" +
                           String(ms);

                                   // Format timeX
        String timeX =     String(now.hour()) + ":" +
                           String(now.minute()) + ":" +
                           String(now.second()) + ":" +
                           String(ms);

        float frequency = 0;
        float current = 0;
        float voltage = 0;
        float mw = 0;
        float mvar = 0;
        float pf = 0;

        
        // Read Modbus data (voltage and frequency)
        for (int i = 0; i <= 1; i++) {
            uint8_t result;
            unsigned int data[2];

            result = node.readHoldingRegisters(data_register[i], 2);
            if (result == node.ku8MBSuccess) {
                data[0] = node.getResponseBuffer(0x00);
                data[1] = node.getResponseBuffer(0x01);

                if (data_register[i] == 0x0C25) {
                    frequency = dataBinder.bindf(data[1], data[0]);
                    Serial.print("Frequency: ");
                    Serial.println(frequency);
                }
                if (data_register[i] == 0x0BC1) {
                    current = dataBinder.bindf(data[1], data[0]);
                    Serial.print("current: ");
                    Serial.println(current);
                }
                if (data_register[i] == 0x0BD1) {
                    voltage = dataBinder.bindf(data[1], data[0]);
                    Serial.print("Voltage: ");
                    Serial.println(voltage);
                }
                if (data_register[i] == 0x0BF3) {
                    mw = dataBinder.bindf(data[1], data[0]);
                    Serial.print("mw: ");
                    Serial.println(mw);
                }
                if (data_register[i] == 0x0BFB) {
                    mvar = dataBinder.bindf(data[1], data[0]);
                    Serial.print("MVAR: ");
                    Serial.println(mvar);
                }
                    if (data_register[i] == 0x0C0B) {
                    pf = dataBinder.bindf(data[1], data[0]);
                    Serial.print("PF: ");
                    Serial.println(pf);
                }


            } else {
                Serial.print("Failed, Response Code: ");
                Serial.println(result, HEX);
                delay(1000);
            }
        }
         // Reconnect to MQTT broker if disconnected
          if (!client.connected()) {
            reconnect();
          }
          client.loop();

          // Publish a test message every 5 seconds
          
          if (((now.second())% 5) == 0) {
            
            String message = 
                    "{\"name\":\"Station_Name\",\"t\":\""  + timeX + "\"" + ", \"mw\":\"" + String(mw) + ", \"v\":\"" + String(voltage) 
                     + ", \"a\":\"" + String(current) + ", \"mx\":\"" + String(mvar) + ", \"pf\":\"" + String(pf) + ", \"f\":\"" + 
                     String(frequency) + "\"}";

             char data[150];
            message.toCharArray(data, (message.length() + 1));

            client.publish(publishTopic, data);
          }
        
        // Log data to SD card
        file.print(timeStamp); // Timestamp
        file.print(",");       // Separator
        file.print(frequency); // Frequency
        file.print(",");       // Separator
        file.print(current); // current
        file.print(",");       // Separator
        file.print(voltage); // voltage
        file.print(",");       // Separator
        file.print(mw); // mw
        file.print(",");       // Separator
        file.print(mvar); // mvar
        file.print(",");       // Separator
        file.println(pf); // mvar

        // Close the file after writing
        file.close();

        Serial.print("Logged: ");
        Serial.print(timeStamp);
        Serial.print(", ");
        Serial.print(frequency);
        Serial.print(", ");
        Serial.print(current);
        Serial.print(", ");
        Serial.print(voltage);
        Serial.print(", ");
        Serial.print(mw);
        Serial.print(", ");
        Serial.print(mvar);
        Serial.print(", ");
        Serial.println(pf);
    } else {
        Serial.println("Saving to SD card Stopped. Safely remove the SD card");
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println(F("Stopped"));
        display.display();
    }

    delay(50);
    
    int readvalue = readSwitch();
    if (readvalue > 700 && readvalue < 1000) {
        sd_card_write_stop = false;
    }
    if (readvalue > 1000) {
        sd_card_write_stop = true;
    }
        // Check for new day and send email with previous day's file
    
    if (now.hour() == 0 && now.minute() == 0) {
        String yesterdayFile = generateFilename();
        yesterdayFile.remove(yesterdayFile.length() - 5); // Remove today's date
        yesterdayFile += String(now.day() - 1) + ".csv";
        sendEmail(yesterdayFile);
        delay(60000); // Prevent multiple emails within the same minute
    }
 
}