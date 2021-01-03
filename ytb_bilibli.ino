//YouTube-Bilibili-ESP8266-MAX7219/ytb_bilibli.ino
#include <ESP8266HTTPClient.h>
//#include <WiFi101.h>

// ----------------------------
// Standard Libraries
// ----------------------------

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>


// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <YoutubeApi.h>

#include <MD_MAX72xx.h>
#include <SPI.h>

#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }

// IP address for the ESP8266 is displayed on the scrolling display
// after startup initialisation and connected to the WiFi network.
//
// Connections for ESP8266 hardware SPI are:
// Vcc       3v3     LED matrices seem to work at 3.3V
// GND       GND     GND
// DIN        D7     HSPID or HMOSI 13
// CS or LD   D8     HSPICS or HCS 15
// CLK        D5     CLK or HCLK 14
//

// Library for connecting to the Youtube API

// Search for "youtube" in the Arduino Library Manager
// https://github.com/witnessmenow/arduino-youtube-api

#include <ArduinoJson.h>
// Library used for parsing Json from the API responses

// Search for "Arduino Json" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

//------- Replace the following! ------
char ssid[] = "******";       // your network SSID (name)
char password[] = "******";  // your network key
#define API_KEY "********"  // your google apps API Token
#define CHANNEL_ID "*******" // makes up the url of channel
char bilibiliURL[] = "http://api.allorigins.win/get?url=https://api.bilibili.com/x/relation/stat?vmid=********&jsonp=jsonp"; // your bilibili url
//------- ---------------------- ------

WiFiClientSecure client;
YoutubeApi api(API_KEY, client);

unsigned long timeBetweenRequests = 60000;
unsigned long nextRunTime;

long subs = 0;





// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //I am using the FC16_HW hardware
#define MAX_DEVICES 4

#define CLK_PIN   14 // or SCK
#define DATA_PIN  13 // or MOSI
#define CS_PIN    15 // or SS

// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Text parameters
#define CHAR_SPACING  1 // pixels between characters

// Global message buffers shared by Serial and Scrolling functions
#define BUF_SIZE  75
char message[BUF_SIZE] = "Pending";
bool newMessageAvailable = true;
int ySubscriberCount = 0;
int bSubscriberCount = 0;


void readSerial(void)
{
  static uint8_t  putIndex = 0;

  while (Serial.available())
  {
    message[putIndex] = (char)Serial.read();
    if ((message[putIndex] == '\n') || (putIndex >= BUF_SIZE-3))  // end of message character or full buffer
    {
      // put in a message separator and end the string
      message[putIndex] = '\0';
      // restart the index for next filling spree and flag we have a message waiting
      putIndex = 0;
      newMessageAvailable = true;
    }
    else
      // Just save the next char in next location
      message[putIndex++];
  }
}

void printText(uint8_t modStart, uint8_t modEnd, char *pMsg)
// Print the text string to the LED matrix modules specified.
// Message area is padded with blank columns after printing.
{
  uint8_t   state = 0;
  uint8_t   curLen;
  uint16_t  showLen;
  uint8_t   cBuf[8];
  int16_t   col = ((modEnd + 1) * COL_SIZE) - 1;

  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  do     // finite state machine to print the characters in the space available
  {
    switch(state)
    {
      case 0: // Load the next character from the font table
        // if we reached end of message, reset the message pointer
        if (*pMsg == '\0')
        {
          showLen = col - (modEnd * COL_SIZE);  // padding characters
          state = 2;
          break;
        }

        // retrieve the next character form the font file
        showLen = mx.getChar(*pMsg++, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);
        curLen = 0;
        state++;
        // !! deliberately fall through to next state to start displaying

      case 1: // display the next part of the character
        mx.setColumn(col--, cBuf[curLen++]);

        // done with font character, now display the space between chars
        if (curLen == showLen)
        {
          showLen = CHAR_SPACING;
          state = 2;
        }
        break;

      case 2: // initialize state for displaying empty columns
        curLen = 0;
        state++;
        // fall through

      case 3: // display inter-character spacing or end of message padding (blank columns)
        mx.setColumn(col--, 0);
        curLen++;
        if (curLen == showLen)
          state = 0;
        break;

      default:
        col = -1;   // this definitely ends the do loop
    }
  } while (col >= (modStart * COL_SIZE));

  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}



void setup() {

  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  // Required if you are using ESP8266 V2.5 or above
  client.setInsecure();

  // If you want to enable some extra debugging
  api._debug = true;



  // MX Part

    // Display initialisation
  mx.begin();
//  strcat(bilibiliURL, bilibiliID);
//  sprintf(bilibiliURL,"http://api.allorigins.win/get?url=https://api.bilibili.com/x/relation/stat?vmid=%d&jsonp=jsonp",bilibiliID);


  Serial.println(bilibiliURL);
}

void loop() {

  if (millis() > nextRunTime)  {
    if(api.getChannelStatistics(CHANNEL_ID))
    {
      Serial.println("---------Stats---------");
      Serial.print("Subscriber Count: ");
      Serial.println(api.channelStats.subscriberCount);
      Serial.print("View Count: ");
      Serial.println(api.channelStats.viewCount);
      Serial.print("Comment Count: ");
      Serial.println(api.channelStats.commentCount);
      Serial.print("Video Count: ");
      Serial.println(api.channelStats.videoCount);
      // Probably not needed :)
      //Serial.print("hiddenSubscriberCount: ");
      //Serial.println(api.channelStats.hiddenSubscriberCount);
      Serial.println("------------------------");
      if (ySubscriberCount < api.channelStats.subscriberCount){
        ySubscriberCount = api.channelStats.subscriberCount;
      }
    }
    if(true)
    {
      HTTPClient http;
      Serial.println("bilibili");//
      http.useHTTP10(true);
      http.begin(bilibiliURL);
      http.GET();                                        //Make the request

      // Parse response
      const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5) + 300;
      DynamicJsonDocument doc(capacity);
      deserializeJson(doc, http.getStream());
      const char* contents = doc["contents"];
      
      Serial.println(contents);
      // Read values
      DynamicJsonDocument data(capacity);
      deserializeJson(data, contents);
//      Serial.println(doc.as<char[]>());
      const char* tmp = data["data"];
      Serial.println(tmp);
      JsonObject bilibili_json = data["data"];
      int tmp_bilibili = bilibili_json["follower"];
      
      Serial.println(tmp_bilibili);
      if (bSubscriberCount < tmp_bilibili){
        bSubscriberCount = tmp_bilibili;
      }
      // Disconnect
      http.end();

    }
    nextRunTime = millis() + timeBetweenRequests;
  }

  
  sprintf(message, "Y%d", ySubscriberCount);
  printText(0, MAX_DEVICES-1, message);
  delay(10000);
  sprintf(message, "B%d", bSubscriberCount);
  printText(0, MAX_DEVICES-1, message);
  delay(10000);
}
