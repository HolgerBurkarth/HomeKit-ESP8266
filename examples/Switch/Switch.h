#pragma region Prolog
/*******************************************************************
$CRT 03 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>Switch.h<< 03 Okt 2024  15:36:50 - (c) proDAD
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
  .Setter = [](homekit_characteristic_t* pC, homekit_value_t value)
  {
    VERBOSE("Switch::Setter");
    if(modify_value(pC, value))
    {
      bool Stat = static_value_cast<bool>(value);
      digitalWrite(SWITCHER_PIN, Stat ? HIGH : LOW);
    }
  },
  .Getter = [](const homekit_characteristic_t* pC) -> homekit_value_t
  {
    VERBOSE("Switch::Getter");
    return static_value_cast<bool>(digitalRead(SWITCHER_PIN) == HIGH);
  }
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

#pragma region InstallVarsAndCmds
void InstallVarsAndCmds(CController& c)
{
  c
    .SetVar("SWITCH", [](auto p)
      {
        if(p.Args[0] != nullptr) // case of: /?SWITCH=true
        {
          /* "true", "TRUE", "1", "ON", "on"  ==> bool */
          bool NewState = convert_value<bool>(*p.Args[0]);
          modify_value_and_notify(&Switcher.State, NewState);
        }

        /* return "true" or "false"  */
        return MakeTextEmitter(Switcher.State);
      })

    ;
}

#pragma endregion

#pragma region Switch_JavaScript
CTextEmitter Switch_JavaScript()
{
  return MakeTextEmitter(F(R"(

function UIUpdateSwitch(state)
{
  var State = (state == 'true');
  SetElementInnerHTML('state', State ? 'ON' : 'OFF');
  SetElementChecked('switch', State);
}

function SetSwitch(state)
{
  ForSetVar('SWITCH', state, responseText => UIUpdateSwitch(responseText) );
}

function Update()
{
  ForVar('SWITCH', responseText => UIUpdateSwitch(responseText) );
}


onload = function()
{
  Update();

  setInterval
  ( 
    function() 
      { 
        Update(); 
      },
      1000
    );
};

)"));
}
#pragma endregion

#pragma region Switch_BodyHtml
CTextEmitter Switch_BodyHtml()
{
  return MakeTextEmitter(F(R"(
<table><tr><th>Switcher is</th><td><div id="state"></div></td></tr></table>

<p>
  {ACTION_CHECKBOX:switch:SetSwitch(this.checked)}
  &nbsp;
  &nbsp;
  &nbsp;
  {ACTION_BUTTON:SetSwitch(false):OFF}
  {ACTION_BUTTON:SetSwitch(true):ON}
</p>
)"));
}
#pragma endregion

#pragma region AddMenuItems
void AddMenuItems(CController& c)
{
  c
    .AddMenuItem
    (
      {
        .Title = "Switch",
        .MenuName = "Start",
        .URI = "/",
        .CSS = ActionUI_CSS(),
        .JavaScript = [](Stream& out)
        {
          out << ActionUI_JavaScript();
          out << UIUtils_JavaScript();
          out << Switch_JavaScript();
        },
        .Body = Switch_BodyHtml()
      }
    )
    ;
}

#pragma endregion


#pragma region setup
void setup()
{
	Serial.begin(115200);
	delay(500); // Important for ESP32: Wait until the serial interface is ready
	Serial.println("\n\n\nEnter Setup");

  pinMode(SWITCHER_PIN, OUTPUT);
  //digitalWrite(SWITCHER_PIN, LOW);


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
