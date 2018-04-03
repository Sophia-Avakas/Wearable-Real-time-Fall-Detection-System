/*Program to send SMS from ESP8266 via IFTTT.
 * For complete detials visit: www.circuitdigest.com
 * EXAMPLE URL: (do not use This)
 *  https://maker.ifttt.com/trigger/ESP/with/key/b8h22xlElZvP27lrAXS3ljtBa0092_aAanYN1IXXXXX
 *  
 */
#include "SparkFunLSM6DS3.h"
#include "Wire.h"
#include "SPI.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#define BUZZER_PIN 5
#define BUTTON_CANCLLED 0
#define BUTTON_ALERT 4

const char* ssid = "Digiport Open!";
const char* password = "Silver&Gold";
const char* host = "maker.ifttt.com";
const int httpsPort = 443;

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
 
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_CANCLLED, INPUT);
  pinMode(BUTTON_ALERT, INPUT);

  //Call .begin() to configure the IMU
  myIMU.begin();
}

void loop() {
    //Get all parameters    
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
    digitalWrite(BUZZER_PIN,HIGH); 
    if (potentialFallCount>=101){ //user has not responded after 20s
      fallConfirmed=true;
      Serial.print("Fall is true!");
      digitalWrite(BUZZER_PIN,LOW);
      
      Serial.print("connecting to ");
      Serial.println(ssid);
      WiFi.begin(ssid, password);
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
    if (angleChange>=1.396 && angleChange<=1.745){ //if orientation change is between 80-100 degrees
      potentialFall=true;
      Serial.println("POTENTIAL FALL DETECTED!");
      trigger=false;
    }
    else{ //user regained normal orientation
      trigger=false;
    }
  }
  
  if (AM >= 2 ){ //if AM breaks upper threshold (2g)
    trigger = true;
    Serial.println("AM breaks threshold!");
  }

   if (digitalRead(BUTTON_ALERT) == HIGH) {
    
      Serial.print("connecting to ");
      Serial.println(ssid);
      WiFi.begin(ssid, password);
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
    
  delay(100);  
}

