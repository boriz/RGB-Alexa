#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

// ESP alexa library
//#define ESPALEXA_ASYNC
//#define ESPALEXA_DEBUG
#define ESPALEXA_MAXDEVICES 5
#include <Espalexa.h>

// Configuration file
#include "config.h"

// RGB strip is connected to pin 13
#define DO_PIN 13

// Callback functions prototype
void Callback_LightColor(EspalexaDevice* dev);
void Callback_Rainbow(EspalexaDevice* dev);

// Other global variables
Adafruit_NeoPixel strip = Adafruit_NeoPixel(150, DO_PIN, NEO_GRB + NEO_KHZ800);
Espalexa espalexa;
int display_mode = 0;
uint32_t light_color = 0;
uint32_t light_color_new = 0;

// Preferneces 
//uint16_t color;
//Preferences prefs;


/*************************** Sketch Code ************************************/
void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Setup start");

  // Initialize random generator
  randomSeed(analogRead(0));  
  
  // Initialize all pixels to 'off'
  strip.begin();

  // The whimpy power supply can't handle a full power, limit brightness a bit
  strip.setBrightness(127); // 0 = off; 255=max brightness
  allSet(0);
  strip.show(); 
 
  // Connect to WiFi access point.
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.printf("Connecting to %s: .", WLAN_SSID);

  // Connect to wifi with timeout
  unsigned long conn_timeout = millis();
  WiFi.setSleep(false);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while ( WiFi.status() != WL_CONNECTED ) 
  {
    int del = random(100, 500);
    delay(del);
    Serial.print(".");

    // Timeout?
    if ( (millis() - conn_timeout) > (30 * 1000) )
    {
      Serial.println();
      Serial.printf("Connect timeout. Status: %d; Rebooting \n", WiFi.status());             
      ESP.restart();
    }
  }
  Serial.println();
  Serial.print("WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  // Add alexa devices
  espalexa.addDevice("RGB light", Callback_LightColor, EspalexaDeviceType::color, 0);
  espalexa.addDevice("RGB rainbow", Callback_Rainbow, EspalexaDeviceType::onoff, 0);
  espalexa.begin();

  light_color_new = strip.Color(10, 10, 10);
    
    // Load defaults
  //prefs.begin("app_set", false);

  // Load clock color from FLASH
  //uint16_t color = prefs.getUInt("color", 0);
  
  // Close prefernces
  //prefs.end();

  Serial.println("Setup end");
}


// Main loop is running on core 1 by default, core 1 is dedicated for LED update logic
void loop() 
{
  if (display_mode == 0)
  {
    // light mode
    if (light_color != light_color_new)
    {
      colorWipe(light_color_new, 30);
      light_color = light_color_new;
    }
  }  

  strip.show();
  espalexa.loop();
  delay(10);
}


// Alexa Light color callback
void Callback_LightColor(EspalexaDevice* d) 
{   
  Serial.printf("Alexa Light callback. Device:%s; Bright:%d, Red:%d, Green:%d, Blue:%d \n", d->getName(), d->getValue(), d->getR(), d->getG(), d->getB());

  // Alexa sets odd colors when turing light on by using "on" command
  // Just remeber if we actualy got ON comnmand
  bool on_command = (d->getValue()== 255) && (d->getR() == 254) && (d->getG() == 221) && (d->getB() == 80);
  Serial.printf("ON command? %d \n", on_command);  
  
  // Limit the brightness
  uint8_t lim_bright = d->getValue();

  // Calcaulte display color based on brightness
  uint8_t lim_r = d->getR() * lim_bright / 255;
  uint8_t lim_g = d->getG() * lim_bright / 255;
  uint8_t lim_b = d->getB() * lim_bright / 255;

  Serial.printf("Colors=Red:%d, Green:%d, Blue:%d \n", lim_r, lim_g, lim_b);

  // Switching on
  if (on_command || display_mode != 0)
  {      
    Serial.println("Received Light command");    
    display_mode = 0;
    return;
  }

  // Trying to turn it off?
  if (d->getValue() == 0)
  {      
    Serial.println("Receive Light off command");
    light_color_new = strip.Color(0, 0, 0);
    return;     
  }

  // Just changing color.
  Serial.println("Update color");
  light_color_new = strip.Color(lim_r, lim_g, lim_b);
}


void Callback_Rainbow(EspalexaDevice* d) 
{
  ;
}


// ================================================================
// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) 
{
  for(uint16_t i=0; i<strip.numPixels(); i++) 
  {
      strip.setPixelColor(strip.numPixels() - i - 1, c);
      strip.show();
      delay(wait);
  }
}


// Set all pixels to the same color
void allSet(uint32_t c)
{
  for (int i=0; i < strip.numPixels(); i++) 
  {
    strip.setPixelColor(i, c);
  }
  strip.show();
}


// Running dots
void runningDots(uint32_t c, uint32_t dots, uint8_t wait) 
{
  for (int q=0; q < strip.numPixels(); q++) 
  {
    // All off
    for (int i=0; i < strip.numPixels(); i++) 
    {
      strip.setPixelColor(i, 0);
    }
    
    // Dots on
    for (int i=0; i < dots; i++) 
    {
      strip.setPixelColor(strip.numPixels() - (i + q) - 1, c);
    }
    strip.show();
   
    delay(wait);   
  }
}


// Rainbow effect
void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) 
  {
    for(i=0; i<strip.numPixels(); i++) 
    {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}


// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j < 256*5; j++) 
  { 
    // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) 
    {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}


//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) 
{
  for (int j=0; j<50; j++) 
  {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) 
    {
      for (int i=0; i < strip.numPixels(); i=i+3) 
      {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();
     
      delay(wait);
     
      for (int i=0; i < strip.numPixels(); i=i+3) 
      {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}


//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) 
  {     
    // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) 
    {
        for (int i=0; i < strip.numPixels(); i=i+3) 
        {
          strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
        }
        strip.show();
       
        delay(wait);
       
        for (int i=0; i < strip.numPixels(); i=i+3) 
        {
          strip.setPixelColor(i+q, 0);        //turn every third pixel off
        }
    }
  }
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) 
{
  if(WheelPos < 85) 
  {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) 
  {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } 
  else 
  {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
