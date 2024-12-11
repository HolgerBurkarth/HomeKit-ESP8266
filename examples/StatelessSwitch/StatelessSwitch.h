#pragma region Prolog
/*******************************************************************
$CRT 11 Dez 2024 : hb

$AUT Holger Burkarth
$DAT >>StatelessSwitch.h<< 11 Dez 2024  16:24:30 - (c) proDAD
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
#define SWITCHER_PIN  D7


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
CStatelessSwitchService StatelessSwitch
({
  .Name = "SlessSwitch",
  });

/*
* @brief HomeKit provides everything you need to interact with Apple HomeKit and host a homepage.
*/
CHomeKit HomeKit
(
  Device,
  homekit_accessory_category_programmable_switch,
  &StatelessSwitch
);

CPrgSwitchEventHandler gbEventHandler(SWITCHER_PIN, StatelessSwitch);

#pragma endregion


#pragma region setup
void setup()
{
	Serial.begin(115200);
	delay(500); // Important for ESP32: Wait until the serial interface is ready
	Serial.println("\n\n\nEnter Setup");

  // Installs and configures everything for Switcher.
  InstallVarsAndCmds(HomeKit);
  AddMenuItems(HomeKit);
  gbEventHandler.Setup(HomeKit);

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
