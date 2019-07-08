/***********************************************************************
                        Récepteur TIC Linky
                        Mode historique & standard (in progress)

V03  : External SoftwareSerial. Tested OK on 07/03/18.
V04  : Replaced available() by new(). Tested Ok on 08/03/18.
V05  : Internal SoftwareSerial. Cf special construction syntax.
V06  : Separate compilation version.
V07  : Major update with simplifications, confirmed working and stable.  
To do : 
  - add retain flag on mqtt messages
  - add error handling for no data received. 

***********************************************************************/

/***************************** Includes *******************************/
#include <string.h>
#include <Streaming.h>
#include <SoftwareSerial.h>

#include "WiFi.h";
#include <PubSubClient.h>



/****************************** Defines *******************************/
#define CLy_Bds 1200      /* Transmission speed in bds */

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */




/****************************** Macros ********************************/
#ifndef TIMEOUT_IF_NO_TELEINFO
#define TIMEOUT_IF_NO_TELEINFO 60000
#endif

#ifndef MQTT_TOPIC_BASE_DEBUG
#define MQTT_TOPIC_BASE_DEBUG "exp/Linky_esp32/data/"
#endif


#define DEBUG true


#ifdef DEBUG
String debug_val;
#endif 

/****************************** Constants *****************************/
const uint8_t pin_LkyRx = 21;
const uint8_t pin_LkyTx = 11;   /* !!! Not used but reserved !!! * Do not use for anything else */



const char* ssid = "HUAWEI-C1A5";
const char* password =  "55966300";
const char* mqtt_server = "192.168.1.104";

RTC_DATA_ATTR int bootCount = 0;

WiFiClient espClient;
PubSubClient client(espClient);




                         

/************************* Global variables ***************************/
char msg[50];
char msg2[50];

String mqtt_topic_base_debug = MQTT_TOPIC_BASE_DEBUG;

unsigned char state = 0;

String label_;
String value;
char checksum1;
char checksum2;
String sn;    //Serial Number - corresponds to ADCO in historic mode
String meter_index_c;   //heures Creuses - corresponds to HCHC in historic mode
String meter_index_p;   //Heures Pleines - corresponds to HCHP in historic mode
String current;         //Current - corresponds to IINST in historic mode
String power;           //Power - Corresponds to PAPP in historic mode


/************************* Object instanciation ***********************/

SoftwareSerial Linky(pin_LkyRx, pin_LkyTx);


/****************************  Routines  ******************************/

void publish (String topic, String payload)
{
    if (!client.connected())
    {
        client.connect("arduinoClient");
        #ifdef DEBUG
        Serial.println("Preparing to publish - MQTT Connected");
        #endif
    } 
    
    client.publish(topic.c_str(), payload.c_str());
    Serial << "Just published:" << payload.c_str() << " on " << topic.c_str() << endl;
}


void publish_value(String sn, String name, String value, bool number=false)
{
    if (value != "")
    {
        if (number)
            value = String(value.toInt()); // Double convertion to remove leading zeros etc...
        else
            value = "\"" + value + "\"";
        publish("cm/teleinfo-" + sn + "/data/" + name, "{\"value\": " + value + "}");
    }
}




void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
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
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}



/******************************  Setup  *******************************/
void setup() {
  /* Initialise serial  link */
  Serial.begin(9600);
  Serial << "Serial started in setup loop -- " << endl ;
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  delay(3000);
  client.publish("Debug/boot", "test"); 

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  dtostrf(bootCount, 6, 1, msg); // Leave room for too large numbers!
  
  publish("debug",msg);

  Linky.begin(CLy_Bds);

  print_wakeup_reason();
  Serial << "Setup finished" << endl;
}

void loop() {
 if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  char c;
  if (Linky.available() > 0)
  { /* At least 1 char has been received */
    c = Linky.read() & 0x7f;   /* Read char, exclude parity */
    
    //Serial << c;
    switch (state)
      {
          // Start of Frame
          case 0:
              if (c == 0x02)
              {
                  //Serial << "Start bit!!!" << endl; 
                  state = 1;
                  sn = "";
                  meter_index_p = "";
                  meter_index_c = "";
                  #ifdef DEBUG
                  debug_val = "{";
                  #endif
              }
              break;
    
          // End of Frame
          case 6:
              if (c == 0x03)
              {
                  
                  #ifdef DEBUG
                  publish(mqtt_topic_base_debug + "debug", debug_val + "\"firmware_md5\": \"" + "fake_val_MD5" + "\"}");
                  Serial << "debug" << debug_val << "\"firmware_md5\": \"" <<  "\"}" << endl;
                  #endif
                  
                  if (sn != "")
                  {
                      publish_value(sn, "index_p", meter_index_p, true);
                      publish_value(sn, "index_c", meter_index_c, true);
                      publish_value(sn, "rms_current", current, true);
                      publish_value(sn, "apparent_power", power, true);
                      Serial << sn << " Index Pleine: " << meter_index_p << endl;
                      Serial << sn << " Index Creuse: " << meter_index_c << endl;
                      Serial << sn << " Rms_current: " << current << endl;
                      Serial << sn << " Apparent_power: " << power << endl;

                      #ifdef DEBUG
                      Serial.println("Going to sleep...");
                      #endif                       
                      ESP.deepSleep(65e6 - micros());  // ~ 60 secondes
                      // Wake-up is done by a hardware reset
                      #ifdef DEBUG
                      Serial.println("This should not be printed");
                      #endif
                  }
                  state = 0;
                  break;
              }
              // NO BREAK HERE !
              // If frame is not yet finished then we carry on with a new group
    
          // Start of Group
          case 1:
              //Serial << "(1)"; 
              if (c != 0x0a) // New Line
              {
                  #ifdef DEBUG
                  Serial <<  "error1" << String(c) << endl;
                  #endif
                  state = 0;
              } else {
                  state = 2;
                  label_ = "";
                  checksum1 = 0;
                  value = "";
              }
              break;
    
          // Label
          case 2:
              checksum1 += c;
              //Serial <<" f ";
              if ((c == 0x20) || (c == 0x09))
                  state = 3;
              else
                  label_ += String(c);
              break;
    
          // Value
          case 3:
              if ((c == 0x20) || (c == 0x09))
              {
                  checksum2 += c;
                  state = 4;
                  //Serial << "Going for the checksum" << endl << endl;
              }
              else
              {
                  checksum1 += c;
                  value += String(c);
              }
              break;
    
          // Checksum
          case 4:
              checksum1 = (checksum1 & 0x3F) + 0x20;
              checksum2 = (checksum2 & 0x3F) + 0x20;
              if ((checksum1 != c) && (checksum2 != c))
              {
                  #ifdef DEBUG
                  Serial <<  "checksum NOK: " << String(checksum1) << ' ' << String(checksum2) << ' ' << String(c) << ' ' << label_ << ' ' << value << endl;
                  #endif
                  state = 0;
              } else
                  state = 5;
              break;
    
          // End of Group
          case 5:
              if (c == 0x0D)
              {
                  //Serial << "End bit!!!" << endl << endl;
                  #ifdef DEBUG
                  debug_val += "\"" + label_ + "\": \"" + value + "\", ";
                  #endif
                  state = 6;
                  if (label_ == "ADCO")
                      sn = value;
                  if (label_ == "HCHC")
                      meter_index_c = value;
                  if (label_ == "HCHP")
                      meter_index_p = value;
                  if (label_ == "IINST")
                      current = value;
                  if (label_ == "PAPP")
                      power = value;
              }
              else
              {
                  #ifdef DEBUG
                    Serial <<  "error5" << endl;
                  #endif
                  state = 0;
              }
              break;
     }         




    
  }
}  
