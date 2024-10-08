#pragma region Prolog
/*******************************************************************
$CRT 08 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>Sensor.h<< 08 Okt 2024  09:47:35 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Description
/*
--EN--


--DE--

*/
#pragma endregion
#pragma region Includes
#include <HomeKit_Sensor.h>

/* Display + AHT-Sensor */
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Adafruit_AHTX0.h>


using namespace HBHomeKit;
using namespace HBHomeKit::Sensor;
#pragma endregion

#pragma region Config

// 0.96" I2C IIC 128x64 monochrome OLED Display
#define DISPLAY_ADDRESS 0x3C
Adafruit_SSD1306 display(128, 64, &Wire);

// AHTx0 Sensor
Adafruit_AHTX0 aht;

constexpr auto SensorType = ESensorType::Temperature_Humidity;

#pragma endregion

#pragma region HomeKit Config
CHost Host
(
  MakeControlUnit(),
  MakeContinuousReadSensorUnit(1000,
    [
      Temperature = CKalman1DFilterF{ .R = 8e-3f /* Measurement variance */ },
      Humidity    = CKalman1DFilterF{ .R = 8e-3f /* Measurement variance */ }
    ]() mutable
    {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

      CSensorInfo Info;
      Info.Temperature = Temperature.Update(temp.temperature);
      Info.Humidity = Humidity.Update(humidity.relative_humidity);
      return Info;
    }),
  MakeOnSensorChangedUnit([Tick = uint8_t(0)](const CSensorInfo& info) mutable
    {
      display.clearDisplay();
      ++Tick;

      char Buf[16];

      if(Tick & 1)
        display.fillCircle(124, 4, 3, WHITE);

      if(info.Temperature)
      {
        display.setFont(&FreeSans18pt7b);
        display.setCursor(15, 30);
        snprintf_P(Buf, sizeof(Buf), PSTR("%0.1f"), *info.Temperature);
        display.print(Buf);
        display.setCursor(display.getCursorX()+7, display.getCursorY()-10);
        display.drawCircle(display.getCursorX()-1, display.getCursorY() - 10, 3, WHITE);
        display.setFont(&FreeSans9pt7b);
        display.println(" C");
      }
      if(info.Humidity)
      {
        display.setFont(&FreeSans12pt7b);
        display.setCursor(38, 60);
        snprintf_P(Buf, sizeof(Buf), PSTR("%0.1f"), *info.Humidity);
        display.print(Buf);
        display.setFont(&FreeSans9pt7b);
        display.println(" %");
      }
      display.display();

    }),

  MakeContinuousEventRecorderUnit(60 * 5) // 5 minutes
);

/*
* @brief A HomeKit device service contains the basic information of the device
* @note The device name is also used to create the Bonjour service name.
*/
const CDeviceService Device
{
  {
    .DeviceName{"Sensor"}, // available as http://Sensor.local
    /*
    * ... more optional device information: You can read more about the CDeviceService in hb_homekit.h
    */
  }
};


/*
* @brief
*/
CSensorService SensorService(&Host, SensorType);


const CAccessory<3> Accessory
{
  {
    .category = homekit_accessory_category_sensor,
  },

  &Device
  , SensorService[0]
  , SensorService[1]
};

/*
* @brief HomeKit provides everything you need to interact with Apple HomeKit and host a homepage.
*/
CHomeKit HomeKit
(
  &Accessory
);

#pragma endregion


#pragma region SetupDisplay
void SetupDisplay()
{
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    while(true);
  }

  display.clearDisplay();

  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(WHITE);

  display.setCursor(6, 30);
  display.print("Starting...");
  display.display();
}
#pragma endregion

#pragma region SetupAHT
void SetupAHT()
{
  if(!aht.begin())
  {
    Serial.println("Didn't find AHTx0");
    while(true);
  }
}
#pragma endregion


#pragma region setup
void setup()
{
	Serial.begin(115200);
	delay(500); // Important for ESP32: Wait until the serial interface is ready
	Serial.println("\n\n\nEnter Setup");


  // Installs and configures everything for Sensor.
  InstallAndSetupSensor(HomeKit, SensorService, Host);

  // Adds a menu (WiFi) that allows the user to connect to a WiFi network.
  AddWiFiLoginMenu(HomeKit);

  // Adds standard menu items to the controller.
  AddStandardMenus(HomeKit);

  // Installs the action UI, required for garage/WiFi web-pages.
  InstallActionUI(HomeKit);

  SetupAHT();
  SetupDisplay();

  // Starts HomeKit
	if(!HomeKit.Setup())
	{
		ERROR("HomeKit setup failed");
		return;
	}

	INFO("Setup finished");
}

#pragma endregion

#pragma region loop
void loop()
{
	HomeKit.Loop();
}

#pragma endregion
