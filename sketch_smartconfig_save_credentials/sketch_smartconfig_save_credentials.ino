#include "FS.h"
#include "esp_system.h"
#include <esp_wifi.h>
#include <string.h>
#include <WiFi.h>
#include <Preferences.h>  // WiFi storage

                        // BME280 stuff from here
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;  

WebServer server(80); 

float temperature, humidity, pressure, altitude;    // to here


  const  char* rssiSSID;       // NO MORE hard coded set AP, all SmartConfig
  const  char* password;
  String PrefSSID, PrefPassword;  // used by preferences storage

  int  WFstatus;
  int UpCount = 0;
  int32_t rssi;           // store WiFi signal strength here
  String getSsid;
  String getPass;
  String  MAC;

  // SSID storage
       Preferences preferences;  // declare class object
  // END SSID storage
  
void setup() {
  Serial.begin(115200);
  Serial.printf("\tWiFi Setup -- \n"  ); 

  bme.begin(0x76); // BME280 stuff

  wifiInit();       // get WiFi connected
  IP_info();
  MAC = getMacAddress();

  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  server.on("/", handle_OnConnect);   //BME280 stuff
  server.onNotFound(handle_NotFound);  // -

  server.begin();                       // -
  Serial.println("HTTP server started"); // -

  delay(2000);
} //  END setup()
  
void loop()
{
  if ( WiFi.status() ==  WL_CONNECTED )      // Main connected loop

  { 
    
    // ANY MAIN LOOP CODE HERE
    // 
    server.handleClient();  // BME280 stuff

  }   // END Main connected loop()
  else
  {            // WiFi DOWN

     //  wifi down start LED flasher here

         WFstatus = getWifiStatus( WFstatus );

     WiFi.begin( PrefSSID.c_str() , PrefPassword.c_str() );
     int WLcount = 0;
     while (WiFi.status() != WL_CONNECTED && WLcount < 200 )
     {
      delay( 100 );
         Serial.printf(".");

         if (UpCount >= 60)  // keep from scrolling sideways forever
         {
            UpCount = 0;
               Serial.printf("\n");
         }
         ++UpCount;
         ++WLcount;
     }

    if( getWifiStatus( WFstatus ) == 3 )   //wifi returns
    { 
    // stop LED flasher, wifi going up
    }
     delay( 1000 );
  }  // END WiFi down 
} // END loop()
  
void wifiInit()  // 
{
   WiFi.mode(WIFI_AP_STA);   // required to read NVR before WiFi.begin()

   // load credentials from NVR, a little RTOS code here
   wifi_config_t conf;
   esp_wifi_get_config(WIFI_IF_STA, &conf);  // load wifi settings to struct comf
   rssiSSID = reinterpret_cast<const char*>(conf.sta.ssid);
   password = reinterpret_cast<const char*>(conf.sta.password);

      //Serial.printf( "%s\n", rssiSSID );
      //Serial.printf( "%s\n", password );

   // Open Preferences with "wifi" namespace. Namespace is limited to 15 chars
   preferences.begin("wifi", false);
       PrefSSID          =  preferences.getString("ssid", "none");      //NVS key ssid
       PrefPassword  =  preferences.getString("password", "none");  //NVS key password
   preferences.end();

   // keep from rewriting flash if not needed
   if( !checkPrefsStore() )     // see is NV and Prefs are the same
   {              // not the same, setup with SmartConfig
      if( PrefSSID == "none" )  // New...setup wifi
      {
        initSmartConfig(); 
        delay( 3000);
        ESP.restart();   // reboot with wifi configured
      }
   } 

   // I flash LEDs while connecting here

   WiFi.begin( PrefSSID.c_str() , PrefPassword.c_str() );

   int WLcount = 0;
   while (WiFi.status() != WL_CONNECTED && WLcount < 200 ) // can take > 100 loops depending on router settings
   {
     delay( 100 );
        Serial.printf(".");
     ++WLcount;
   }
  delay( 3000 );

  //  stop the led flasher here

  }  // END wifiInit()

// match WiFi IDs in NVS to Pref store,  assumes WiFi.mode(WIFI_AP_STA);  was executed
bool checkPrefsStore()   
{
    bool val = false;
    String NVssid, NVpass, prefssid, prefpass;

    NVssid = getSsidPass( "ssid" );
    NVpass = getSsidPass( "pass" );

    // Open Preferences with my-app namespace. Namespace name is limited to 15 chars
    preferences.begin("wifi", false);
        prefssid  =  preferences.getString("ssid", "none");     //NVS key ssid
        prefpass  =  preferences.getString("password", "none"); //NVS key password
    preferences.end();

    if( NVssid.equals(prefssid) && NVpass.equals(prefpass) )
      { val = true; }

  return val;
}

// optionally call this function any way you want in your own code
// to remap WiFi to another AP using SmartConfig mode.   Button, condition etc.. 
void initSmartConfig() 
{
   // start LED flasher
  int loopCounter = 0;

  WiFi.mode( WIFI_AP_STA );       //Init WiFi, start SmartConfig
      Serial.printf( "Entering SmartConfig\n" );

  WiFi.beginSmartConfig();

  while (!WiFi.smartConfigDone()) 
  {
     // flash led to indicate not configured
          Serial.printf( "." );
     if( loopCounter >= 40 )  // keep from scrolling sideways forever
     {
         loopCounter = 0;
         Serial.printf( "\n" );
     }
     delay(600);
    ++loopCounter;
  }
  loopCounter = 0;

  // stopped flasher here

   Serial.printf("\nSmartConfig received.\n Waiting for WiFi\n\n");
   delay(2000 );
    
  while( WiFi.status() != WL_CONNECTED )      // check till connected
  { 
    delay(500);
  }
  IP_info();  // connected lets see IP info

  preferences.begin("wifi", false);      // put it in storage
     preferences.putString( "ssid"         , getSsid);
     preferences.putString( "password", getPass);
  preferences.end();

    delay(300);
}  // END SmartConfig()

void IP_info()
{
   getSsid = WiFi.SSID();
   getPass = WiFi.psk();
   rssi = getRSSI(  rssiSSID );
   WFstatus = getWifiStatus( WFstatus );
   MAC = getMacAddress();

      Serial.printf( "\n\n\tSSID\t%s, ", getSsid.c_str() );
      Serial.print( rssi);  Serial.printf(" dBm\n" );  // printf??
      Serial.printf( "\tPass:\t %s\n", getPass.c_str() ); 
      Serial.print( "\n\n\tIP address:\t" );  Serial.print(WiFi.localIP() );
      Serial.print( " / " );
      Serial.println( WiFi.subnetMask() );
      Serial.print( "\tGateway IP:\t" );  Serial.println( WiFi.gatewayIP() );
      Serial.print( "\t1st DNS:\t" );     Serial.println( WiFi.dnsIP() );
      Serial.printf( "\tMAC:\t\t%s\n", MAC.c_str() );
}

int getWifiStatus( int WiFiStatus  )
{
  WiFiStatus = WiFi.status();
  Serial.printf("\tStatus %d",  WiFiStatus );
  switch( WiFiStatus )
  {
    case WL_IDLE_STATUS :                         // WL_IDLE_STATUS     = 0,
          Serial.printf(", WiFi IDLE \n");
          break;
    case WL_NO_SSID_AVAIL:                        // WL_NO_SSID_AVAIL   = 1,
          Serial.printf(", NO SSID AVAIL \n");
          break;
    case WL_SCAN_COMPLETED:                       // WL_SCAN_COMPLETED  = 2,
          Serial.printf(", WiFi SCAN_COMPLETED \n");
          break;
    case WL_CONNECTED:                            // WL_CONNECTED       = 3,
          Serial.printf(", WiFi CONNECTED \n");
          break;
    case WL_CONNECT_FAILED:                       // WL_CONNECT_FAILED  = 4,
          Serial.printf(", WiFi WL_CONNECT FAILED\n"); 
          break;
    case WL_CONNECTION_LOST:                      // WL_CONNECTION_LOST = 5,
          Serial.printf(", WiFi CONNECTION LOST\n");
          WiFi.persistent(false);                 // don't write FLASH
          break;
    case WL_DISCONNECTED:                         // WL_DISCONNECTED    = 6
          Serial.printf(", WiFi DISCONNECTED ==\n");
          WiFi.persistent(false);                 // don't write FLASH when reconnecting
          break;
  }
  return WiFiStatus;
}
// END getWifiStatus()

// Get the station interface MAC address.
// @return String MAC
String getMacAddress(void)
{
    WiFi.mode(WIFI_AP_STA);                    // required to read NVR before WiFi.begin()
    uint8_t baseMac[6];
    esp_read_mac( baseMac, ESP_MAC_WIFI_STA ); // Get MAC address for WiFi station
    char macStr[18] = { 0 };
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    return String(macStr);
}
// END getMacAddress()


// Return RSSI or 0 if target SSID not found
// const char* SSID = "YOUR_SSID";  // declare in GLOBAL space
// call:  int32_t rssi = getRSSI( SSID );
int32_t getRSSI( const char* target_ssid ) 
{
  byte available_networks = WiFi.scanNetworks();

  for (int network = 0; network < available_networks; network++) 
  {
    if ( strcmp(  WiFi.SSID( network).c_str(), target_ssid ) == 0) 
    {
      return WiFi.RSSI( network );
    }
  }
  return 0;
} //  END  getRSSI()


// Requires; #include <esp_wifi.h>
// Returns String NONE, ssid or pass arcording to request 
// ie String var = getSsidPass( "pass" );
String getSsidPass( String s )
{
  String val = "NONE";  // return "NONE" if wrong key sent
  s.toUpperCase();
  if( s.compareTo("SSID") == 0 )
  {
     wifi_config_t conf;
     esp_wifi_get_config( WIFI_IF_STA, &conf );
     val = String( reinterpret_cast<const char*>(conf.sta.ssid) );
  }
  if( s.compareTo("PASS") == 0 )
  {
     wifi_config_t conf;
     esp_wifi_get_config( WIFI_IF_STA, &conf );
     val = String( reinterpret_cast<const char*>(conf.sta.password) );
  }
 return val;
}


// BME280 stuff

void handle_OnConnect() {
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  server.send(200, "text/html", SendHTML(temperature,humidity,pressure,altitude)); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float temperature,float humidity,float pressure,float altitude){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>ESP32 Weather Station</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<div id=\"webpage\">\n";
  ptr +="<h1>ESP32 Weather Station</h1>\n";
  ptr +="<p>Temperature: ";
  ptr +=temperature;
  ptr +="&deg;C</p>";
  ptr +="<p>Humidity: ";
  ptr +=humidity;
  ptr +="%</p>";
  ptr +="<p>Pressure: ";
  ptr +=pressure;
  ptr +="hPa</p>";
  ptr +="<p>Altitude: ";
  ptr +=altitude;
  ptr +="m</p>";
  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}
