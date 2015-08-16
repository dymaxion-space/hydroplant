//DYMXIØN HYDROPLANT V.0.3.2
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
elapsedMillis senseTimeElapsed;

//sleep
Sleep sleep;
//time until next plant check
//6 hours = 21 600 000 milliseconds -> checks plant watering 4 times a day
//TODO change for testing and set properly before sealing watering system 
unsigned long sleepTime = 20000; //set sleep time in ms, max sleep time is 49.7 days
boolean setSleep = true;
boolean sentNotification = false;

// Important!! We use pin 13 for enable esp8266  -> DO NOT CHANGE!
#define WIFI_ENABLE_PIN 13

//WIFI SETTINGS, 2.4 GHZ B-Mode!
//If conection is not working, check that your wireless router is configured to support B-mode
//Usually B/G/N-Mixed which should support all connection types (20Mhz seems to be preferred for B-Mode)
#define SSID "hydroplant"
#define PASS "hydroplant"

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
int waterLowLimit = 50; //stop pump -> send alert, make slightly higher then 0 to avoid jitter 
int soilMoistureLimit = 150; //change this according to your plant (0-100 is really dry, 350-500 is rather normal)
int waterPumpingLimit = 200;

unsigned long waterPumpDuration = 60000; //in ms, controlled via potentiometer (5000-6000 ms)
unsigned long notificationDuration = 120000; //in ms -> adjust the duration in which interval you would like to receive a notification if plant needs water 
unsigned long senseTimeDuration = 2000; //only used as led blink timer notification

//led if wifi is ready
int ledPlantPin = 18;

//water warning light
int ledWaterPin = 10;

//water pump pin
int pumpPin = 21;
boolean startPumping = false; //used to determine if water pump is currently in use

//inital settings
int ledStatePlant = LOW; 
int ledStateWater = LOW;              // ledState used to set the LED
long previousMillisPlant = 0;        // will store last time LED was updated
long previousMillisWater = 0; 



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
    pinMode(ledPlantPin, OUTPUT);
    pinMode(ledWaterPin, OUTPUT);
    //digitalWrite(ledWaterPin, HIGH);

    //power sensor pin setup
    pinMode(waterSensorPowerPin1, OUTPUT);
    pinMode(waterSensorPowerPin2, OUTPUT);
    pinMode(soilSensorPowerPin1, OUTPUT);
    pinMode(soilSensorPowerPin2, OUTPUT);
    digitalWrite(waterSensorPowerPin1, LOW);
    digitalWrite(waterSensorPowerPin2, LOW);
    digitalWrite(soilSensorPowerPin1, LOW);
    digitalWrite(soilSensorPowerPin2, LOW);

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
   
   delay(1000);
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



  //read water sensor value
  digitalWrite(waterSensorPowerPin2, HIGH);
  waterSense = int(readSensorAverage(waterPin));
  digitalWrite(waterSensorPowerPin2, LOW);

  //read soil here only in debug mode, because not needed if there is no water
  #ifdef DEBUG
    digitalWrite(soilSensorPowerPin2, HIGH);
    soilSense = int(readSensorAverage(soilPin));
    digitalWrite(soilSensorPowerPin2, LOW);
  #endif

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
  DEBUG_PRINT("Water Sensor Value: ");
  DEBUG_PRINTDEC(waterSense);
  DEBUG_PRINTLN("");
  DEBUG_PRINT("Soil Sensor Value: ");
  DEBUG_PRINTDEC(soilSense);
  DEBUG_PRINTLN("");  
  
  #ifdef DEBUG
    delay(1000);
  #endif

  if (waterSense > waterLowLimit) {
    //has water available 

    //if not yet pumping, show sensing notification
    if (startPumping == false) {
        //-> read soil sensor once (average of 100 measurements)
        digitalWrite(soilSensorPowerPin2, HIGH);
        soilSense = int(readSensorAverage(soilPin));
        digitalWrite(soilSensorPowerPin2, LOW);

        senseTimeElapsed = 0;
        while(senseTimeElapsed < senseTimeDuration ){
            digitalWrite(ledWaterPin, LOW);
            blinkPlantLed(100);
            DEBUG_PRINTLN("blinkPlantLed");
        }  
    }

    if (soilSense <= soilMoistureLimit) {
      //give water
      //disable plant led
      //digitalWrite(ledPlantPin, LOW);


      //water is available -> reset notification
      sentNotification = false; 

      if (startPumping == false) {
        pumpTimeElapsed = 0; //reset counter once for this pump cycle
        startPumping = true; // cycle can only be triggered once
      }

      if (pumpTimeElapsed > waterPumpDuration) {
        DEBUG_PRINTLN("pumping stopped");
        gotoSleep(); //goto sleep after pumping is finished
      }
      else {
        digitalWrite(ledPlantPin, LOW);
        blinkWaterLed(50);
        digitalWrite(pumpPin, HIGH); //pump some water
        DEBUG_PRINTLN("currently pumping");
      }  



    }
    else {
        //no water needed
        gotoSleep();
    }

  }
  else {
    //no water!
      digitalWrite(pumpPin, LOW); //disable pumping
      startPumping = false;

      
      if (sentNotification == false) {
        notificationTimeElapsed = 0; //reset counter once for this notification cycle
        sentNotification = true; // cycle can only be triggered once
        sendWaterAlert();
      }

      if (notificationTimeElapsed > notificationDuration) {
        sentNotification = false; //reset notification
      }
      
  
      //blink water notification light
      digitalWrite(ledPlantPin, LOW);
      blinkWaterLed(250); 
  }
  

}

void gotoSleep() {
  //disable leds
  digitalWrite(ledPlantPin, LOW); //disable green light
  digitalWrite(ledWaterPin, HIGH); //show blue light during sleep (has power and water)

  //disable power for sensors - change current flow to prevent corrosion
  digitalWrite(waterSensorPowerPin2, LOW);
  digitalWrite(soilSensorPowerPin2, LOW);
  digitalWrite(waterSensorPowerPin1, HIGH);
  digitalWrite(soilSensorPowerPin1, HIGH);
  delay(20);
  digitalWrite(waterSensorPowerPin1, LOW);
  digitalWrite(soilSensorPowerPin1, LOW);

  //reset pump
  startPumping = false; //reset pump
  digitalWrite(pumpPin, LOW);  //disable pump 

  

  #ifdef DEBUG
    //sleep until next wake-up 
    DEBUG_PRINT("sleeping for: ");
    DEBUG_PRINTDEC(sleepTime);
    DEBUG_PRINTLN("");
    delay(sleepTime); //simulate sleep
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


#define NUM_READS 100
float readSensorAverage(int sensorpin){
   // read multiple values and sort them to take the mode

   int sortedValues[NUM_READS];
   for(int i=0;i<NUM_READS;i++){
     int value = analogRead(sensorpin);
     int j;
     if(value<sortedValues[0] || i==0){
        j=0; //insert at first position
     }
     else{
       for(j=1;j<i;j++){
          if(sortedValues[j-1]<=value && sortedValues[j]>=value){
            // j is insert position
            break;
          }
       }
     }
     for(int k=i;k>j;k--){
       // move all values higher than current reading up one position
       sortedValues[k]=sortedValues[k-1];
     }
     sortedValues[j]=value; //insert current reading
     
   }
   //return scaled mode of 10 values
   float returnval = 0;
   for(int i=NUM_READS/2-5;i<(NUM_READS/2+5);i++){
     returnval +=sortedValues[i];
   }
   returnval = returnval/10;
   //return returnval*1100/1023;
   return returnval;
}


void blinkWaterLed(int interval) {
  //enable water notification, blink water
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillisWater > interval) {
    // save the last time you blinked the LED 
    previousMillisWater = currentMillis;   

    // if the LED is off turn it on and vice-versa:
    if (ledStateWater == LOW) {
      ledStateWater = HIGH;
    }
    else {
      ledStateWater = LOW;
    }
    // set the LED with the ledState of the variable:
    digitalWrite(ledWaterPin, ledStateWater);
  }
}


void blinkPlantLed(int interval) {
  //enable water notification, blink water
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillisPlant > interval) {
    // save the last time you blinked the LED 
    previousMillisPlant = currentMillis;   

    // if the LED is off turn it on and vice-versa:
    if (ledStatePlant == LOW) {
      ledStatePlant = HIGH;
    }
    else {
      ledStatePlant = LOW;
    }
    // set the LED with the ledState of the variable:
    digitalWrite(ledPlantPin, ledStatePlant);
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
    connectWifi();
    delay(1000);
    //turn off wifi notification led
    digitalWrite(ledPlantPin , LOW);
  }

  //if wifi is connected -> light notification led
  else {
    digitalWrite(ledPlantPin , HIGH);

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
   digitalWrite(ledPlantPin , LOW); //turn off wifi led after sending message
    
  }



}
