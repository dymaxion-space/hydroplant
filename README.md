# DYMAX.IØN HYDROPLANT
Arduino compatible automatic watering system for plants with wifi push notification.
Using Cactus micro arduino by April Brothers: [http://wiki.aprbrother.com/wiki/Cactus_Micro](http://wiki.aprbrother.com/wiki/Cactus_Micro)

Code should be usable with any Arduino compatible board using ESP8266. But not tested. 

##Functionality
* soil sensor to measure soil moisture of the plant
* water level sensor to measure if there is still water available
* pump controlled via cactus micro -> if soil is to dry and needs watering
* potentiometer to set needed soil moisture for the plant
* notification via www.pushingbox.com if water supply needs refill (e-mail, twitter, prowl, notifymyandroid)
* sleep mode to reduce power consumption
* various parameters for pumping duration, sleeping time, nofification interval

##Dependencies
**elapsedMillis** 
[https://github.com/pfeerick/elapsedMillis](https://github.com/pfeerick/elapsedMillis)
Used to measure pumping duration

**Sleepn0m1**
[https://github.com/n0m1/Sleep_n0m1](https://github.com/n0m1/Sleep_n0m1)
Used for sleeping mode

##Versions
###0.2.2
* Fixes Major Bug: Pump runs during sleep -> oh, it's wet now, baby!

##TODO
* Add Photos
* Add Schematics