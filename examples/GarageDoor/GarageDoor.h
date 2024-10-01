#pragma region Prolog
/*******************************************************************
$CRT 01 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>GarageDoor.h<< 01 Okt 2024  09:00:26 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Description
/*
--EN--
This example shows a garage door opener that can be controlled by
Apple HomeKit. The device uses the same function as a traditional
button to open the door. An ultrasonic distance sensor monitors
the position of the door to tell Apple HomeKit whether the door
is closed, open, or half open.

New or different functions can be configured via CHost, derived
from CUnitBase, so that only a small amount of programming is
required, e.g. to detect the door position via limit switches or
to start the door motor other than by a pulse.


--DE--
Dieses Beispiel zeigt einen Garagentoröffner, der über Apple HomeKit
gesteuert werden kann. Das Gerät verwendet die gleiche Funktion wie
ein herkömmlicher Taster, um das Tor zu öffnen. Ein
Ultraschall-Abstandssensor überwacht die Position des Tores, um
Apple HomeKit mitzuteilen, ob das Tor geschlossen, geöffnet oder
in einer halb geöffneten Position ist.

Neue oder abweichende Funktionen können, abgeleitet aus CUnitBase,
über CHost konfiguriert werden, so dass nur ein geringer
Programmieraufwand erforderlich ist, um z.B. die Torposition über
Endschalter zu ermitteln oder den Tormotor abweichend von einem
Impuls zu starten.

*/
#pragma endregion
#pragma region Includes
#include <HomeKit_GarageDoorOpener.h>

using namespace HBHomeKit;
using namespace HBHomeKit::GarageDoorOpener;
#pragma endregion

#pragma region PIN Config

/*
* @brief This pin is used to start the door motor by pulling the trigger pin HIGH for 100 ms.
* @note Make sure that this pin is set to LOW during booting so that
*       no trigger pulse is sent when restarting.
*       For example: D2 would be possible, but not D3.
*/
#define DOOR_PIN	D2


/* @brief The unit reads the door position from an ultrasonic sensor.
* The sensor is mounted at the top of the door and measures the distance to the door.
* @note Sensor: HC-SR04 Ultrasonic Sensor
* @node The EchoPin must be connected to an interrupt pin. (e.g.: D8)
*/
#define TRIGGER_PIN D7
#define ECHO_PIN		D8 // must be interrupt capable

#pragma endregion

#pragma region HomeKit Config

/*
* @brief A HomeKit device service contains the basic information of the device
* @note The device name is also used to create the Bonjour service name.
*/
const CDeviceService Device
{
  {
    .DeviceName{"Garage"}, // available as http://Garage.local
    /*
    * ... more optional device information: You can read more about the CDeviceService in hb_homekit.h
    */
  }
};

/*
* @brief Define functions by adding specialized units.
*/
CHost Host
(
    MakeControlUnit()
  , MakeSR04UltrasonicSensorPositionUnit({ TRIGGER_PIN, ECHO_PIN })
  , MakeTriggerDoorMotorUnit(DOOR_PIN)

  , MakeContinuousEventRecorderUnit()
);

/*
* @brief Garage door opener as a service for HomeKit
*/
CGarageDoorOpenerService Door(&Host);

/*
* @brief HomeKit provides everything you need to interact with Apple HomeKit and host a homepage.
*/
CHomeKit HomeKit
(
  Device,
  homekit_accessory_category_garage,
  &Door
);

#pragma endregion

#pragma region setup
void setup()
{
	Serial.begin(115200);
	delay(500); // Important for ESP32: Wait until the serial interface is ready
	Serial.println("\n\n\nEnter Setup");


  // Installs and configures everything for CGarageDoorOpenerService.
  InstallAndSetupGarageDoorOpener(HomeKit, Door, Host);

  // Adds standard menu items to the controller.
  AddStandardMenus(HomeKit);

  // Adds a menu (WiFi) that allows the user to connect to a WiFi network.
  AddWiFiLoginMenu(HomeKit);

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
