#include "arduino_secrets.h"
#include "thingProperties.h" // This library is required for function with the Arduino IoT Cloud

// Here are the library declarations needed for this project:

#include <FastLED.h> // Library for controlling LED strip

// Libraries for operating OLED display
#include <Wire.h> 
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Library for the DHT11 temperature and humidity sensor 
#include "DHT.h"

// The following are the pin declarations for the connected devices:
#define DHTPIN 33 // Temperature and humidity sensor
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define onBoardLed 2 

#define ledPin 27 // Data pin for the LED strip
#define numLeds 142
CRGB leds[numLeds];

#define soilMoistureSensorPin 35

#define waterPumpPin 12

// Sets the respective width and height of the display in pixels
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define fanPin 13
 
// These are the parameters required for running the fan with the proper PWM frequency
const int pwmFrequency = 25000; /* 25 KHz */
const int pwmChannel = 0;
const int pwmResolution = 10;
const int maxDutyCycle = (int)(pow(2, pwmResolution) - 1);

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  // Initialize serial and wait for port to open:
  Serial.begin(9600);
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(1500); 

   if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  delay(1500);
  display.clearDisplay();
  
  display.setTextSize(0.3);
  display.setTextColor(WHITE);
  
  pinMode(waterPumpPin, OUTPUT);
  
  pinMode(onBoardLed, OUTPUT);
  
  // Sets up the ARGB LED strip
  FastLED.addLeds<WS2812, ledPin, GRB>(leds, numLeds);
  FastLED.clear();
  FastLED.show(); 
  lightOn = false;
  
  dht.begin();
  
  ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
  /* Attach the LED PWM Channel to the GPIO Pin */
  ledcAttachPin(fanPin, pwmChannel);
  
  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

bool connectionOn() // Basic function used for reporting if the cloud is connected or not
{
  bool status; 
  
  if (ArduinoCloud.connected() == 0) // Cloud is not connected
    status = false; 
  else // Cloud is connected
    status = true;
  
  return status; 
}

class DataCollect // Reads the data of all the sensors 
{
  private:
    int drySoil = 4095; // These are the raw data values that the soil moisture sensor returns when it is submersed in wet and dry soil, and is used to map the percentages of the soil moisture
    int wetSoil = 1800; 
  public:
    float temperatureSensor()
    {
      temperature = dht.readTemperature();
      return temperature; 
    }
    
    float humiditySensor()
    {
      humidity = dht.readHumidity();
      return humidity;
    }
    
    int soilMoistureSensor()
    {
      int soilMoistureVal = analogRead(soilMoistureSensorPin);
      int soilMoisturePercentage = map(soilMoistureVal, wetSoil, drySoil, 100, 0); // Maps the soil moisture percentage on a scale of 0-100
      soilMoisture = soilMoisturePercentage;
      return soilMoisturePercentage;
    }
    
};

DataCollect readData; // Creates an object of the DataCollect class made above

class EnvironmentalControls
{
  public:
    void controlFan() // Controls fan speed based on temperatures and humidity recorded 
    {
      if (connectionOn() == false) // Establish fan control even if Arduino Cloud is not connected
      {
        if (readData.temperatureSensor() < 21)
        {
          ledcWrite(pwmChannel, 0);
          fanSpeed = 0;
        }
        else if ((readData.temperatureSensor() >= 21) && (readData.temperatureSensor() < 23))
        {
          ledcWrite(pwmChannel, maxDutyCycle / 2);
          fanSpeed = 50;
        }
        else if ((readData.temperatureSensor() >= 23) && (readData.temperatureSensor() < 26))
        {
          ledcWrite(pwmChannel, maxDutyCycle / 1.5);
          fanSpeed = 67;
        }
      }
      else if (fanScheduler.isActive()) // When the fan schedule is on, the fan adjusts it speed based on the temperature
      {
        if (readData.temperatureSensor() < 21)
        {
          ledcWrite(pwmChannel, maxDutyCycle /3);
          fanSpeed = 33;
        }
        else if ((readData.temperatureSensor() >= 21) && (readData.temperatureSensor() < 23))
        {
          ledcWrite(pwmChannel, maxDutyCycle / 2);
          fanSpeed = 50;
        }
        else if ((readData.temperatureSensor() >= 23) && (readData.temperatureSensor() < 25))
        {
          ledcWrite(pwmChannel, maxDutyCycle / 1.5);
          fanSpeed = 67;
        }
        else if (readData.temperatureSensor() >= 25)
        {
          ledcWrite(pwmChannel, maxDutyCycle);
          fanSpeed = 100;
        }
      }
      else if (readData.humiditySensor() > 80) // If the humidity is too great, the fan will kick start ventilating more air to reduce it  
      {
        ledcWrite(pwmChannel, maxDutyCycle / 2);
        fanSpeed = 50;
      }
      else // Turns fan off 
      {
        ledcWrite(pwmChannel, 0);
        fanSpeed = 0;
      }
     
    }
    
    // The next two functions work similarly, using the scheduler feature in the Arduino IoT Cloud. When the schedule parameters are on and the following two jobs should be active (lights and watering plants), the respective devices are turned on. Otherwise, the devices are turned off 
    void controlLights()
    {
      if (lightScheduler.isActive()) // Turns light on
      {
        lightOn = true;
        
        for (int i = 0; i != 142; i++)
        {
          leds[i] = CRGB(80, 40, 100);
          FastLED.show();
          delay(5);
        }
      }
      else // Turns light off 
      {
        lightOn = false;
        
        for (int i = 142; i != -1; i--)
        {
          leds[i] = CRGB(0, 0, 0);
          FastLED.show();
          delay(5);
        }
      }
    }
    
    void waterPlants() // Activates water pump to water the plants 
    {
      if (waterScheduler.isActive()) // Turns pump on
      {
        pumpOn = true;
        digitalWrite(waterPumpPin, HIGH);
      }
      else // Turns pump off 
      {
        pumpOn = false; 
        digitalWrite(waterPumpPin, LOW);
      }
    }
};

EnvironmentalControls environment;

class ScreenControls // Outputs to the OLED display 
{
  public: 
    void homeScreen()
    {
      display.setCursor(5, 0);
      display.print("Connectivity Status: ");
      display.println(connectionOn());
      
      display.setCursor(5, 20);
      display.print("Temperature(C): ");
      display.println(readData.temperatureSensor());
      
      display.setCursor(5, 40);
      display.print("Humidity(%): ");
      display.println(readData.humiditySensor());
      
      display.setCursor(5, 60);
      display.print("Soil Moisture(%)");
      display.println(readData.soilMoistureSensor());
    }
};

ScreenControls setScreen;

void outputData() // Calls on all the functions within the data collection class to be ran within the main part of the program
{
  readData.temperatureSensor();
  readData.humiditySensor();
  readData.soilMoistureSensor();
}

void runEnvironmentalControls() // Calls on all the functions in the environmental control class
{
  environment.controlFan();
  environment.controlLights();
  environment.waterPlants();
}

void loop()
{
  ArduinoCloud.update();
  timeRead = ArduinoCloud.getLocalTime(); // Reads local time
  
  outputData();
  
  runEnvironmentalControls();
  
  setScreen.homeScreen();
  
  display.display();
  delay(250);
  display.clearDisplay();
  delay(250);
    
  if (connectionOn() == false) // Blinks LED if not yet connected to cloud
  {
    digitalWrite(onBoardLed, LOW);
    delay(500);
    digitalWrite(onBoardLed, HIGH);
  }
  else if (connectionOn() == true)// Onboard LED stays on to indicated an online connection is active
    digitalWrite(onBoardLed, HIGH);  
}
void onTemperatureChange()  {
  // Add your code here to act upon Temperature change
}
/*
  Since FanScheduler is READ_WRITE variable, onFanSchedulerChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onFanSchedulerChange()  {
  // Add your code here to act upon FanScheduler change
}
/*
  Since LightScheduler is READ_WRITE variable, onLightSchedulerChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onLightSchedulerChange()  {
  // Add your code here to act upon LightScheduler change
}
/*
  Since WaterScheduler is READ_WRITE variable, onWaterSchedulerChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onWaterSchedulerChange()  {
  // Add your code here to act upon WaterScheduler change
}