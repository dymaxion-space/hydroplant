//DYMXIØN HYDROPLANT V.0.3.0
//using cactus micro arduino compatible with wifi esp2866 by april brothers
//http://wiki.aprbrother.com/wiki/Cactus_Micro

#include <SoftwareSerial.h>
#include <Sleep_n0m1.h>
//download sleep library https://github.com/n0m1/Sleep_n0m1
//copy to arduino library folder: /Users/USERNAME/Documents/Arduino/libraries  (mac)
//import library: http://www.arduino.cc/en/guide/libraries#toc4 (howto)

#include <elapsedMillis.h>
//donwnload elapsedMillis library https://github.com/pfeerick/elapsedMillis
//copy to arduino library folder: /Users/USERNAME/Documents/Arduino/libraries  (mac)
//import library: http://www.arduino.cc/en/guide/libraries#toc4 (howto)

//elapsed millis
elapsedMillis pumpTimeElapsed; //elapsed time of pump cycle
elapsedMillis notificationTimeElapsed; //elapsed time of notification cycle

//sleep
Sleep sleep;
//time until next plant check
//6 hours = 21 600 000 milliseconds -> checks plant watering 4 times a day
//TODO change for testing and set properly before sealing watering system 
unsigned long sleepTime = 21600000; //set sleep time in ms, max sleep time is 49.7 days
boolean setSleep = true;
boolean sentNotification = false;

// Important!! We use pin 13 for enable esp8266  -> DO NOT CHANGE!
#define WIFI_ENABLE_PIN 13

//WIFI SETTINGS, 2.4 GHZ B-Mode!
//If conection is not working, check that your wireless router is configured to support B-mode
//Usually B/G/N-Mixed which should support all connection types (20Mhz seems to be preferred for B-Mode)
#define SSID "change-this"
#define PASS "change-me!"

//pushingbox device id, used to send email or mobile notifications (if water level is low -> refill needed)
#define DEVID "v06CCA7D5005850A"
//create an account on https://www.pushingbox.com (free) - you can push notifications to various services
//like e-mail, twitter, android and iphone notification

//DEBUG SETTINGS
#define DEBUG 1 //uncomment to enable debug

#ifdef DEBUG
#define DEBUG_PRINT(x)     Serial.print(x)
#define DEBUG_PRINTDEC(x)     Serial.print(x, DEC)
#define DEBUG_PRINTLN(x)  Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x) 
#endif


//custom sensors pin
int soilPin = A8; //soil sensor analog input
int waterPin = A6; //water level sensor analog input


//potentiometer controls
int soilPotPin = A1; //set the humidity limit with a potentiometer
int waterPotPin = A2; //set the duration of pump (needs some mapping)

//sensor power pins
int waterSensorPowerPin1 =  3; 
int waterSensorPowerPin2 = 5; 

int soilSensorPowerPin1 = 7; 
int soilSensorPowerPin2 = 9; 

int soilSense = 0; //stores soil moisture sensed value (0-1024)
int waterSense = 0; //stores water level sensed value (0-1024)
int waterLowLimit = 20; //stop pump -> send alert, make slightly higher then 0 to avoid jitter 
int soilMoistureLimit = 150; //change this according to your plant (0-100 is really dry, 350-500 is rather normal)
int waterPumpingLimit = 200;

unsigned long waterPumpDuration = 60000; //in ms, controlled via potentiometer (5000-6000 ms)
unsigned long notificationDuration = 14400000; //in ms -> adjust the duration in which interval you would like to receive a notification if plant needs water 

//led if wifi is ready
int ledWifiPin = 2;

//water warning light
int ledWaterPin = 6;

//water pump pin
int pumpPin = 21;
boolean startPumping = false; //used to determine if water pump is currently in use

//inital settings
int ledState = LOW;             // ledState used to set the LED
long previousMillis = 0;        // will store last time LED was updated

//blink timer if wifi connected
long interval = 1000;           // interval at which to blink (milliseconds)

//software serial to communicate with esp2866 wifi
char serialbuffer[100];//serial buffer for request command
int wifiConnected = 0;

SoftwareSerial mySerial(11, 12); // rx, tx

void setup()
{
    mySerial.begin(9600);//connection to ESP8266
    
    //output to console, if debug is set
    #ifdef DEBUG
      Serial.begin(9600); //serial debug
    #endif

    //pump pin setup
    pinMode(pumpPin, OUTPUT);
    digitalWrite(pumpPin, LOW); //disable pin 
    
    //leds setup
    pinMode(ledWifiPin, OUTPUT);
    pinMode(ledWaterPin, OUTPUT);
    digitalWrite(ledWaterPin, LOW);

    //power sensor pin setup
    pinMode(waterSensorPowerPin1, OUTPUT);
    pinMode(waterSensorPowerPin2, OUTPUT);
    pinMode(soilSensorPowerPin1, OUTPUT);
    pinMode(soilSensorPowerPin2, OUTPUT);
    digitalWrite(waterSensorPowerPin1, LOW);
    digitalWrite(waterSensorPowerPin2, HIGH);
    digitalWrite(soilSensorPowerPin1, LOW);
    digitalWrite(soilSensorPowerPin2, HIGH);

    //wifi defaults for cactus micro
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);
    delay(1000);//delay

    //set mode needed for new boards
    mySerial.println("AT+RST");
    delay(3000);//delay after mode change       
    mySerial.println("AT+CWMODE=1");
    delay(300);
    mySerial.println("AT+RST");
    delay(500);

}

boolean connectWifi() {     
 String cmd="AT+CWJAP=\"";
 cmd+=SSID;
 cmd+="\",\"";
 cmd+=PASS;
 cmd+="\"";
 //Serial.println(cmd);
 DEBUG_PRINTLN(cmd);
 mySerial.println(cmd);
           
 for(int i = 0; i < 20; i++) {
   DEBUG_PRINT(".");
   //Serial.print(".");
   if(mySerial.find("OK"))
   {
     wifiConnected = 1;
     break;
   }
   
   delay(50);
 }
 DEBUG_PRINTLN(
   wifiConnected ? 
   "OK, Connected to WiFi." :
   "Can not connect to the WiFi."
   );
 return wifiConnected;
}

void loop()
{



  //remove later if it still works without
  /*
  //output everything from ESP8266 to the Arduino Micro Serial output
  //TODO: check disable after working implementation
  while (mySerial.available() > 0) {
    Serial.write(mySerial.read());
  }

  //to send commands to the ESP8266 from serial monitor (ex. check IP addr)
  if (Serial.available() > 0) {
     //read from serial monitor until terminating character
     int len = Serial.readBytesUntil('\n', serialbuffer, sizeof(serialbuffer));

     //trim buffer to length of the actual message
     String message = String(serialbuffer).substring(0,len-1);
     
     DEBUG_PRINT("message: ");
     DEBUG_PRINT(message);
     DEBUG_PRINTLN("");
     //Serial.println("message: " + message);

     //check to see if the incoming serial message is an AT command
     if(message.substring(0,2)=="AT"){
       //make command request
       DEBUG_PRINTLN("COMMAND REQUEST");
       mySerial.println(message); 
     }//if not AT command, ignore
  }
  */


  //read soil & water sensors values
  soilSense = analogRead(soilPin);
  waterSense = analogRead(waterPin);
  //potentiometer controlled manually, the threshold
  soilMoistureLimit = analogRead(soilPotPin);
  waterPumpingLimit = analogRead(waterPotPin);
  waterPumpDuration = map(waterPumpingLimit, 0, 1023, 5000, 60000); //5 to 60 seconds, controlled with potentiometer
  
  
  //output to console
  DEBUG_PRINTLN("CUSTOM SENSOR VALUES------------------");
  DEBUG_PRINT("Soil humidity threshold: ");
  DEBUG_PRINTDEC(soilMoistureLimit);
  DEBUG_PRINTLN("");
  DEBUG_PRINT("Water Pumping threshold: ");
  DEBUG_PRINTDEC(waterPumpDuration);
  DEBUG_PRINTLN("");
  DEBUG_PRINT("Soil Sensor Value: ");
  DEBUG_PRINTDEC(soilSense);
  DEBUG_PRINTLN("");
  DEBUG_PRINT("Water Sensor Value: ");
  DEBUG_PRINTDEC(waterSense);
  DEBUG_PRINTLN("");

  //check if the plant needs water
  if (soilSense <= soilMoistureLimit) {
    //if there still is enough water, pump it
    if (waterSense > waterLowLimit) {

      //water is available -> reset notification
      sentNotification = false; 

      if (startPumping == false) {
        pumpTimeElapsed = 0; //reset counter once for this pump cycle
        startPumping = true; // cycle can only be triggered once
      }

      if (pumpTimeElapsed > waterPumpDuration) {
        startPumping = false; //reset pump
        digitalWrite(pumpPin, LOW);  //disable pump 
        DEBUG_PRINTLN("pumping stopped");
        gotoSleep(); //goto sleep after pumping is finished
      }
      else {
        digitalWrite(pumpPin, HIGH); //pump some water
        DEBUG_PRINTLN("currently pumping");
      }   
    }

    // otherwise send alert notification
    else {

      if (sentNotification == false) {
        notificationTimeElapsed = 0; //reset counter once for this notification cycle
        sentNotification = true; // cycle can only be triggered once
        sendWaterAlert();
      }

      if (notificationTimeElapsed > notificationDuration) {
        sentNotification = false; //reset notification
      }
  
      //blink water notification light
      blinkWaterLed();
    }
  }
  else {
    //plant needs no water, so just go to sleep
    gotoSleep();
  }

  //slow down output to read serial monitor in debug mode
  #ifdef DEBUG
    delay(1000); 
  #endif
 
}

void gotoSleep() {
  //disable leds
  digitalWrite(ledWaterPin, LOW);
  digitalWrite(ledWifiPin, LOW);

  //reset pump
  startPumping = false; //reset pump
  digitalWrite(pumpPin, LOW);  //disable pump 

  

  #ifdef DEBUG
    delay(2000); //delay to simulate sleep
  #else
    //sleep until next wake-up 
    DEBUG_PRINT("sleeping for: ");
    DEBUG_PRINTDEC(sleepTime);
    DEBUG_PRINTLN("");
    delay(100); //delay to allow serial to fully print before sleep
    sleep.pwrDownMode(); //set sleep mode
    sleep.sleepDelay(sleepTime); //sleep for: sleepTime
  #endif
  
}

void blinkWaterLed() {
  //enable water notification, blink water
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval) {
    // save the last time you blinked the LED 
    previousMillis = currentMillis;   

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    }
    else {
      ledState = LOW;
    }
    // set the LED with the ledState of the variable:
    digitalWrite(ledWaterPin, ledState);
  }
}

void sendWaterAlert() {
  //try to connect to wifi 
  if(!wifiConnected) {
    mySerial.println("AT");
    delay(1000);
    if(mySerial.find("OK")){
      //Serial.println("Module Test: OK");
      DEBUG_PRINTLN("Module Test: OK");
      connectWifi();
    } 
  }

  //if still not connected
  if(!wifiConnected) {
    delay(500);
    return;
    //turn off wifi notification led
    digitalWrite(ledWifiPin , LOW);
  }

  //if wifi is connected -> light notification led
  else {
    digitalWrite(ledWifiPin , HIGH);

      //create start command
  String startcommand = "AT+CIPSTART=\"TCP\",\"koga.cx\", 80";

  mySerial.println(startcommand);
  DEBUG_PRINTLN(startcommand);

   //test for a start error
   if(mySerial.find("Error")){
      DEBUG_PRINTLN("error on start");
      return;
   }
   
   //create the request command
   String sendcommand = "GET /pushingbox.php"; 
   sendcommand.concat("?devid=");
   sendcommand.concat(DEVID);
   sendcommand.concat("\r\n");
   //sendcommand.concat(" HTTP/1.0\r\n\r\n");
   
   DEBUG_PRINTLN(sendcommand);
   
   
   //send to ESP8266
   mySerial.print("AT+CIPSEND=");
   mySerial.println(sendcommand.length());
   
   //display command in serial monitor
   DEBUG_PRINT("AT+CIPSEND=");
   DEBUG_PRINTDEC(sendcommand.length());
   DEBUG_PRINTLN("");
   
   if(mySerial.find(">"))
   {
     DEBUG_PRINTLN(">");
     }else
     {
       mySerial.println("AT+CIPCLOSE");
       DEBUG_PRINTLN("connect timeout");
       delay(1000);
       return;
     }

   mySerial.print(sendcommand);
   delay(1000);
   mySerial.println("AT+CIPCLOSE");
   delay(5000);
   digitalWrite(ledWifiPin , LOW); //turn off wifi led after sending message
    
  }



}
