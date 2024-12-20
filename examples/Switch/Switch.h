#pragma region Prolog
/*******************************************************************
$CRT 03 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>Switch.h<< 11 Dez 2024  15:39:39 - (c) proDAD
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
#include <HomeKit_Switch.h>

using namespace HBHomeKit;
using namespace HBHomeKit::Switch;
#pragma endregion

#pragma region PIN Config

/*
* @brief
*/
#define SWITCHER_PIN  D2


#pragma endregion

#pragma region HomeKit Config

/*
* @brief A HomeKit device service contains the basic information of the device
* @note The device name is also used to create the Bonjour service name.
*/
const CDeviceService Device
{
  {
    .DeviceName{"Switch"}, // available as http://Switch.local
    /*
    * ... more optional device information: You can read more about the CDeviceService in hb_homekit.h
    */
  }
};


/*
* @brief Switcher as a service for HomeKit
*/
CSwitchService Switcher
({
  .Name = "Switch",
#if 1
   .Setter = [](homekit_characteristic_t* pC, homekit_value_t value)
  {
    VERBOSE("Switch::Setter");
    if(modify_value(pC, value))
    {
      bool Stat = static_value_cast<bool>(value);
      digitalWrite(SWITCHER_PIN, Stat ? HIGH : LOW);
    }
  },
  #else
  .Getter = [](const homekit_characteristic_t* pC) -> homekit_value_t
  {
    VERBOSE("Switch::Getter");
    return static_value_cast<bool>(digitalRead(SWITCHER_PIN) == HIGH);
  }
  #endif
  });

/*
* @brief HomeKit provides everything you need to interact with Apple HomeKit and host a homepage.
*/
CHomeKit HomeKit
(
  Device,
  homekit_accessory_category_switch,
  &Switcher
);

#pragma endregion


#pragma region setup
void setup()
{
	Serial.begin(115200);
	delay(500); // Important for ESP32: Wait until the serial interface is ready
	Serial.println("\n\n\nEnter Setup");

  pinMode(SWITCHER_PIN, OUTPUT);


  // Installs and configures everything for Switcher.
  InstallVarsAndCmds(HomeKit);
  AddMenuItems(HomeKit);

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
