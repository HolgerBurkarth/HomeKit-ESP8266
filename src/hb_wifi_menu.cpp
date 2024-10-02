#pragma region Prolog
/*******************************************************************
$CRT 17 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_wifi_menu.cpp<< 02 Okt 2024  07:41:03 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling: logtab wlogin
#pragma endregion
#pragma region Includes
#include <Arduino.h>
#include "hb_homekit.h"

namespace HBHomeKit
{
namespace
{
#pragma endregion

#pragma region CreateHtmlWifiTable
String CreateHtmlWifiTable()
{
  char Buffer[200];
  String Txt;
  Txt.reserve(4 * 1024);
  String CurrSSID = WiFi.SSID();

  Txt += F("<table id='logtab'><caption>Scanned WiFi Devices</caption>");
  Txt += F("<tr><th>#</th><th>SSID</th><th>RSSI</th><th>Ch.</th><th>Encryption</th></tr>");


  #pragma region AddTableItem
  auto AddTableItem = [&](int i)
    {
      const auto& SSID = WiFi.SSID(i);
      bool IsConnected = (SSID == CurrSSID);
      const char* Authmode{ "" };

      switch(WiFi.encryptionType(i))
      {
        case AUTH_OPEN:         Authmode = ("OPEN");          break;
        case AUTH_WEP:          Authmode = ("WEP");           break;
        case AUTH_WPA_PSK:      Authmode = ("WPA/PSK");       break;
        case AUTH_WPA2_PSK:     Authmode = ("WPA2/PSK");      break;
        case AUTH_WPA_WPA2_PSK: Authmode = ("WPA/WPA2/PSK");  break;
      }

      Txt += IsConnected ? F("<tr class='warn'>") : F("<tr>");

      snprintf_P
      (
        Buffer, sizeof(Buffer)
        , PSTR("<td>%d</td><td nowrap>")
        , i + 1
      );
      Txt += Buffer;

      if(IsConnected)
      {
        snprintf_P
        (
          Buffer, sizeof(Buffer)
          , PSTR("%.60s")
          , SSID.c_str()
        );
      }
      else
      {
        snprintf_P
        (
          Buffer, sizeof(Buffer)
          , PSTR("<button type='submit' name='CMD' value='ENTER_WLOGIN:%.60s'>+</button> %.60s")
          , SSID.c_str()
          , SSID.c_str()
        );
      }
      Txt += Buffer;

      snprintf_P
      (
        Buffer, sizeof(Buffer)
        , PSTR("</td><td>%d</td><td>%d</td><td>%s</td></tr>")
        , (int)WiFi.RSSI(i)
        , (int)WiFi.channel(i)
        , Authmode
        //, WiFi.BSSIDstr(i).c_str()
      );
      Txt += Buffer;
    };

  #pragma endregion

  #if 0
  int numberOfNetwork = WiFi.scanComplete();
  if(numberOfNetwork == -2)
  {
    WiFi.scanNetworks(true);
  }
  else if(numberOfNetwork > 0)
  {
    for(int i = 0; i < numberOfNetwork; ++i)
      AddTableItem(i);

    WiFi.scanDelete();
    if(WiFi.scanComplete() == -2)
      WiFi.scanNetworks(true);
  }
  #else
  int numberOfNetwork = WiFi.scanNetworks();
  for(int i = 0; i < numberOfNetwork; ++i)
    AddTableItem(i);
  #endif


  Txt += F("</table>");
  return Txt;
}
#pragma endregion

#pragma region CWifiLoginContext
struct CWifiLoginContext : IPasswordEnumerator
{
  #pragma region Types
  enum
  {
    MaxSSIDLength = 48,
    MaxPasswordLength = 48
  };


  struct CItem
  {
    String SSID;
    String Password;
  };

  #pragma endregion

  #pragma region Fields
  std::array<CItem, 1> mItems;
  int mIdx{ -1 };
  bool mIsAccepted{ false };

  #pragma endregion

  #pragma region Properties
  const String& GetFirstSSID() const
  {
    return mItems[0].SSID;
  }
  const String& GetFirstPassword() const
  {
    return mItems[0].Password;
  }

  void SetFirstSSID(String ssid)
  {
    mItems[0].SSID = std::move(ssid);
  }

  void SetFirstPassword(String password)
  {
    mItems[0].Password = std::move(password);
  }

  #pragma endregion

  #pragma region IPasswordEnumerator Methods
  virtual void Reset() override
  {
    mIdx = -1;
    mIsAccepted = false;
  }

  virtual bool Next() override
  {
    if(++mIdx < mItems.size())
      return true;
    return false;
  }

  virtual String GetSSID() const override
  {
    return mItems[mIdx].SSID;
  }

  virtual String GetPassword() const override
  {
    return mItems[mIdx].Password;
  }

  virtual void Accepted() override
  {
    mIsAccepted = true;
    SaveToEEPROM();
  }

  #pragma endregion

  #pragma region Methods

  #pragma region LoadFromEEPROM
  void LoadFromEEPROM()
  {
    uint8_t Buf[std::max(MaxSSIDLength, MaxPasswordLength) + 1]; // +1 for null terminator
    char Name[4+1];
    auto FS = std::make_unique<CSimpleFileSystem>();

    for(size_t i = 0; i < mItems.size(); ++i)
    {
      memset(Buf, 0, sizeof(Buf));
      snprintf_P(Name, sizeof(Name), PSTR("SSI%d"), i);
      if(FS->ReadFile(Name, Buf, MaxSSIDLength) > 0)
        mItems[i].SSID = (const char*)Buf;

      memset(Buf, 0, sizeof(Buf));
      snprintf_P(Name, sizeof(Name), PSTR("PAS%d"), i);
      if(FS->ReadFile(Name, Buf, MaxPasswordLength) > 0)
        mItems[i].Password = (const char*)Buf;

      //Serial.printf("SSID: \"%.50s\"  Password: \"%.50s\"\n", mItems[i].SSID.c_str(), mItems[i].Password.c_str());

    }

  }

  #pragma endregion

  #pragma region SaveToEEPROM
  void SaveToEEPROM()
  {
    uint8_t Buf[std::max(MaxSSIDLength, MaxPasswordLength) + 1]; // +1 for the null terminator
    char Name[4+1];
    auto FS = std::make_unique<CSimpleFileSystem>();

    for(size_t i = 0; i < mItems.size(); ++i)
    {
      memset(Buf, 0, sizeof(Buf));
      strncpy((char*)Buf, mItems[i].SSID.c_str(), MaxSSIDLength);
      snprintf_P(Name, sizeof(Name), PSTR("SSI%d"), i);
      FS->WriteFile(Name, Buf, MaxSSIDLength);

      memset(Buf, 0, sizeof(Buf));
      strncpy((char*)Buf, mItems[i].Password.c_str(), MaxPasswordLength);
      snprintf_P(Name, sizeof(Name), PSTR("PAS%d"), i);
      FS->WriteFile(Name, Buf, MaxPasswordLength);
    }
  }

  #pragma endregion

  //END Methods
  #pragma endregion

};

//END CWifiLoginContext
#pragma endregion


#pragma region AddWiFiLoginMenu
} // namespace


CController& AddWiFiLoginMenu(CController& c, bool alwaysUsePeristentLogin)
{
  auto& WifiConnection = CHomeKit::Singleton->WifiConnection;
  auto Ctx = std::make_shared<CWifiLoginContext>();
  Ctx->LoadFromEEPROM();

  #pragma region ActivateOwnPasswordEnumerator
  auto ActivateOwnPasswordEnumerator = [&]()
    {
      WifiConnection.SetPasswordEnumerator(Ctx);
    };

  #pragma endregion

  /* If the password enumerator is not set, we use our own EEPROM-Based enumerator.
  */
  if(alwaysUsePeristentLogin || !WifiConnection.HasPasswordEnumerator())
    ActivateOwnPasswordEnumerator();

  c

    #pragma region Variables

    #pragma region WLOGIN_SSID
    /* The SSID of the first WiFi network to connect to.
    */
    .SetVar("WLOGIN_SSID", [Ctx](auto)
      {
        String SSID = Ctx->GetFirstSSID();
        if(SSID.isEmpty())
          SSID = WiFi.SSID();
        return MakeTextEmitter(std::move(SSID));
      })

    #pragma endregion
    #pragma region WLOGIN_PASSWORD
    /* The password of the first WiFi network to connect to.
    */
    .SetVar("WLOGIN_PASSWORD", [Ctx](auto)
      {
        String Pass = Ctx->GetFirstPassword();
        if(Pass.isEmpty())
        {
          if(WiFi.SSID() == Ctx->GetFirstSSID())
            Pass = WiFi.psk();
        }
        return MakeTextEmitter(std::move(Pass));
      })

    #pragma endregion
    #pragma region WIFI_LIST
    /* The list of available WiFi networks as an HTML table.
    */
    .SetVar("WIFI_LIST", [&, Ctx](auto)
      {
        return MakeTextEmitter(CreateHtmlWifiTable());
      })

    #pragma endregion

    //END Variables
    #pragma endregion

    #pragma region Commands

    #pragma region ENTER_WLOGIN
    /* Display the password entry page "/wlogin".
    * @note The SSID is expected as argument.
    * @see MenuItem("/wlogin")
    * @example {ENTER_WLOGIN:MySSID}
    */
    .InstallCmd("ENTER_WLOGIN", [&, Ctx](auto p)
      {
        String Arguments;
        int MenuIndex = c.IndexOfMenuURI("/wlogin");
        if(p.Args[0])
          Arguments = safe_value_cast<const char*>(p.Args[0]);

        if(MenuIndex < 0)
        {
          ERROR("MenuItem for /wlogin was not found");
        }
        else if(Arguments.isEmpty())
        {
          ERROR("ENTER_WLOGIN: missing argument");
        }
        else
        {
          c.SetSelectedMenuItemIndex(MenuIndex);
          Ctx->SetFirstSSID(std::move(Arguments));
        }
      })

    #pragma endregion
    #pragma region LOGIN
    /* Connect to the selected WiFi network.
    * @note The required WiFi information is expected in the WLOGIN_SSID and WLOGIN_PASSWORD variables.
    * @note The "root" page is displayed if the connection is successful.
    */
    .InstallCmd("LOGIN", [&, Ctx](auto)
      {
        bool IsSTA = CWiFiConnection::IsSTA();
        String Tmp;
        CWiFiConnection& WC = CHomeKit::Singleton->WifiConnection;
        CController& Ctrl = CHomeKit::Singleton->Controller;
        if(Ctrl.TryGetVar("WLOGIN_SSID", &Tmp))
          Ctx->SetFirstSSID(Tmp);
        if(Ctrl.TryGetVar("WLOGIN_PASSWORD", &Tmp))
          Ctx->SetFirstPassword(Tmp);

        if(WC.STAConnect())
        {
          Ctrl.SetSelectedMenuItemIndex(0); // forward to root
          if(!IsSTA)
          {
            ESP.restart();
          }
        }

      })

    #pragma endregion

    //END Commands
    #pragma endregion

    #pragma region Menus
    #pragma region /wifi
    /* Displays a page with a list of available WIFIs.
    * The WIFIs can be chosen via buttons to establish a connection.
    * The user is then taken to the login page.
    * @see Command "LOGIN" "WIFI_LIST"
    * @see Variables "WLOGIN_SSID" "WLOGIN_PASSWORD"
    */
    .AddMenuItem
    (
      {
        .Title = "WiFi",
        .MenuName = "WiFi",
        .URI = "/wifi",
        .LowMemoryUsage = true,
        .CSS = MakeTextEmitter(F(R"(
#logtab td, #logtab th {
  border: 1px solid #444;
  padding: 0.2em;
}
#logtab th {
  padding-top: 0.5em;
  padding-bottom: 0.5em;
  text-align: center;
  background-color: #888;
  color: black;
}
#logtab .warn {
  background-color: #640;
}
)")),
        .Body = MakeTextEmitter(F(R"(
{FORM_BEGIN}

{WIFI_LIST}
<br>
{FORM_CMD:REFRESH}

{FORM_END}
)")),
      }
    )

    #pragma endregion
    #pragma region /wlogin
    .AddMenuItem
    (
      {
        .Title = "WiFi Login",
        .MenuName = "Login",
        .URI = "/wlogin",
        .LowMemoryUsage = true,
        .CSS = MakeTextEmitter(F(R"(
input[type="text"] {
  font-size: 1.2em;
  padding: 0.2em;
  margin: 0.2em;
}
)")),
        .Body = MakeTextEmitter(F(R"(
{FORM_BEGIN}
<p>Enter Password for {WLOGIN_SSID}</p>
<input type="text" name="WLOGIN_PASSWORD" value="{WLOGIN_PASSWORD}"/>
{FORM_CMD:LOGIN}

{FORM_END}
)")),
      //.Visible = []() -> bool { return false; } // hide this menu; not visible in the menu-bar
      }
    )

    #pragma endregion

    //END Menus
    #pragma endregion

    ;

  return c;
}

#pragma endregion


#pragma region Epilog
} // namespace HBHomeKit
#pragma endregion
