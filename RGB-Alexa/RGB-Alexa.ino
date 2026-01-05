#include <WiFi.h>
//#include <Preferences.h>
#include <FastLED.h>

// ESP alexa library
//#define ESPALEXA_ASYNC
//#define ESPALEXA_DEBUG
#define ESPALEXA_MAXDEVICES 5
#include <Espalexa.h>

// Configuration file
#include "config.h"

// RGB strip is connected to pin 13
#define LED_PIN 13
#define NUM_LEDS 300 

// Callbacks and function prototype
void Callback_LightColor(EspalexaDevice* dev);
void Callback_Rainbow(EspalexaDevice* dev);
void Callback_ChaseColor(EspalexaDevice* dev); // Chase light calback
void Task_Alexa (void * parameter); // Alexa task

// Other global variables
CRGB leds[NUM_LEDS];
Espalexa espalexa;
int display_mode = 0;
uint8_t rainbow_brightness = 255;
CRGB light_color = 0;
CRGB light_color_new = 0;
bool light_refresh_request = false;
CRGB chase_color = 0;
TaskHandle_t h_task_alexa;

// Preferneces 
//uint16_t color;
//Preferences prefs;


/*************************** Sketch Code ************************************/
void setup() 
{
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.println("Setup start");
  
  // Initialize all pixels to 'off'
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // The whimpy power supply can't handle a full power, limit brightness a bit
  FastLED.setBrightness(255); // 0 = off; 255 = max brightness
  FastLED.clear();
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
 
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
    delay(500);
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
  fill_solid(leds, NUM_LEDS, CRGB::Green); 
  FastLED.show();

  // Add alexa devices
  espalexa.addDevice("RGB light", Callback_LightColor, EspalexaDeviceType::color, 0);
  espalexa.addDevice("RGB rainbow", Callback_Rainbow, EspalexaDeviceType::onoff, 0);
  espalexa.addDevice("RGB chase", Callback_ChaseColor, EspalexaDeviceType::color, 0);
  espalexa.begin();

  light_color_new = CRGB(0, 0, 30);
    
    // Load defaults
  //prefs.begin("app_set", false);

  // Load clock color from FLASH
  //uint16_t color = prefs.getUInt("color", 0);
  
  // Close prefernces
  //prefs.end();

  // Create alexa task
  // It is not critical, so can run on protocol core (core 0)
  // Task, Name, stack, parameter, priority, handle, core
  xTaskCreatePinnedToCore(Task_Alexa, "Alexa", 10000, NULL, 1, &h_task_alexa, 0);

  Serial.println("Setup end");
}


// Main loop is running on core 1 by default, core 1 is dedicated for LED update logic
void loop() 
{
  if (display_mode == 0)
  {    
    // light mode
    if ( (light_color != light_color_new) || (light_refresh_request) )
    {
      Serial.println("Loop LightColor. New color");
      Light_ColorWipe(light_color_new, 10);
      light_color = light_color_new;
      light_refresh_request = false;
    }
    
    // Update LEDs periodically just in case
    FastLED.show();
    delay(100);
  } 
  else if (display_mode == 1) 
  {
    // Rainbow mode
    int effect = random(1, 4);
    
    switch(effect)
    {       
      case 1:
        // Chase with rainbow colors
        Serial.println("Loop Rainbow. Rainbow_TheaterChase");
        Rainbow_TheaterChase(30);
        break;
          
      case 2:
        // Standard rainbow
        Serial.println("Loop Rainbow. Rainbow");
        Rainbow(200);
        break;
          
      case 3:
        // Equal ranbow
        Serial.println("Loop Rainbow. Rainbow_Cycle");
        Rainbow_Cycle(30);
        break;
    }    
  }
  else if (display_mode == 2) 
  {
    // Chase mode
    Chase_Theater(chase_color, 30);
  }
  else
  {
    // Wrong mode
    display_mode = 0;
  }
  
  // Short delay
  delay(10);
}


// ================================================================
// Tasks
void Task_Alexa (void * parameter)
{
  Serial.printf("Alexa task. Core: %d; Freq: %d MHz \n", xPortGetCoreID(), getCpuFrequencyMhz());

  while (true)
  {
    espalexa.loop();
    delay(10);
  }
}


// ================================================================
// Callbacks
// Alexa Light color callback
void Callback_LightColor(EspalexaDevice* d) 
{   
  Serial.printf("LightColor. Callback. Device:%s; Bright:%d, Red:%d, Green:%d, Blue:%d \n", d->getName(), d->getValue(), d->getR(), d->getG(), d->getB());

  // Alexa sets odd colors when turing light on by using "on" command
  // Just remeber if we actualy got ON comnmand
  bool on_command = (d->getValue()== 255) && (d->getR() == 254) && (d->getG() == 221) && (d->getB() == 80);
  Serial.printf("LightColor. Callback. ON command? %d \n", on_command);  
  
  // Limit the brightness
  uint8_t lim_bright = d->getValue();

  // Calcaulte display color based on brightness
  uint8_t lim_r = d->getR() * lim_bright / 255;
  uint8_t lim_g = d->getG() * lim_bright / 255;
  uint8_t lim_b = d->getB() * lim_bright / 255;

  Serial.printf("LightColor. Callback. Colors=Red:%d, Green:%d, Blue:%d \n", lim_r, lim_g, lim_b);

  // Switching on
  if (on_command || (display_mode != 0))
  {      
    Serial.println("LightColor. Callback. Received Light ON command");
    light_color_new = CRGB(lim_r, lim_g, lim_b);
    light_refresh_request = true;
    display_mode = 0;
    return;
  }

  // Trying to turn it off?
  if (d->getValue() == 0)
  {      
    Serial.println("LightColor. Callback. Receive Light OFF command");
    light_color_new = CRGB(0, 0, 0);
    light_refresh_request = true;
    return;     
  }

  // Just changing color.
  light_color_new = CRGB(lim_r, lim_g, lim_b);
}


void Callback_Rainbow(EspalexaDevice* d) 
{
  Serial.printf("Rainbow. Callback. Device:%s; Bright:%d, Red:%d, Green:%d, Blue:%d \n", d->getName(), d->getValue(), d->getR(), d->getG(), d->getB());
  
  // Trying to turn it off?
  if (d->getValue() == 0)
  {      
    Serial.println("Rainbow. Callback. Receive Rainbow OFF command");
    light_color_new = CRGB(0, 0, 0);
    light_refresh_request = true;
    display_mode = 0;
    return;     
  }

  // On command or changing brighness, just update values
  rainbow_brightness = d->getValue();
  display_mode = 1;
}


void Callback_ChaseColor(EspalexaDevice* d) 
{   
  Serial.printf("ChaseColor. Callback. Device:%s; Bright:%d, Red:%d, Green:%d, Blue:%d \n", d->getName(), d->getValue(), d->getR(), d->getG(), d->getB());

  // Alexa sets odd colors when turing light on by using "on" command
  // Just remeber if we actualy got ON comnmand
  bool on_command = (d->getValue()== 255) && (d->getR() == 254) && (d->getG() == 221) && (d->getB() == 80);
  Serial.printf("ChaseColor. Callback. ON command? %d \n", on_command);  
  
  // Limit the brightness
  uint8_t lim_bright = d->getValue();

  // Calcaulte display color based on brightness
  uint8_t lim_r = d->getR() * lim_bright / 255;
  uint8_t lim_g = d->getG() * lim_bright / 255;
  uint8_t lim_b = d->getB() * lim_bright / 255;

  Serial.printf("ChaseColor. Callback. Colors=Red:%d, Green:%d, Blue:%d \n", lim_r, lim_g, lim_b);

  // Switching on
  if (on_command || (display_mode != 2))
  {      
    Serial.println("ChaseColor. Callback. Received Chase ON command");    
    chase_color = CRGB(lim_r, lim_g, lim_b);
    display_mode = 2;
    return;
  }

  // Trying to turn it off?
  if (d->getValue() == 0)
  {      
    Serial.println("ChaseColor. Callback. Receive Chase OFF command");
    light_color_new = CRGB(0, 0, 0);
    light_refresh_request = true;
    display_mode = 0;
    return;     
  }

  // Just changing color.
  chase_color = CRGB(lim_r, lim_g, lim_b);
}


// ================================================================
// Effects
// Fill the dots one after the other with a color
void Light_ColorWipe(CRGB c, uint8_t wait) 
{
  for(uint16_t i = 0; i < NUM_LEDS; i++) 
  {
      leds[NUM_LEDS - i - 1] = c;
      FastLED.show();
      delay(wait);
      if (display_mode != 0) 
      {
        Serial.println("Light_ColorWipe cancelled.");
        return;
      }
  }
}


// Rainbow effect
void Rainbow(uint8_t wait) 
{  
  for(int j = 0; j < 256; j++) 
  {
    for(int i = 0; i < NUM_LEDS; i++) 
    {
      leds[i] = Wheel((i+j) & 255);
    }
    FastLED.show();
    delay(wait);
    if (display_mode != 1) 
    {
      Serial.println("Rainbow cancelled.");
      return;
    }
  }
}


// Slightly different, this makes the rainbow equally distributed throughout
void Rainbow_Cycle(uint8_t wait) {
  for(int j=0; j < 256 * 5; j++) 
  { 
    // 5 cycles of all colors on wheel
    for(int i = 0; i < NUM_LEDS; i++) 
    {
      leds[i] = Wheel(((i * 256 / NUM_LEDS) + j) & 255);
    }
    FastLED.show();
    delay(wait);
    if (display_mode != 1) 
    {
      Serial.println("Rainbow_Cycle cancelled.");
      return;
    }
  }
}


// Theatre-style crawling lights with rainbow effect
void Rainbow_TheaterChase(uint8_t wait) {
  for (int j=0; j < 256; j++) 
  {     
    // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) 
    {
        // turn every third pixel on
        for (int i=0; i < NUM_LEDS; i=i+3) 
        {
          leds[i+q] = Wheel( (i+j) % 255);    
        }
        FastLED.show();       
        delay(wait);
        if (display_mode != 1) 
        {
          Serial.println("Rainbow_TheaterChase cancelled.");
          return;
        }

        // turn every third pixel off
        for (int i=0; i < NUM_LEDS; i=i+3) 
        {
          leds[i+q] = CRGB::Black;        
        }
    }
  }
}


// Theatre-style crawling lights.
void Chase_Theater(CRGB c, uint8_t wait) 
{
  for (int j=0; j < 10; j++) 
  {
    for (int q=0; q < 3; q++) 
    {
      // turn every third pixel on
      for (int i=0; i < NUM_LEDS; i=i+3) 
      {
        leds[i+q] = c;    
      }
      FastLED.show();   
      delay(wait);
      if (display_mode != 2) 
      {
        Serial.println("Chase_Theater cancelled.");
        return;
      }
  
      // turn every third pixel off
      for (int i=0; i < NUM_LEDS; i=i+3) 
      {
        leds[i+q] = CRGB::Black;        
      }
    }
  }
}



// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
CRGB Wheel(byte WheelPos) 
{
  if(WheelPos < 85) 
  {
   return CRGB(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) 
  {
   WheelPos -= 85;
   return CRGB(255 - WheelPos * 3, 0, WheelPos * 3);
  } 
  else 
  {
   WheelPos -= 170;
   return CRGB(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
