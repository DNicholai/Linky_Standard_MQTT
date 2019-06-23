/***********************************************************************
                        Récepteur TIC Linky
                        Mode historique

V03  : External SoftwareSerial. Tested OK on 07/03/18.
V04  : Replaced available() by new(). Tested Ok on 08/03/18.
V05  : Internal SoftwareSerial. Cf special construction syntax.
V06  : Separate compilation version.

***********************************************************************/

/***************************** Includes *******************************/
#include <string.h>
#include <Streaming.h>
#include "LinkyHistTIC.h"

#include "EspMQTTClient.h"

/****************************** Defines *******************************/



/****************************** Constants *****************************/
const uint8_t pin_LkyRx = 21;
const uint8_t pin_LkyTx = 11;   /* !!! Not used but reserved !!! 
                                  * Do not use for anything else */

/************************* Global variables ***************************/




/************************* Object instanciation ***********************/
LinkyHistTIC Linky(pin_LkyRx, pin_LkyTx);

EspMQTTClient client(
  "Bbox-3CD720FE",
  "352D6D11199DD61929D2CDF119E511",
  "192.168.1.100",  // MQTT Broker server ip
  //"MQTTUsername",   // Can be omitted if not needed
  //"MQTTPassword",   // Can be omitted if not needed
  "TestClient",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

/****************************  Routines  ******************************/




/******************************  Setup  *******************************/
void setup()
  {

  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
  client.enableLastWillMessage("TestClient/lastwill", "I am going offline");  // You can activate the retain flag by setting the third parameter to true


  /* Initialise serial link */
  Serial.begin(9600);

  /* Initialise the Linky receiver */
  Linky.Init();

  Serial << F("Bonjour") << endl;
  }






/******************************* Loop *********************************/
void loop()
  {

  client.loop();
  
  Linky.Update();

        client.publish("Linky/EAST", String(Linky.EASTv()), 1); // You can activate the retain flag by setting the third parameter to true
        client.publish("Linky/SINSTS", String(Linky.SINSTSv()), 1); // You can activate the retain flag by setting the third parameter to true
        
      sleep(2000);
  };


// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe("mytopic/test", [](const String & payload) {
    Serial.println(payload);
  });
 }
