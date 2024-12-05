#pragma region Prolog
/*******************************************************************
$CRT 18 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_device_menu.cpp<< 05 Dez 2024  10:04:50 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling: firmwareupdate
#pragma endregion
#pragma region Includes
#include <Arduino.h>
#include "hb_homekit.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

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

#pragma region FirmwareUpdateStat
static String gbFirmwareUpdateStat;
static bool gbUpdatingFirmware = false;

void FirmwareUpdateStat(Stream& out)
{
  out << gbFirmwareUpdateStat;
}
#pragma endregion

#pragma region UpdatingFirmware

bool UpdatingFirmware(CController& c)
{
  WiFiClient client;

  // Add optional callback notifiers
  ESPhttpUpdate.onStart([&]() 
    {
      VERBOSE("HTTP update process started");
    });
  ESPhttpUpdate.onEnd([&]()
    {
      VERBOSE("HTTP update process finished");
    });
  ESPhttpUpdate.onError([&](int err)
    {
      VERBOSE("HTTP update fatal error code %d", err);
    });
  ESPhttpUpdate.onProgress([&](int cur, int total)
    {
      VERBOSE("HTTP update process at %d of %d bytes...", cur, total);
    });


  ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  ESPhttpUpdate.rebootOnUpdate(false);

  t_httpUpdate_return ret = ESPhttpUpdate.update(client, c.GetFirmwareUpdateURL());

  // Reset callbacks
  ESPhttpUpdate.onStart(nullptr);
  ESPhttpUpdate.onEnd(nullptr);
  ESPhttpUpdate.onError(nullptr);
  ESPhttpUpdate.onProgress(nullptr);


  char Buf[256]{};

  switch(ret)
  {
    case HTTP_UPDATE_FAILED:
      sprintf_P(Buf
        , PSTR("Firmware update failed: %s")
        , ESPhttpUpdate.getLastErrorString().c_str()
      );
      ERROR("%s", Buf);
      break;

    case HTTP_UPDATE_NO_UPDATES:
      sprintf_P(Buf, PSTR("Firmware update: No updates available"));
      break;

    case HTTP_UPDATE_OK:
      sprintf_P(Buf, PSTR("Firmware update successful -- Press REBOOT"));
      INFO("s", Buf);
      break;
  }

  if(Buf[0])
  {
    strcat(Buf, "\n");
    gbFirmwareUpdateStat.concat(Buf);
  }

  return ret == HTTP_UPDATE_OK;
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
    .SetVar("MODEL", [](auto) { return Model; })
    .SetVar("FIRMWARE", [](auto) { return Firmware; })
    .SetVar("FIRMWARE_UPDATE_URL", [&](auto) { return MakeTextEmitter(c.GetFirmwareUpdateURL()); })
    .SetVar("FIRMWARE_UPDATE_STA", [&](auto) { return FirmwareUpdateStat; })
    .SetVar("START_UPDATE_FIRMWARE", [&](auto p) -> CTextEmitter
      {
        if(p.Args[0] != nullptr)
        {
          INFO("START_UPDATE_FIRMWARE Button pressed");
          if(!gbUpdatingFirmware)
          {
            gbFirmwareUpdateStat.clear();

            if(UpdatingFirmware(c))
              gbUpdatingFirmware = true;
          }
        }
        return MakeTextEmitter();
      })


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
        .CSS = ActionUI_CSS(),
        .JavaScript = [](Stream& out)
        {
          out << ActionUI_JavaScript();
          out << F(R"(
function SetDiv(divID, value)
{
  document.getElementById(divID).innerHTML = value;
}

function OnStartUpdateFirmware()
{
  SetDiv('firmwareupdate', 'Firmware update started...');
  SetVar('START_UPDATE_FIRMWARE', '1');
}


onload = function()
{
  setInterval(function()
    {
      ForVar('DATE', responseText => SetDiv('date', responseText) );
      ForVar('TIME', responseText => SetDiv('time', responseText) );
      ForVar('CLIENT_COUNT', responseText => SetDiv('clients', '#' + responseText + ' clients') );
      ForVar('FIRMWARE', responseText => SetDiv('firmware', responseText) );
      ForVar('FIRMWARE_UPDATE_STA', responseText => SetDiv('firmwareupdate', responseText) );
    }, 1000);
};

)");
        },
        .Body = [&](Stream& out)
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
<tr><th>Firmware</th><td><div id='firmware'>{FIRMWARE}</div></td></tr>
<tr><th>Firmware URL</th><td>{FIRMWARE_UPDATE_URL}</td></tr>

{PARAM_TABLE_END}
<br>
{FORM_BEGIN}
{FORM_CMD:REBOOT} {FORM_CMD:RESET_PAIRING}
<br><hr><br>
{FORM_CMD:FORMAT_FILESYSTEM}
{FORM_END}
)");

          if(!c.GetFirmwareUpdateURL().isEmpty())
            out << F(R"(
<br><br><br><br><br><hr>
<pre><div id='firmwareupdate'></div></pre>
{ACTION_BUTTON:OnStartUpdateFirmware():Update Firmware}
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
