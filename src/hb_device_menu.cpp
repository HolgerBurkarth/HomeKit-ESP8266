#pragma region Prolog
/*******************************************************************
$CRT 18 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_device_menu.cpp<< 04 Okt 2024  07:40:04 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Includes
#include <Arduino.h>
#include "hb_homekit.h"

namespace HBHomeKit
{
namespace
{
#pragma endregion


#pragma region FindCharacteristicText
const char* FindCharacteristicText(const char* chUUID)
{
  const homekit_server_config_t* pConfig = GetHomeKitConfig();
  if(pConfig)
  {
    // @see CDeviceService
    const homekit_service_t* InfoService = FindServiceByType(pConfig, HOMEKIT_SERVICE_ACCESSORY_INFORMATION);
    if(InfoService)
    {
      const homekit_characteristic_t* Ch = FindCharacteristicByTypeOrName(InfoService, chUUID);
      if(Ch)
        return static_value_cast<const char*>(Ch);
    }
  }
  return "";
}

#pragma endregion

#pragma region Manufacturer
void Manufacturer(Stream& out)
{
  out << FindCharacteristicText(HOMEKIT_CHARACTERISTIC_MANUFACTURER);
}
#pragma endregion

#pragma region SerialNumber
void SerialNumber(Stream& out)
{
  out << FindCharacteristicText(HOMEKIT_CHARACTERISTIC_SERIAL_NUMBER);
}
#pragma endregion

#pragma region Firmware
void Firmware(Stream& out)
{
  out << FindCharacteristicText(HOMEKIT_CHARACTERISTIC_FIRMWARE_REVISION);
}
#pragma endregion

#pragma region Model
void Model(Stream& out)
{
  out << FindCharacteristicText(HOMEKIT_CHARACTERISTIC_MODEL);
}
#pragma endregion


#pragma region AddDeviceMenu
} // namespace


CController& AddDeviceMenu(CController& c)
{
  c
    #pragma region Variables

    .SetVar("MANUFACTURER", [](auto) { return Manufacturer; })
    .SetVar("SERIAL_NUMBER", [](auto) { return SerialNumber; })
    .SetVar("FIRMWARE", [](auto) { return Firmware; })
    .SetVar("MODEL", [](auto) { return Model; })


    //END Variables
    #pragma endregion

    #pragma region Menu
    .AddMenuItem
    (
      {
        .Title = "Device Information",
        .MenuName = "Device",
        .URI = "/device",
        .LowMemoryUsage = true,
        .SpecialMenu = true,
        .JavaScript = [](Stream& out)
        {
          out << ActionUI_JavaScript();
          out << F(R"(
function SetDiv(divID, value)
{
  document.getElementById(divID).innerHTML = value;
}

onload = function()
{
  setInterval(function()
    {
      ForVar('DATE', responseText => SetDiv('date', responseText) );
      ForVar('TIME', responseText => SetDiv('time', responseText) );
      ForVar('CLIENT_COUNT', responseText => SetDiv('clients', '#' + responseText + ' clients') );
    }, 1000);
};

)");
        },
        .Body = [](Stream& out)
        {
          out << F(R"(
{PARAM_TABLE_BEGIN}
<tr><th>Device Name</th><td>{DEVICE_NAME}</td></tr>
<tr><th>UTC Date</th><td><div id='date'>{DATE}</div></td></tr>
<tr><th>UTC Time</th><td><div id='time'>{TIME}</div></td></tr>
<tr><th>Paired with HK</th><td>{IS_PAIRED}</td></tr>
<tr><th>Connected with</th><td><div id='clients'>#% clients</div></td></tr>

<tr><th>Remote IP</th><td>{REMOTE_IP}</td></tr>
<tr><th>Device IP</th><td>{LOCAL_IP}</td></tr>
<tr><th>MAC</th><td>{MAC}</td></tr>
<tr><th>Model</th><td>{MODEL}</td></tr>
<tr><th>Manufacturer</th><td>{MANUFACTURER}</td></tr>
<tr><th>Serial Number</th><td>{SERIAL_NUMBER}</td></tr>
<tr><th>Firmware</th><td>{FIRMWARE}</td></tr>

{PARAM_TABLE_END}
<br>
{FORM_BEGIN}
{FORM_CMD:REBOOT} {FORM_CMD:RESET_PAIRING}
<br><hr><br>
{FORM_CMD:FORMAT_FILESYSTEM}
{FORM_END}
)");
        }
      }
    )
    //END Menus
    #pragma endregion

    ;

  return c;
}

#pragma endregion


#pragma region Epilog
} // namespace HBHomeKit
#pragma endregion
