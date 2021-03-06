// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Please use an Arduino IDE 1.6.8 or greater

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>

#include "config.h"
#include "SparkFunLSM6DS3.h"
#include "Wire.h"
#include "SPI.h"
#define BUZZER_PIN 5
#define BUTTON_CANCLLED 0
#define BUTTON_ALERT 4

static bool messagePending = false;
static bool messageSending = true;

static char* ssid = "UCInet Mobile Access";
static char* pass = "";
const char* host = "maker.ifttt.com";
const int httpsPort = 443;


static char *connectionString;


static int interval = INTERVAL;

float bx = 0; 
float by = 0; 
float bz = 256; //store reference upright acceleration
float BM = pow(pow(bx,2)+pow(by,2)+pow(bz,2),0.5);
boolean fallConfirmed = false; //stores if a fall has occurred and user not responding
boolean trigger=false; //stores if second trigger (upper threshold) has occurred
boolean potentialFall=false; //stores if a potential fall has occurred
boolean cancel=false; //stores whether a potential fall has been cancelle
int potentialFallCount=0; //stores the counts past a potential fall

LSM6DS3 myIMU; //Default constructor is I2C, addr 0x6B
 

//
//void blinkLED()
//{
//    digitalWrite(LED_PIN, HIGH);
//    delay(500);
//    digitalWrite(LED_PIN, LOW);
//}
//
//void initWifi()
//{
//    // Attempt to connect to Wifi network:
//    Serial.printf("Attempting to connect to SSID: %s.\r\n", ssid);
//
//    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
//    WiFi.begin(ssid, pass);
//    while (WiFi.status() != WL_CONNECTED)
//    {
//        // Get Mac Address and show it.
//        // WiFi.macAddress(mac) save the mac address into a six length array, but the endian may be different. The huzzah board should
//        // start from mac[0] to mac[5], but some other kinds of board run in the oppsite direction.
//        uint8_t mac[6];
//        WiFi.macAddress(mac);
//        Serial.printf("You device with MAC address %02x:%02x:%02x:%02x:%02x:%02x connects to %s failed! Waiting 10 seconds to retry.\r\n",
//                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ssid);
//        WiFi.begin(ssid, pass);
//        delay(10000);
//    }
//    Serial.printf("Connected to wifi %s.\r\n", ssid);
//}

void initTime()
{
    time_t epochTime;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    while (true)
    {
        epochTime = time(NULL);

        if (epochTime == 0)
        {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        }
        else
        {
            Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
            break;
        }
    }
}

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
void setup()
{

  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_CANCLLED, INPUT);
  pinMode(BUTTON_ALERT, INPUT);

  //Call .begin() to configure the IMU
  myIMU.begin();
  ////////
  
//    pinMode(LED_PIN, OUTPUT);

    initSerial();
    delay(2000);
    readCredentials();

   // initWifi();
    initTime();
    initSensor();
    
    /*
    * Break changes in version 1.0.34: AzureIoTHub library removed AzureIoTClient class.
    * So we remove the code below to avoid compile error.
    */
    // initIoThubClient();

    iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
    if (iotHubClientHandle == NULL)
    {
        Serial.println("Failed on IoTHubClient_CreateFromConnectionString.");
        while (1);
    }

    IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, NULL);
    IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, NULL);
    IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, twinCallback, NULL);
}

static int messageCount = 1;
void loop()
{


  float x = 0, y = 0, z = 0, gx = 0, gy = 0;
  float AM; 
  double angleChange = 0; 

  x = myIMU.readFloatAccelX();
  y = myIMU.readFloatAccelY();
  z = myIMU.readFloatAccelZ();
  gx = myIMU.readFloatGyroX();
  gy = myIMU.readFloatGyroY();

  Serial.print(" x = ");
  Serial.println(x, 4);
  Serial.print(" y = ");
  Serial.println(y, 4);
  Serial.print(" z = ");
  Serial.println(z, 4);
  Serial.print(" gx = ");
  Serial.println(gx, 4);
  Serial.print(" gy = ");
  Serial.println(gy, 4);

  AM = pow(pow(x,2)+pow(y,2)+pow(z,2),0.5);
  Serial.print(" AM = ");
  Serial.println(AM);
  angleChange = acos((x * bx+ y* by+ z* bz)/ AM/ BM);

 if (potentialFall==true && digitalRead(BUTTON_CANCLLED) == HIGH)
    { //if green button pushed during a fall alert, alert is cancelled
    cancel=true;
  }  
  
  if(cancel==true) //if user cancels alert, turn off alert
  {
    Serial.print('Fall is cancelled!');
    potentialFall=false;
    fallConfirmed=false;
    potentialFallCount=0;
    digitalWrite(BUZZER_PIN, LOW);
    cancel=false;
  }

  if (fallConfirmed==true){ //fall is confirmed
    digitalWrite(BUZZER_PIN, LOW); //turn off buzzer to save power
    potentialFallCount++;
  }

  if (potentialFall==true){ //in event of a fall detection, allow 20s for user to cancel fall alert
    Serial.println("--------------------------------------------------------------------------------------------fall detected!");   
    digitalWrite(BUZZER_PIN,HIGH); 
    if (potentialFallCount>=3){ //user has not responded after 20s
      fallConfirmed=true;
      Serial.print("Fall is true!");
      digitalWrite(BUZZER_PIN,LOW);
      
      Serial.print("connecting to ");
      Serial.println(ssid);
      WiFi.begin(ssid, pass);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    WiFiClientSecure client;
    Serial.print("connecting to ");
    Serial.println(host);
    if (!client.connect(host, httpsPort)) {
      Serial.println("connection failed");
      return;
    }
   
    String url = "/trigger/ESP/with/key/dceOzHP8dl_MtqqDF4WtYU";
    Serial.print("requesting URL: ");
    Serial.println(url);
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: BuildFailureDetectorESP8266\r\n" +
                 "Connection: close\r\n\r\n");
    Serial.println("request sent");
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    String line = client.readStringUntil('\n');
    Serial.println("reply was:");
    Serial.println("==========");
    Serial.println(line);
    Serial.println("==========");
    Serial.println("closing connection");
    
      potentialFall=false;
      potentialFallCount=0;
    }
   potentialFallCount++;
   Serial.println(potentialFallCount);   
 }
 
  if (trigger==true){
    angleChange=acos(((double)x*(double)bx+(double)y*(double)by+(double)z*(double)bz)/(double)AM/(double)BM);
    if (angleChange>=1.196 && angleChange<=1.945){ //if orientation change is between 80-100 degrees
      potentialFall=true;
      Serial.println("(((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((POTENTIAL FALL");
      trigger=false;
    }
    else{ //user regained normal orientation
      trigger=false;
    }
  }
  
  if (AM >= 1.3 ){ //if AM breaks upper threshold (2g)
    trigger = true;
    Serial.println("*****************************************************************************************************TRIGGER 1 ACTIVATED");
  }

   if (digitalRead(BUTTON_ALERT) == HIGH) {
    
      Serial.print("connecting to ");
      Serial.println(ssid);
      WiFi.begin(ssid, pass);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    WiFiClientSecure client;
    Serial.print("connecting to ");
    Serial.println(host);
    if (!client.connect(host, httpsPort)) {
      Serial.println("connection failed");
      return;
    }
   
    String url = "/trigger/ESP/with/key/dceOzHP8dl_MtqqDF4WtYU";
    Serial.print("requesting URL: ");
    Serial.println(url);
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: BuildFailureDetectorESP8266\r\n" +
                 "Connection: close\r\n\r\n");
    Serial.println("request sent");
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    String line = client.readStringUntil('\n');
    Serial.println("reply was:");
    Serial.println("==========");
    Serial.println(line);
    Serial.println("==========");
    Serial.println("closing connection");    
  }




  
    if (!messagePending && messageSending)
    {
        char messagePayload[MESSAGE_MAX_LEN];
        bool temperatureAlert = readMessage(messageCount, messagePayload);
        sendMessage(iotHubClientHandle, messagePayload, temperatureAlert);
        messageCount++;
        delay(interval);
    }
    IoTHubClient_LL_DoWork(iotHubClientHandle);
    delay(100);
}
