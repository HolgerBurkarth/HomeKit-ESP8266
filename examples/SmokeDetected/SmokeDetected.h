#pragma region Prolog
/*******************************************************************
$CRT 12 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>SmokeDetected.h<< 12 Okt 2024  09:32:00 - (c) proDAD
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


using namespace HBHomeKit;
using namespace HBHomeKit::Sensor;
#pragma endregion

#pragma region Config

#define SMOKE_SENSOR_PIN D7


#pragma endregion

#pragma region HomeKit Config
CHost Host
(
  MakeControlUnit(),
  MakeContinuousReadEventUnit(
    250, // [ms] Determine new sensor values every quarter second
    [
    ]() mutable
    {
      CEventInfo Info;
      bool Detected = digitalRead(SMOKE_SENSOR_PIN);

      Info.Event = EEventType::SmokeDetected;
      Info.Value = static_value_cast(Detected ? HOMEKIT_SMOKE_DETECTED_YES : HOMEKIT_SMOKE_DETECTED_NO);

      return Info;
    })

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
CSensorService SensorService(&Host, EEventType::SmokeDetected);


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


#pragma region setup
void setup()
{
	Serial.begin(115200);
	delay(500); // Important for ESP32: Wait until the serial interface is ready
	Serial.println("\n\n\nEnter Setup");

  pinMode(SMOKE_SENSOR_PIN, INPUT);

  // Installs and configures everything for Sensor.
  InstallAndSetupSensor(HomeKit, SensorService, Host);

  // Adds a menu (WiFi) that allows the user to connect to a WiFi network.
  AddWiFiLoginMenu(HomeKit);

  // Adds standard menu items to the controller.
  AddStandardMenus(HomeKit);

  // Installs the action UI, required for garage/WiFi web-pages.
  InstallActionUI(HomeKit);


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
