#pragma region Prolog
/*******************************************************************
$CRT 10 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_homekit.cpp<< 04 Okt 2024  07:36:08 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling: logtab paramtab wlogin
#pragma endregion
#pragma region Includes
#include <Arduino.h>
#include <StreamString.h>
#include "hb_homekit.h"

namespace HBHomeKit
{
#pragma endregion

#pragma region Globals
#if defined(ESP8266)
ESP8266WebServer WebServer{ 80 };
#elif defined(ESP32)
#endif

#pragma endregion

#pragma region Safe_GetLocalTime
bool Safe_GetLocalTime(tm* pTM)
{
  /* Note: getLocalTime works very slowly if there is no Internet connection. */
  if(!pTM || !CWiFiConnection::IsSTA())
    return false;

  return getLocalTime(pTM);
}
#pragma endregion

#pragma region DeviceName
const char* DeviceName(const homekit_server_config_t* pConfig)
{
  // @see CDeviceService
  const homekit_service_t* InfoService = FindServiceByType(pConfig, HOMEKIT_SERVICE_ACCESSORY_INFORMATION);
  if(InfoService)
  {
    const homekit_characteristic_t* NameCh = FindCharacteristicByTypeOrName(InfoService, HOMEKIT_CHARACTERISTIC_NAME);
    if(NameCh)
      return static_value_cast<const char*>(NameCh);
  }
  return "";
}

#pragma endregion

#pragma region Split
std::vector<String> Split(const String& text, char sep)
{
  std::vector<String> Tokens;
  int Start = 0;
  int End = text.indexOf(sep);

  while(End != -1)
  {
    Tokens.push_back(text.substring(Start, End));
    Start = End + 1;
    End = text.indexOf(sep, Start);
  }

  Tokens.push_back(text.substring(Start));
  return Tokens;
}

#pragma endregion

#pragma region CStreamPassThru (local)
namespace
{
/* Forward write actions over a stream to a writer function. */
class CStreamPassThru : public StreamNull
{
public:
  using CTargetWriter = std::function<size_t(const uint8_t* buffer, size_t size)>;
private:
  CTargetWriter mWriter;
public:
  CStreamPassThru(CTargetWriter writer) : mWriter(std::move(writer)) {}

  virtual size_t write(uint8_t v) override
  {
    return mWriter(&v, 1);
  }

  virtual size_t write(const uint8_t* buffer, size_t size) override
  {
    return mWriter(buffer, size);
  }

};

} // namespace
//END CStreamPassThru
#pragma endregion

#pragma region CTextBuilder
namespace
{
class CTextBuilder : public StreamNull
{
  #pragma region Types
  struct Line_Type
  {
    uint8_t Buf[512];
  };

  using List_Type = std::list<Line_Type>;
  using const_interator = List_Type::const_iterator;

  #pragma endregion

  #pragma region Fields
  size_t    mTotalSize{};
  size_t    mLineSize{};
  uint8_t* mCurLine{};
  List_Type mLines;

  #pragma endregion

public:

  size_t size() const { return mTotalSize; }

  #pragma region ItrAt
  auto ItrAt(size_t index) const
  {
    std::pair<const_interator, size_t> Result{ mLines.end(), 0 };
    for(auto Itr = mLines.begin(); Itr != mLines.end(); ++Itr)
    {
      if(index < sizeof(Line_Type))
      {
        Result.first = Itr;
        Result.second = index;
        break;
      }
      index -= sizeof(Line_Type);
    }
    return Result;
  }

  #pragma endregion

  #pragma region AddString
  void AddString(const uint8_t* text, size_t len)
  {
    if(!text || len == 0)
      return;

    //Serial.printf("AddString: %d  |%.16s|\n", len, text);

    while(len > 0)
    {
      size_t UseableLen = std::min(len, sizeof(Line_Type) - mLineSize);

      if(mCurLine == nullptr || UseableLen == 0)
      {
        //Serial.printf("AddString: New Line\n");
        mCurLine = mLines.emplace_back().Buf;
        mLineSize = 0;
        continue;
      }

      memcpy(mCurLine + mLineSize, text, UseableLen);
      mLineSize += UseableLen;
      mTotalSize += UseableLen;
      text += UseableLen;
      len -= UseableLen;
    }

    //Serial.printf("AddString: TotalSize: %d\n", mTotalSize);
  }

  #pragma endregion


  #pragma region StreamZero Implementation
  size_t mStreamReadIndex{};

  virtual size_t write(uint8_t v) override
  {
    AddString(&v, 1);
    return 1;
  }

  virtual size_t write(const uint8_t* buffer, size_t size) override
  {
    AddString(buffer, size);
    return size;
  }

  virtual ssize_t streamRemaining() override
  {
    return mTotalSize - mStreamReadIndex;
  }

  virtual int read(uint8_t* buffer, size_t len) override
  {
    if(!buffer || len == 0 || len > mTotalSize - mStreamReadIndex)
      return 0;

    auto Pair = ItrAt(mStreamReadIndex);
    const_interator Itr = Pair.first;
    size_t Index = Pair.second;

    size_t Read = 0;
    while(len > 0 && Itr != mLines.end())
    {
      size_t UseableLen = std::min(len, sizeof(Line_Type) - Index);
      memcpy(buffer, Itr->Buf + Index, UseableLen);
      buffer += UseableLen;
      len -= UseableLen;
      Read += UseableLen;
      mStreamReadIndex += UseableLen;

      if(Index + UseableLen == sizeof(Line_Type))
      {
        ++Itr;
        Index = 0;
      }
      else
        Index += UseableLen;
    }

    return Read;
  }

  virtual size_t readBytes(char* buffer, size_t len) override
  {
    return read(reinterpret_cast<uint8_t*>(buffer), len);
  }

  virtual int peek() override
  {
    if(mStreamReadIndex >= mTotalSize)
      return -1;

    auto Pair = ItrAt(mStreamReadIndex);
    return Pair.first->Buf[Pair.second];
  }

  virtual int read() override
  {
    if(mStreamReadIndex >= mTotalSize)
      return -1;

    auto Pair = ItrAt(mStreamReadIndex);
    int Result = Pair.first->Buf[Pair.second];
    ++mStreamReadIndex;
    return Result;
  }

  virtual int available() override
  {
    return mTotalSize - mStreamReadIndex;
  }

  //END StreamZero Implementation
  #pragma endregion


};
} // namespace
#pragma endregion

#pragma region homekit_value_cast - Implementation

#pragma region homekit_value_cast(const homekit_characteristic_t*)
homekit_value_t homekit_value_cast(const homekit_characteristic_t* ch)
{
  if(!ch)
    return homekit_value_t{};
  if(ch->getter_ex)
    return ch->getter_ex(ch);
  else
    return ch->value;
}

#pragma endregion

#pragma region homekit_value_cast<bool>
template<> homekit_value_t homekit_value_cast<bool>(const homekit_value_t& value) noexcept
{
  if(is_value_typeof<bool>(value))
    return value;
  else if(is_value_typeof<const char*>(value))
  {
    const char* Text = static_value_cast<const char*>(value);
    bool Result = Text &&
      (
           strcmp_P(Text, PSTR("1")) == 0
        || strcmp_P(Text, PSTR("ON")) == 0
        || strcmp_P(Text, PSTR("on")) == 0
        || strcmp_P(Text, PSTR("TRUE")) == 0
        || strcmp_P(Text, PSTR("true")) == 0
        );
    return static_value_cast<bool>(Result);
  }
  else if(is_value_typeof<int>(value))
    return static_value_cast<bool>(static_value_cast<int>(value) != 0);
  else if(is_value_typeof<float>(value))
    return static_value_cast<bool>(static_value_cast<float>(value) > 0.0f);

  return homekit_value_t{};
}
#pragma endregion

#pragma region homekit_value_cast<float>
template<> homekit_value_t homekit_value_cast<float>(const homekit_value_t& value) noexcept
{
  if(is_value_typeof<float>(value))
    return value;
  else if(is_value_typeof<const char*>(value))
  {
    const char* Text = static_value_cast<const char*>(value);
    float Result = Text ? atof(Text) : 0.0f;
    return static_value_cast<float>(Result);
  }
  else if(is_value_typeof<int>(value))
    return static_value_cast<float>(static_value_cast<int>(value));
  else if(is_value_typeof<bool>(value))
    return static_value_cast<float>(static_value_cast<bool>(value) ? 1.0f : 0.0f);

  return homekit_value_t{};
}
#pragma endregion

#pragma region homekit_value_cast<int>
template<> homekit_value_t homekit_value_cast<int>(const homekit_value_t& value) noexcept
{
  if(is_value_typeof<int>(value))
    return value;
  else if(is_value_typeof<const char*>(value))
  {
    const char* Text = static_value_cast<const char*>(value);
    float Result = Text ? atoi(Text) : 0;
    return static_value_cast<int>(Result);
  }
  else if(is_value_typeof<float>(value))
    return static_value_cast<int>(static_value_cast<float>(value));
  else if(is_value_typeof<bool>(value))
    return static_value_cast<int>(static_value_cast<bool>(value) ? 1 : 0);

  return homekit_value_t{};
}
#pragma endregion


//END homekit_value_cast - Implementation
#pragma endregion

#pragma region value - Implementation

static void CallSetter(homekit_characteristic_t* ch, const homekit_value_t& newValue)
{
  /* Temporary zeroing of the function pointer prevents recurrence. */

  auto Setter = ch->setter_ex;
  ch->setter_ex = nullptr;

  Setter(ch, newValue);
  ch->setter_ex = Setter;
}

bool modify_value(homekit_value_t& value, const homekit_value_t& newValue)
{
  if(&value == &newValue || homekit_value_equal(&value, &newValue))
    return false;

  homekit_value_destruct(&value);
  homekit_value_copy(&value, &newValue);
  return true;
}

bool modify_value(const homekit_characteristic_t* ch, const homekit_value_t& newValue)
{
  if(ch)
  {
    homekit_characteristic_t* Ch = const_cast<homekit_characteristic_t*>(ch);

    if(ch->setter_ex)
    {
      if(&ch->value == &newValue || homekit_value_equal(&ch->value, &newValue))
        return false;

      CallSetter(Ch, newValue);
    }
    else
    {
      if(modify_value(Ch->value, newValue))
      {
        INFO("modify_value: %s", to_string_debug(ch).c_str());
        return true;
      }
    }
  }

  return false;
}

bool modify_value_and_notify(homekit_characteristic_t* ch, const homekit_value_t& newValue)
{
  if(ch)
  {
    if(&ch->value == &newValue || homekit_value_equal(&ch->value, &newValue))
      return false;

    /* If the value has changed, the new value is copied to the characteristic.
    * And the new value is sent to the client.
    * @note: setter_ex is ignored in this case.
    * @see client_notify_characteristic
    */
    homekit_characteristic_notify(ch, newValue);

    if(ch->setter_ex)
      CallSetter(ch, newValue);
    else
    {
      modify_value(ch->value, newValue);
      INFO("modify_value_and_notify: %s", to_string_debug(ch).c_str());
    }
  }
  return false;
}

//END value - Implementation
#pragma endregion

#pragma region format_text - Implementation

/* bool */
template<> size_t format_text(char* buffer, size_t size, const bool& value)
{
  return format_text_P(buffer, size, PSTR("%s"), value ? "true" : "false");
}

/* int */
template<> size_t format_text(char* buffer, size_t size, const int& value)
{
  return snprintf_P(buffer, size, PSTR("%d i"), value);
}

/* uint8 */
template<> size_t format_text(char* buffer, size_t size, const uint8_t& value)
{
  return snprintf_P(buffer, size, PSTR("%d u8"), value);
}

/* uint16 */
template<> size_t format_text(char* buffer, size_t size, const uint16_t& value)
{
  return snprintf_P(buffer, size, PSTR("%d u16"), value);
}

/* uint32 */
template<> size_t format_text(char* buffer, size_t size, const uint32_t& value)
{
  return snprintf_P(buffer, size, PSTR("%d u32"), value);
}

/* uint64 */
template<> size_t format_text(char* buffer, size_t size, const uint64_t& value)
{
  return snprintf_P(buffer, size, PSTR("%lld u64"), value);
}

/* float */
template<> size_t format_text(char* buffer, size_t size, const float& value)
{
  return snprintf_P(buffer, size, PSTR("%e"), value);
}

/* const char* */
template<> size_t format_text(char* buffer, size_t size, const char* const& value)
{
  return snprintf_P(buffer, size, PSTR("\"%s\""), value);
}

//END format_text - Implementation
#pragma endregion

#pragma region to_string - Implementation

const char* to_string(const homekit_value_t& value, char* buffer, size_t size)
{
  //Serial.printf("to_string  fmt=%d\n", value.format);
  buffer[0] = 0;
  switch(value.format)
  {
    case homekit_format_bool:
      format_text(buffer, size, static_value_cast<bool>(value));
      break;
    case homekit_format_uint8:
      format_text(buffer, size, static_value_cast<uint8_t>(value));
      break;
    case homekit_format_uint16:
      format_text(buffer, size, static_value_cast<uint16_t>(value));
      break;
    case homekit_format_uint32:
      format_text(buffer, size, static_value_cast<uint32_t>(value));
      break;
    case homekit_format_uint64:
      format_text(buffer, size, static_value_cast<uint64_t>(value));
      break;
    case homekit_format_int:
      format_text(buffer, size, static_value_cast<int>(value));
      break;
    case homekit_format_float:
      format_text(buffer, size, static_value_cast<float>(value));
      break;
    case homekit_format_string:
      format_text(buffer, size, static_value_cast<const char*>(value));
      break;

      //case homekit_format_tlv:
      //  return format_text(buffer, size, "TLV");
      //case homekit_format_data:
      //  return format_text(buffer, size, "DATA");
      //default:
      //  return format_text(buffer, size, "Unknown");
  }

  //format_text(buffer, size, static_value_cast<T>(value));
  return buffer;
}

String to_string(const homekit_value_t& value)
{
  char buffer[64];
  return to_string(value, buffer, sizeof(buffer));
}

String to_string_debug(const homekit_characteristic_t* ch)
{
  char buffer[128], valbuf[64];
  snprintf_P
  (
    buffer, sizeof(buffer),
    PSTR("\"%s\" = %s"),
    ch->description,
    to_string(ch->value, valbuf, sizeof(valbuf))
  );
  return buffer;
}


//END to_string - Implementation
#pragma endregion

#pragma region Find - Functions

#pragma region FindServiceByType
/*
* @brief: Find the first service of a given type
* @param pConfig: HomeKit configuration, nullptr -> nullptr
* @param type: Service type (e.g. "SWITCH" or HOMEKIT_SERVICE_SWITCH)
* @return: Pointer to service or nullptr
*/
const homekit_service_t* FindServiceByType(const homekit_server_config_t* pConfig, const char* type)
{
  using Result_Type = const homekit_service_t*;

  return ForEachAccessory<Result_Type>(pConfig, [&](const homekit_accessory_t* pAccessory)
    {
      return ForEachService<Result_Type>
        (
          pAccessory,
          [&](const homekit_service_t* pService)
          {
            if(  strcmp(pService->type, type) == 0
              || strcmp(homekit_service_identify_to_string(pService->type), type) == 0
              )
            {
              return pService;
            }
            else
            {
              return Result_Type{};
            }
          });
    });
}

#pragma endregion

#pragma region FindCharacteristicByTypeOrName
/*
* @brief: Find the first characteristic of a given name
* @param pConfig: HomeKit configuration, nullptr -> nullptr
* @param name: Service name (e.g. "ON", HOMEKIT_CHARACTERISTIC_ON, "On" )
* @return: Pointer to service or nullptr
*/
homekit_characteristic_t* FindCharacteristicByTypeOrName(const homekit_service_t* pService, const char* typeOrName)
{
  using Result_Type = homekit_characteristic_t*;

  return ForEachCharacteristic<Result_Type>(pService, [&](const homekit_characteristic_t* pCharacteristic)
    {
      if(  strcmp(pCharacteristic->type, typeOrName) == 0
        || strcmp(homekit_characteristic_identify_to_string(pCharacteristic->type), typeOrName) == 0
        || strcmp(pCharacteristic->description, typeOrName) == 0
        )
      {
        return const_cast<Result_Type>(pCharacteristic);
      }
      else
      {
        return Result_Type{};
      }
    });
}
#pragma endregion

//END Find - Functions
#pragma endregion

#pragma region CWiFiConnection
WiFiMode_t CWiFiConnection::mCurrentMode{ WIFI_OFF };

#pragma region Connect
bool CWiFiConnection::Connect(bool tryMode)
{
  bool Ret{};
  bool HasSSID{};

  /* Set the LED pin as output
  * and turn it off.
  */
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);
  Serial.println();

  /* If no password enumerator is available,
  * set the recommended mode to AP
  */
  if(mPasswordEnumerator)
  {
    mPasswordEnumerator->Reset();
    HasSSID = mPasswordEnumerator->Next();
    if(HasSSID)
    {
      auto SSID = mPasswordEnumerator->GetSSID();
      HasSSID = !SSID.isEmpty();
    }
  }

  /* If the recommended mode is not AP,
  * and no password enumerator is available,
  * fall back to AP mode.
  */
  switch(GetRecomendedMode())
  {
    case WIFI_STA:
    case WIFI_AP_STA:
      if(!HasSSID)
        SetRecomendedMode(WIFI_AP);
      break;
  }

  /* Try to connect to the WiFi network
  * according to the recommended mode.
  */
  switch(GetRecomendedMode())
  {
    case WIFI_STA:
    case WIFI_AP_STA:
      Ret = STAConnect();
      break;

    default:
    case WIFI_AP:
      Ret = APConnect();
      break;
  }

  /* If no connection could be established,
  * and not in try mode, rapid flashing
  * for 60/6 seconds, then optional reboot.
  */
  if(!tryMode && !Ret)
  {
    for(int loop = 400/6; --loop >= 0;)
    {
      digitalWrite(BUILTIN_LED, LOW);
      delay(100);
      digitalWrite(BUILTIN_LED, HIGH);
      delay(50);
    }
  }

  return Ret;
}

#pragma endregion

#pragma region APConnect
bool CWiFiConnection::APConnect()
{
  int channel = 1, ssid_hidden = 0; // see ESP8266WiFiAPClass::softAP
  const homekit_server_config_t* Config = GetHomeKitConfig();
  char SSID[48]{}, Pass[48]{};

  mInUse = false;

  // Default SSID and Password
  strncpy_P(SSID, PSTR("ESP8266"), sizeof(SSID) - 1);
  strncpy_P(Pass, PSTR("12345678"), sizeof(Pass) - 1);

  if(Config)
  {
    int i, k;
    const char* DN = DeviceName(Config);
    if(DN)
      strncpy(SSID, DN, sizeof(SSID) - 1);

    // Copy and remove '-' from password: "111-11-111" -> "11111111"
    for(k = 0, i = 0; Config->password[i] && i < sizeof(Pass) - 1; ++i)
    {
      if(Config->password[i] != '-')
        Pass[k++] = Config->password[i];
    }
    Pass[k] = 0;
  }

  WiFi.mode(WIFI_AP);
  WiFi.persistent(false);
  WiFi.disconnect(false);
  WiFi.setAutoReconnect(true);

  WiFi.softAP(SSID, Pass, channel, ssid_hidden);
  WiFi.begin();
  mCurrentMode = WIFI_AP;

  if(!mDNSServer)
  {
    mDNSServer = std::move(std::unique_ptr<DNSServer>(new DNSServer));
    // modify TTL associated  with the domain name (in seconds)
    // default is 60 seconds
    mDNSServer->setTTL(3600); // 1h

    // set which return code will be used for all other domains (e.g. sending
    // ServerFailure instead of NonExistentDomain will reduce number of queries
    // sent by clients)
    // default is DNSReplyCode::NonExistentDomain
    mDNSServer->setErrorReplyCode(DNSReplyCode::ServerFailure);

    // start DNS server for a unspecific domain name
    mDNSServer->start(53, "*", WiFi.softAPIP());

    mDnsTicker.attach_ms_scheduled(100, [this]()
      {
        mDNSServer->processNextRequest();
      });
  }


  VERBOSE("APConnect: %s  SSID: \"%s\"  Pass: \"%s\""
    , WiFi.softAPIP().toString().c_str()
    , SSID
    , Pass
  );
  return true;
}
#pragma endregion

#pragma region STAConnect
bool CWiFiConnection::STAConnect()
{
  if(!mPasswordEnumerator)
  {
    ERROR("mPasswordEnumerator is nullptr");
    return false;
  }

  mInUse = false;

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.disconnect(false);
  WiFi.setAutoReconnect(true);

  VERBOSE("STAConnect: Try to connect to WiFi");

  mPasswordEnumerator->Reset();
  while(mPasswordEnumerator->Next())
  {
    auto ssid = mPasswordEnumerator->GetSSID();
    auto password = mPasswordEnumerator->GetPassword();

    digitalWrite(BUILTIN_LED, LOW);
    delay(100);
    digitalWrite(BUILTIN_LED, HIGH);

    if(ssid.isEmpty())
    {
      WARN("STAConnect: SSID is empty");
      continue;
    }

    if(TryConnectWiFi(ssid, password))
    {
      digitalWrite(BUILTIN_LED, LOW);
      mCurrentMode = WIFI_STA;
      mPasswordEnumerator->Accepted();
      return true;
    }
  }

  mCurrentMode = WIFI_OFF;
  ERROR("WiFi STA connection finally failed");

  return false;
}

#pragma endregion

#pragma region TryConnectWiFi
bool CWiFiConnection::TryConnectWiFi(const String& ssid, const String& password)
{
  VERBOSE("\tWiFi try connecting \"%s\" ", ssid.c_str());
  WiFi.begin(ssid, password);

  const int Interval = 1024;
  for(int c = mConnectingTimeout / Interval; --c >= 0;)
  {
    if(WiFi.isConnected())
    {
      Serial.println();
      INFO("WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
      return true;
    }

    Serial.print('.');
    delay(Interval);
  }
  Serial.println();
  WARN("WiFi connection %s failed", ssid.c_str());
  return false;
}

#pragma endregion

#pragma region WifiInfoText
String CWiFiConnection::WifiInfoText() const
{
  char Buf[80];
  if(IsAP(GetCurrentMode()))
    snprintf_P(Buf, sizeof(Buf)
      , PSTR("AP: %.64s")
      , WiFi.softAPIP().toString().c_str()
    );
  else
  {
    auto RSSI = WiFi.RSSI();
    int RIdx = (RSSI < -90) ? 0 : (RSSI < -80) ? 1 : (RSSI < -70) ? 2 : (RSSI < -60) ? 3 : 4;
    const char* RSSI_Levels[] = { "?", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84", "\xe2\x96\x85" };

    snprintf_P(Buf, sizeof(Buf)
      , PSTR("%s  %.64s")
      , RSSI_Levels[RIdx]
      , WiFi.SSID().c_str()
    );
  }

  Buf[sizeof(Buf) - 1] = 0;
  return Buf;
}

#pragma endregion


//END CWiFiConnection
#pragma endregion

#pragma region CHtmlWebSiteBuilder
struct CHtmlWebSiteBuilder : IHtmlWebSiteBuilder
{
  #pragma region static StandardCSS
  static CTextEmitter StandardCSS()
  {
    return MakeTextEmitter(F(R"(
body {
  background-color: black;
  color: #bbb;
  font-family: Verdana, Arial, sans-serif;
  margin: 0;
}
h1 {
  margin: 0.0em;
  padding: 0.4em;
  font-size: 1.6em;
  width: 100%;
  text-align: center;
}
h2 {
  margin: 0;
  padding: 0;
  font-size: 1.0em;
  width: 100%;
  text-align: center;
}
.Header small
{
  font-size: 0.6em;
  float: left;
  background-color: #444;
  padding: 0.2em;
}

.topnav {
  overflow: hidden;
  background-color: #444;
  margin: 0;
  padding-left: 0.2em;
  padding-right: 0.2em;
}
.topnav a {
  float: left;
  display: block;
  color: #eee;
  text-align: center;
  padding: 0.5em;
  text-decoration: none;
}
.topnav a:hover {
  background-color: #ddd;
  color: black;
}
.topnav .selected
{
  background-color: #ea0;
  font-weight: bold;
  color: black;
}
.topnav .justifyright
{
  float: right;
}

.content
{
  margin: 0;
  padding: 0;
  width: 100%;
  overflow-x: auto;
}

table caption {
  font-weight: bold;
  font-size: 1.2em;
  color: #777;
  border-bottom: 2px solid #777;
}
button {
  background-color: #ea0;
  border: 2px solid #ccc;
  border-radius: 1.5em;
  color: black;
  padding: 0.4em 1.2em;
  text-decoration: none;
  margin: 0.1em 0.2em;
  cursor: pointer;
  font-weight: bold;
  font-size: 1.0em;
}
b {
  color: #ccc;
}
i {
  color: #ea0;
}

.unit {
  font-size: 0.7em;
  color: #999;
}

.entrytab {
 font-size: 0.9em;
 border-collapse: collapse;
 width: 100%;
}
.entrytab td, .entrytab th {
  border: 1px solid #000;
  padding: 0.2em;
}
.entrytab th {
  text-align: left;
  font-weight: bold;
  color: #ddd;
}
.entrytab tr:nth-child(even){background-color: #222;}
.entrytab tr:nth-child(odd){background-color: #000;}

)"));
  }
  #pragma endregion


  #pragma region Header
  virtual void Header(Stream& out, CHeaderParams params) override
  {
    out << F(R"(<!DOCTYPE html><html><head>)");

    if(params.RefreshInterval > 0)
    {
      out << F("<meta http-equiv='refresh' content='");
      out << String(params.RefreshInterval);
      out << F("'/>\n");
    }

    out << F(R"(<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>)");

    out << params.Title;

    out << F(R"(</title>
<style type="text/css">
)");

    out << StandardCSS();
    out << params.CSS;

    out << F(R"(
</style><script type="text/javascript">
)");


    out << params.JavaScript;

    out << F(R"(
</script>
</head>
<body>
<div class = 'Header'>)");


    if(params.ExText && params.ExText[0])
      out << F(R"(<small>)") << params.ExText << F(R"(</small>)");

    out << F(R"(<h2>)");
    out << params.Title;
    out << F(R"(</h2></div>)");
  }

  #pragma endregion

  #pragma region Tail
  virtual void Tail(Stream& out) override
  {
    out << F(R"(
</body>
</html>)");
  }

  #pragma endregion

  #pragma region MenuStrip
  virtual void MenuStrip(Stream& out, IMenuStripItems& items) override
  {
    char Buffer[256];

    out << F("<div class='topnav'>");

    while(items.Next())
    {
      auto Item = items.Current();
      auto Selected = Item.Selected;
      auto Special = Item.Special;

      snprintf_P
      (
        Buffer, sizeof(Buffer)
        , PSTR("%s<a %s href=\"%s\">%s</a>%s")
        , Special ? "<div class='justifyright'>" : ""
        , Selected ? "class='selected'" : ""
        , Item.URI
        , Item.Title
        , Special ? "</div>" : ""
      );

      out << Buffer;
    }

    out << F("</div>");
  }

  #pragma endregion

  #pragma region Body
  virtual void Body(Stream& out, CTextEmitter bodyText) override
  {
    out << F("<div class='content'>");
    out << bodyText;
    out << F("</div>");
  }
  #pragma endregion


};

IHtmlWebSiteBuilder_Ptr CreateHtmlWebSiteBuilder()
{
  return std::make_unique<CHtmlWebSiteBuilder>();
}

#pragma endregion

#pragma region CController - Implementation

#pragma region CMenuStripItems
/* Enumerator for menu items
*/
struct CController::CMenuStripItems : IHtmlWebSiteBuilder::IMenuStripItems
{
  #pragma region Fields
  const_menu_iterator mBegin;
  const_menu_iterator mEnd;
  const_menu_iterator mCurrent;
  const CHtmlWebSiteMenuItem* mSelected{};
  
  /* Only menus with low memory usage are visible.
  * @see CHtmlWebSiteMenuItem::LowMemoryUsage
  * @see SendHtmlPage
  */
  bool mOnlyLMUMenusVisible{};

  #pragma endregion

  #pragma region Construction
  CMenuStripItems
  (
    const_menu_iterator begin,
    const_menu_iterator end,
    const CHtmlWebSiteMenuItem* selected,
    bool onlyLMUMenusVisible
  )
    : mBegin{ begin }
    , mEnd{ end }
    , mSelected{ selected }
    , mOnlyLMUMenusVisible{ onlyLMUMenusVisible }
  {
  }

  #pragma endregion

  #pragma region Next
  virtual bool Next() override
  {
    auto IsVisible = [&](const_menu_iterator item) -> bool
      {
        if(mOnlyLMUMenusVisible)
          if(!item->LowMemoryUsage)
            return false;

        return item->Visible ? item->Visible() : true;
      };

    if(mCurrent == const_menu_iterator{})
      mCurrent = mBegin;
    else
      ++mCurrent;

    while(mCurrent != mEnd && !IsVisible(mCurrent))
      ++mCurrent;

    return mCurrent != mEnd;
  }

  #pragma endregion

  #pragma region Current
  virtual IHtmlWebSiteBuilder::CMenuStripItem Current() override
  {
    return
    {
      .Title = mCurrent->MenuName,
      .URI = mCurrent->URI,
      .Selected = &*mCurrent == mSelected,
      .Special = mCurrent->SpecialMenu
    };
  }

  #pragma endregion

};
#pragma endregion

#pragma region GetSelectedMenuItem
const CHtmlWebSiteMenuItem* CController::GetSelectedMenuItem() const
{
  size_t Index = 0;
  for(auto& Item : mMenuItems)
  {
    if(Index == mSelectedMenuIndex)
      return &Item;
    ++Index;
  }
  return nullptr;
}

#pragma endregion

#pragma region GetSelectedMenuItemID
int CController::GetSelectedMenuItemID() const
{
  auto Item = GetSelectedMenuItem();
  return Item ? Item->ID : -1;
}
#pragma endregion

#pragma region SetSelectedMenuItemIndex
bool CController::SetSelectedMenuItemIndex(int index)
{
  if(index < 0)
    return false;

  if(mSelectedMenuIndex == index)
    return false;

  mSelectedMenuIndex = index;
  return true;
}

#pragma endregion

#pragma region AddMenuItem
CController& CController::AddMenuItem(CHtmlWebSiteMenuItem&& item)
{
  if(item.URI[0] == '/' && item.URI[1] == '\0') // Root always at index 0
    mMenuItems.emplace_front(std::move(item));
  else
    mMenuItems.emplace_back(std::move(item));
  return *this;
}

#pragma endregion

#pragma region InstallCmd
CController& CController::InstallCmd(const char* cmd, CInvoker&& func)
{
  mCMDs[cmd] = CCmdItem
  {
    .Invoker = std::move(func)
  };
  return *this;
}
CController& CController::InstallCmd(const char* cmd, CmdAttribute attr, CInvoker&& func)
{
  mCMDs[cmd] = CCmdItem
  {
    .Invoker = std::move(func),
    .Attr = attr
  };
  return *this;
}

CController& CController::InstallCmd(const char* cmd, const char* text, CInvoker&& func)
{
  mCMDs[cmd] = CCmdItem
  {
    .Invoker = std::move(func),
    .Text = text
  };
  return *this;
}
CController& CController::InstallCmd(const char* cmd, const char* text, CmdAttribute attr, CInvoker&& func)
{
  mCMDs[cmd] = CCmdItem
  {
    .Invoker = std::move(func),
    .Text = text,
    .Attr = attr
  };
  return *this;
}

#pragma endregion

#pragma region SetVar
CController& CController::SetVar(const char* var, Expander&& expander)
{
  mVars[var] = std::move(expander);
  return *this;
}
CController& CController::SetVar(const String& var, Expander&& expander)
{
  mVars[var] = std::move(expander);
  return *this;
}

CController& CController::SetVar(const char* var, String content)
{
  return SetVar(var, [content = std::move(content)](auto)
    {
      return MakeTextEmitter(content);
    });
}
CController& CController::SetVar(const String& var, String content)
{
  return SetVar(var, [content = std::move(content)](auto)
    {
      return MakeTextEmitter(content);
    });
}

#pragma endregion

#pragma region TryGetVar
bool CController::TryGetVar(const char* var, String* pContent, CInvokerParam param) const
{
  auto It = mVars.find(var);
  if(It == mVars.end())
  {
    if(pContent)
      *pContent = emptyString;
    return false;
  }
  if(pContent)
    *pContent = to_string(It->second(param));
  return true;
}
bool CController::TryGetVar(const String& var, String* pContent, CInvokerParam param) const
{
  auto It = mVars.find(var);
  if(It == mVars.end())
  {
    if(pContent)
      *pContent = emptyString;
    return false;
  }
  if(pContent)
    *pContent = to_string(It->second(param));
  return true;
}

#pragma endregion

#pragma region Setup
void CController::Setup()
{
  InstallStdVariables();
  InstallStdCMDs();

  #pragma region WebServer
  // enable CORS header in WebServer results
  WebServer.enableCORS(true);

  // enable ETAG header in WebServer results from serveStatic handler
  WebServer.enableETag(true);

  WebServer.onNotFound([this]() 
    { 
      mWifiConnection.SetInUse();
      PerformWebServerRequest();
    });
  WebServer.on("/var", HTTP_OPTIONS, [] { WebServer.send(204); });
  WebServer.on("/var", HTTP_GET, [this] 
    {
      mWifiConnection.SetInUse();
      PerformWebServerVarRequest();
    });

  WebServer.addHook([this](const String& method, const String& url, WiFiClient* client, ESP8266WebServer::ContentTypeFunction contentType)
    {
      #if 0
      Serial.printf("Hook  Method: %s  URL: %s  From: %s\n"
        , method.c_str(), url.c_str(), client->remoteIP().toString().c_str());
      #endif

      if(mDisableWebRequests)
        return ESP8266WebServer::CLIENT_IS_GIVEN; // blocks the client
      return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
    });


  WebServer.begin();
  VERBOSE("Web-Server started");

  #pragma endregion
}
#pragma endregion

#pragma region Loop
void CController::Loop(bool tickerEnabled)
{
  WebServer.handleClient();

  const uint32_t CurTick = millis();
  if(tickerEnabled)
  {
    OneShotTicker.OnTick(CurTick);
    Ticker.OnTick(CurTick);
  }
}
#pragma endregion

#pragma region IndexOfMenuURI
int CController::IndexOfMenuURI(const char* uri) const
{
  int Index = 0;

  if(uri[0] == '/' && uri[1] == 0) /* "/"->root */
    return 0;

  for(auto& Item : mMenuItems)
  {
    if(strcmp(Item.URI, uri) == 0)
      return Index;
    ++Index;
  }

  return -1;
}

#pragma endregion

#pragma region SendHtmlPage
void CController::SendHtmlPage()
{
  // use HTTP/1.1 Chunked response to avoid building a huge temporary string
  if(WebServer.chunkedResponseModeStart(200, "text/html"))
  {
    CStreamPassThru HtmlOutStream
    (
      [&](const uint8_t* buffer, size_t size) -> size_t
      {
        WebServer.sendContent_P(reinterpret_cast<const char*>(buffer), size);
        return size;
      }
    );
    SendHtmlPage(HtmlOutStream);
    WebServer.chunkedResponseFinalize();
    INFO_HEAP();
  }
  else
  {
    if(!mOnlyLMUMenusVisible)
    {
      WARN("HTTP1.1 not supported - Only small websites can be sent.");
      mOnlyLMUMenusVisible = true;
    }

    /* The HTML 1.0 protocol expects the page length as a parameter at
    * the beginning. To determine the number of bytes, the entire HTML
    * page must be prepared in memory.
    * Some menus contain large HTML pages that cannot be rendered
    * completely in memory. For this reason, only those menus are
    * displayed that can be safely rendered with little memory.
    * The LowMemoryUsage option can be used to check if a suitable
    * menu has been selected. If this is not the case, a suitable
    * menu will be selected.
    */
    {
      int FirstLowMemPageIndex = -1;
      int Index=0;
      for(const auto& Item : mMenuItems)
      {
        if(Item.LowMemoryUsage && FirstLowMemPageIndex < 0)
          FirstLowMemPageIndex = Index;

        if(mSelectedMenuIndex == Index)
          if(!Item.LowMemoryUsage)
            mSelectedMenuIndex = -1;

        ++Index;
      }
      if(mSelectedMenuIndex < 0 && FirstLowMemPageIndex >= 0)
      {
        WARN("Because of HTML1.0 it is necessary to switch to a page with low memory usage.");
        mSelectedMenuIndex = FirstLowMemPageIndex;
      }
      if(mSelectedMenuIndex < 0)
      {
        ERROR("Unfortunately there is no Low-Mem-Usage page.");
        mSelectedMenuIndex = 0;
      }
    }

    CTextBuilder TextBuilder;
    SendHtmlPage(TextBuilder);
    VERBOSE("HTML Page has %d bytes", TextBuilder.size());
    WebServer.send(200, "text/html", static_cast<Stream*>(&TextBuilder));
    INFO_HEAP();
  }
}

#pragma endregion

#pragma region SendHtmlPage(Stream&)
void CController::SendHtmlPage(Stream& out) const
{
  auto MenuItem = GetSelectedMenuItem();
  String WifiInfo = mWifiConnection.WifiInfoText();

  /*--- Html-Page Header --- */
  mBuilder->Header
  (
    out,
    {
      .Title = DeviceName(),
      .ExText = WifiInfo.c_str(),

      .CSS = MenuItem ? MenuItem->CSS : CTextEmitter{},
      .JavaScript = MenuItem ? MenuItem->JavaScript : CTextEmitter{},

      .RefreshInterval = MenuItem ? MenuItem->RefreshInterval : 0
    }
  );

  /*--- Menu Strip --- */
  {
    CMenuStripItems MenuItems
    (
      mMenuItems.begin(),
      mMenuItems.end(),
      MenuItem, mOnlyLMUMenusVisible
    );
    mBuilder->MenuStrip(out, MenuItems);
  }

  /*--- Title --- */
  if(MenuItem && MenuItem->Title)
  {
    out << F("<h1>");
    out << MenuItem->Title;
    out << F("</h1>");
  }

  /*--- Body --- */
  if(MenuItem && MenuItem->Body)
  {
    mBuilder->Body
    (
      out,
      [this, BodyText = to_string(MenuItem->Body)](Stream& out)
      {
        ExpandVariables(out, BodyText);
      }
    );
  }

  /*--- Html-Page Tail --- */
  mBuilder->Tail(out);
}

#pragma endregion

#pragma region GetCmdText
const char* CController::GetCmdText(const char* cmd) const
{
  auto It = mCMDs.find(cmd);
  if(It == mCMDs.end())
    return cmd;
  if(It->second.Text.isEmpty())
    return cmd;
  return It->second.Text.c_str();
}
#pragma endregion

#pragma region ExpandVariables
void CController::ExpandVariables(Stream& out, const String& _inputText) const
{
  size_t Pos = 0;
  const char* const InputText = _inputText.c_str(); // for faster access
  if(InputText == nullptr)
    return;

  auto WriteChar = [&out](char c) { out.write(&c, 1); };
  auto WriteString = [&out](const String& s) { out.write(s.c_str(), s.length()); };
  auto WriteEmitter = [&out](CTextEmitter&& e) { e(out); };

  for(Pos = 0; InputText[Pos]; )
  {
    if(InputText[Pos] == '{')
    {
      size_t EndPos = _inputText.indexOf('}', Pos);
      if(EndPos != -1)
      {
        // Split "{VarName:Args}" -> "VarName" and "Args"
        String VarName, Args;
        size_t SepPos = _inputText.indexOf(':', Pos);
        if(SepPos != -1 && SepPos < EndPos) // Args found
        {
          VarName = _inputText.substring(Pos + 1, SepPos);
          Args = _inputText.substring(SepPos + 1, EndPos);
        }
        else
        {
          VarName = _inputText.substring(Pos + 1, EndPos);
        }

        auto It = mVars.find(VarName);
        if(It != mVars.end())
        {
          const homekit_value_t Arg0 = static_value_cast(Args.c_str());

          // expand variable by calling the expander-function @see CInvokerParam
          WriteEmitter(It->second(Args.isEmpty() ? nullptr : &Arg0));
        }
        else
        {
          // variable not found -> copy the original text
          WriteString(_inputText.substring(Pos, EndPos + 1));
          WARN("Variable \"%.32s\" not found", VarName.c_str());
        }

        Pos = EndPos + 1;
      }
      else
      {
        ERROR("Missing '}' in \"%.32s\"", &InputText[Pos]);
        WriteChar(InputText[Pos++]);
      }
    }
    else
    {
      WriteChar(InputText[Pos++]);
    }
  }
}
#pragma endregion

#pragma region PerformWebServerRequest
void CController::PerformWebServerRequest()
{
  CmdAttribute Attr{};
  const String* CMD = &emptyString;


  for(int i = WebServer.args(); --i >= 0;)
  {
    const String& Name = WebServer.argName(i);
    const String& Arg = WebServer.arg(i);
    VERBOSE("Arg[%d]: \"%s\" = \"%s\"", i, Name.c_str(), Arg.c_str());

    if(Name == "CMD")
      CMD = &Arg;
    else
      SetVar(Name, Arg);
  }


  /* Parse the command from Webserver request
   * FORM_CMD: Command to be executed: e.g.: "REBOOT" or "LOGIN:Name"
   * PlainCMD: CMD without any arguments: e.g.: "REBOOT" or "LOGIN"
   * Arg: Argument of FORM_CMD: e.g.: "Name"
   * CmdItr: Iterator to the command in the command list. If not found, it is mCMDs.end()
   * Param: Invoker-Parameter for the command.
  */
  // PlainCMD: CMD without any arguments (: as sep)
  int ArgPos = CMD->indexOf(':');
  String PlainCMD = CMD->isEmpty()
    ? ""
    : CMD->substring(0, ArgPos)
    ;
  String Arg = (ArgPos == -1)
    ? ""
    : CMD->substring(ArgPos + 1)
    ;
  homekit_value_t Arg0 = static_value_cast(Arg.c_str());
  CInvokerParam Param{ &Arg0 };
  auto CmdItr = FindCMD(PlainCMD);


  /* Set the selected menu item by the URI of the WebServer request
   * e.g. "/device" -> "Device Information" - Menu
  */
  {
    int MIdx = IndexOfMenuURI(WebServer.uri().c_str());
    if(MIdx >= 0)
      SetSelectedMenuItemIndex(MIdx);
  }

  /* Execute the command
   * If the command is not found, an error message is output.
   * If the command is found, the command is executed.
   * If the command is marked with CmdAttr_FirstSendPage, the page is sent first.
  */
  if(CmdItr != mCMDs.end())
  {
    Attr = CmdItr->second.Attr;
    if(Attr != CmdAttr_FirstSendPage)
      InvokeCMD(CmdItr, PlainCMD, Param);
  }
  else if(!CMD->isEmpty())
  {
    ERROR("Unsupported CMD \"%s\"", PlainCMD.c_str());
  }

  /* Send the HTML page to the client.
  */
  {
    const auto* MenuItem = GetSelectedMenuItem();

    DispatchWebseverUriRequest
    (
      MenuItem != nullptr
      ? MenuItem->URI
      : WebServer.uri().c_str()
    );
  }

  /* Execute the command
   * If the command is marked with CmdAttr_FirstSendPage, the page is sent first.
  */
  if(Attr == CmdAttr_FirstSendPage && CmdItr != mCMDs.end())
  {
    // waiting until page is sent
    delay(1000);

    InvokeCMD(CmdItr, PlainCMD, Param);
  }

}
#pragma endregion

#pragma region PerformWebServerVarRequest
void CController::PerformWebServerVarRequest()
{
  String ResultText;
  int ArgIndex;

  //Serial.printf("HTTP_GET /var:\n");

  for(ArgIndex = 0; ArgIndex < WebServer.args(); ++ArgIndex)
  {
    //Serial.printf("\t\"%s\" = \"%s\"\n", WebServer.argName(ArgIndex).c_str(), WebServer.arg(ArgIndex).c_str());

    const String& Name = WebServer.argName(ArgIndex);
    if(TryGetVar(Name, nullptr))
      break;

  }

  if(ArgIndex >= WebServer.args())
  {
    WebServer.send(404, "text/plain", "Variable Not found");

    for(ArgIndex = 0; ArgIndex < WebServer.args(); ++ArgIndex)
    {
      const String& Name = WebServer.argName(ArgIndex);
      ResultText += F("\"");
      ResultText += WebServer.argName(ArgIndex);
      ResultText += F("\" ");
    }

    // get IP of the client
    ResultText += F(" From: ");
    ResultText += WebServer.client().remoteIP().toString();

    ERROR("HTTP_GET /var: Not found: %s", ResultText.c_str());
  }
  else
  {
    const String& Name = WebServer.argName(ArgIndex);
    const String& ArgContent = WebServer.arg(ArgIndex);
    CInvokerParam Param;
    homekit_value_t Arg0;

    if(!ArgContent.isEmpty())
    {
      Arg0 = static_value_cast<const char*>(ArgContent.c_str());
      Param.Args[0] = &Arg0;
    }

    TryGetVar(Name, &ResultText, Param);

    #if 0
    if(ArgContent.isEmpty())
    {
      VERBOSE("HTTP_GET /var:%s -> \"%.60s\""
        , Name.c_str()
        , ResultText.c_str()
      );
    }
    else
    {
      VERBOSE("HTTP_GET /var:%s=\"%s\" -> \"%.60s\""
        , Name.c_str()
        , ArgContent.c_str()
        , ResultText.c_str()
      );
    }

    #endif // 0

    WebServer.send(200, "text/plain", ResultText);
  }
}
#pragma endregion

#pragma region DispatchWebseverUriRequest
void CController::DispatchWebseverUriRequest(const char* uri)
{
  int Idx = IndexOfMenuURI(uri);

  if(Idx < 0)
  {
    const char* Suffix = strrchr(uri, '.'); // e.g. ".css"
    if(Suffix)
    {
      VERBOSE("WebServer Request \"%s\" -> File not found", uri);
      WebServer.send(404, "text/plain", "File not found");
    }
    else
    {
      ERROR("WebServer Request \"%s\" -> Page Not found -> forward to Root", uri);
      // forward to "/" (root)
      SetSelectedMenuItemIndex(0);
      WebServer.sendHeader("Location", "/");
      WebServer.send(302, "text/plain", "");
    }
  }
  else
  {
    INFO("WebServer Request \"%s\"", uri);
    SetSelectedMenuItemIndex(Idx);
    SendHtmlPage();
    //WebServer.send(200, "text/html", CreateHtmlPage());
  }
}

#pragma endregion

#pragma region FindCMD
CController::const_cmd_iterator CController::FindCMD(const String& cmd) const
{
  return mCMDs.find(cmd);
}

#pragma endregion

#pragma region InvokeCMD
void CController::InvokeCMD(const_cmd_iterator cmdItr, const String& cmd, CInvokerParam param)
{
  char ArgBuf[64];

  if(cmdItr != mCMDs.end())
  {
    ArgBuf[0] = 0;
    if(param.Args[0])
      to_string(*param.Args[0], ArgBuf, sizeof(ArgBuf));

    INFO("CMD \"%s\" %s", cmd.c_str(), ArgBuf);
    cmdItr->second.Invoker(param);
  }
  else
  {
    ERROR("Unknown CMD \"%s\"", cmd.c_str());
  }
}
#pragma endregion

#pragma region InstallStdCMDs
void CController::InstallStdCMDs()
{
  InstallCmd("REBOOT", CmdAttr_FirstSendPage, [](auto)
    {
      ESP.reset();
    });
  InstallCmd("RESET_PAIRING", "Reset Pairing", CmdAttr_FirstSendPage, [](auto)
    {
      homekit_storage_reset();
      ESP.reset();
    });
  InstallCmd("REFRESH", [](auto)
    {
      //The page will be retransmitted.
    });
  InstallCmd("FORMAT_FILESYSTEM", [](auto)
    {
      SimpleFileSystem::Format();
    });

}

#pragma endregion

#pragma region InstallStdVariables
void CController::InstallStdVariables()
{
  #pragma region LOCAL_IP
  SetVar("LOCAL_IP", [this](auto)
    {
      return MakeTextEmitter(WiFi.localIP().toString());
    });

  #pragma endregion
  #pragma region REMOTE_IP
  SetVar("REMOTE_IP", [this](auto)
    {
      return MakeTextEmitter(WebServer.client().remoteIP().toString());
    });

  #pragma endregion
  #pragma region MAC
  SetVar("MAC", [this](auto)
    {
      return MakeTextEmitter(WiFi.macAddress());
    });

  #pragma endregion
  #pragma region DEVICE_NAME
  SetVar("DEVICE_NAME", [this](auto)
    {
      return MakeTextEmitter(DeviceName());
    });

  #pragma endregion
  #pragma region IS_PAIRED
  SetVar("IS_PAIRED", [this](auto)
    {
      return MakeTextEmitter(homekit_is_paired() ? "Yes" : "No");
    });

  #pragma endregion
  #pragma region CLIENT_COUNT
  SetVar("CLIENT_COUNT", [this](auto)
    {
      return MakeTextEmitter(String(arduino_homekit_connected_clients_count()));
    });

  #pragma endregion
  #pragma region FORM_CMD
  SetVar("FORM_CMD", [this](auto p)
    {
      char Buffer[256];
      auto CmdID = safe_value_cast<const char*>(p.Args[0]);
      if(!CmdID)
      {
        ERROR("FORM_CMD: missing argument");
        return MakeTextEmitter(F(""));
      }
      if(FindCMD(CmdID) == mCMDs.end())
      {
        ERROR("FORM_CMD: unknown command \"%s\"", CmdID);
        return MakeTextEmitter(F("***unknown command***"));
      }

      snprintf_P
      (
        Buffer, sizeof(Buffer),
        PSTR("<button type='submit' name='CMD' value='%s'>%s</button>")
        , CmdID
        , GetCmdText(CmdID)
      );
      return MakeTextEmitter(Buffer);
    });

  #pragma endregion
  #pragma region FORM_BEGIN
  SetVar("FORM_BEGIN", [this](auto)
    {
      const char* URI = GetSelectedMenuItem()->URI;
      char Buffer[256];
      // <form action='/device' method='POST'>
      snprintf_P
      (
        Buffer, sizeof(Buffer),
        PSTR("<form action='%s' method='POST'>")
        , URI
      );
      return MakeTextEmitter(String(Buffer));
    });

  #pragma endregion
  #pragma region FORM_END
  SetVar("FORM_END", [this](auto)
    {
      return MakeTextEmitter(F("</form>"));
    });

  #pragma endregion
  #pragma region PARAM_TABLE_BEGIN
  SetVar("PARAM_TABLE_BEGIN", [this](auto)
    {
      return MakeTextEmitter(F("<table class='entrytab'>"));
    });

  #pragma endregion
  #pragma region PARAM_TABLE_END
  SetVar("PARAM_TABLE_END", [this](auto)
    {
      return MakeTextEmitter(F("</table>"));
    });

  #pragma endregion
  #pragma region DATE
  SetVar("DATE", [this](auto)
    {
      char TimeStamp[32];
      struct tm timeinfo;
      if(!Safe_GetLocalTime(&timeinfo))
        return MakeTextEmitter(F("????-??-??"));
      else
      {
        strftime(TimeStamp, sizeof(TimeStamp), "%Y-%m-%d", &timeinfo);
        return MakeTextEmitter(TimeStamp);
      }
    });
  #pragma endregion
  #pragma region TIME
  SetVar("TIME", [this](auto)
    {
      char TimeStamp[32];
      struct tm timeinfo;
      if(!Safe_GetLocalTime(&timeinfo))
        return MakeTextEmitter(F("??:??:??"));
      else
      {
        strftime(TimeStamp, sizeof(TimeStamp), "%H:%M:%S", &timeinfo);
        return MakeTextEmitter(TimeStamp);
      }
    });

  #pragma endregion



}
#pragma endregion

#pragma region CWiFiConnectionCtrl Members
void CController::CWiFiConnectionCtrl::ConnectOrDie()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    if(!mCtrl.mWifiConnection.Connect())
    {
      WARN("Could not connect to another WLAN. Fallback to ST.");
      mCtrl.mWifiConnection.APConnect();
    }
    
    bool IsSTA = CWiFiConnection::IsSTA();

    if(IsSTA)
    {
      mCtrl.Ticker.Start(STAWatchDogTimeoutMS, [&]()
        {
          if(WiFi.status() != WL_CONNECTED)
          {
            WARN("WiFi not more connected, try to re-connect");
            if(!mCtrl.mWifiConnection.STAConnect())
            {
              ERROR("Could not connect to STA. Rebooting.");
              delay(500); // wait for serial output
              ESP.reset();
            }
          }
        });
    }
    else
    {
      VERBOSE("If there is no access to a private WiFi website within approximately 5 minutes, the device will restart and try to connect to an existing WiFi.");
      mCtrl.OneShotTicker.Start(APWatchDogTimeoutMS, [&]()
        {
          if(!mCtrl.mWifiConnection.IsInUse())
          {
            WARN("A reboot is now performed because the temporary AP connection was not used.");
            delay(500); // wait for serial output
            ESP.reset();
          }
        });
    }
  }
}
#pragma endregion


//END CController - Implementation
#pragma endregion

#pragma region CSimpleFileSystem - Implementation
#define _PRT(f, ...) //Serial.printf(f, ##__VA_ARGS__)

#pragma region CFileEnumerator::Next
bool CSimpleFileSystem::CFileEnumerator::Next()
{
  if(mBeginModifyCounter != mFS.mModifyCounter)
    return false;

  while(++mIndex < MaxTableEntries)
  {
    const auto& E = mFS.mPage.Entries[mIndex];

    if(E.Name[0] != 0)
    {
      memcpy(mName, E.Name, NameLength);
      mName[NameLength] = 0;
      return true;
    }
  }
  return false;
}

#pragma endregion

#pragma region Sync
void CSimpleFileSystem::Sync()
{
  if(mWriteBackNeeded)
  {
    WritePage();
    mWriteBackNeeded = false;
    INFO("File system synced: %d bytes", mUsedMemorySize);
    //DebugPrintTable();
  }
}

#pragma endregion

#pragma region FileSize
int CSimpleFileSystem::FileSize(const char* pName) const
{
  int index = FindEntry(pName);
  if(index < 0)
    return -1;
  return mPage.Entries[index].Size;
}

#pragma endregion

#pragma region ReadFile
int CSimpleFileSystem::ReadFile(const char* pName, uint8_t* pBuf, int Size)
{
  int index = FindEntry(pName);
  if(index < 0)
    return -1;

  const auto& E = mPage.Entries[index];
  const auto& M = mPage.Memory;

  if(Size != E.Size)
    return -1;

  memcpy(pBuf, &M[E.StartOffset], E.Size);
  return E.Size;
}

#pragma endregion

#pragma region WriteFile
/* Writes or creates a file.
  * @param pName The name of the file. Must be 4 characters long.
  * @param pBuf The buffer to write.
  * @param Size The size of the buffer. Must be equal to the file size if the file already exists.
  * @return The number of bytes written or -1 if the file system is full.
*/
int CSimpleFileSystem::WriteFile(const char* pName, const uint8_t* pBuf, int Size)
{
  bool NewFile = false;
  int index = FindEntry(pName);
  if(index < 0)
  {
    index = FindEntry(); // find empty slot
    if(index < 0)
    {
      ERROR("No more entry for new files");
      return -1;
    }

    size_t FreeMemory = sizeof(mPage.Memory) - mUsedMemorySize;
    if(Size > FreeMemory)
    {
      ERROR("Not enough memory for new file");
      return -1;
    }

    memcpy(mPage.Entries[index].Name, pName, NameLength);
    NewFile = true;
  }
  else
  {
    if(mPage.Entries[index].Size != Size)
    {
      ERROR("Size mismatch for existing file");
      return -1;
    }
  }

  INFO("%s #%d \"%s\"  %d bytes"
    , NewFile ? "CreateFile" : "WriteFile"
    , index
    , pName
    , Size
  );

  auto& E = mPage.Entries[index];

  if(NewFile)
  {
    E.StartOffset = mUsedMemorySize;
    E.Size = Size;
    mUsedMemorySize += Size;
  }

  bool Changed = memcmp(&mPage.Memory[E.StartOffset], pBuf, Size) != 0;

  memcpy(&mPage.Memory[E.StartOffset], pBuf, Size);

  if(Changed || NewFile)
    Touch();
  return Size;
}

#pragma endregion

#pragma region List
/* Lists the files in the file system.
*/
void CSimpleFileSystem::List()
{
  auto Enum = Files();
  Serial.println("Files:");
  while(Enum.Next())
  {
    Serial.printf
    (
      "\t\"%-4.4s\"  #%d\n"
      , Enum.Current()
      , FileSize(Enum.Current())
    );
  }
  Serial.println();
}

#pragma endregion

#pragma region Touch
void CSimpleFileSystem::Touch()
{
  ++mModifyCounter;
  mWriteBackNeeded = true;
}

#pragma endregion

#pragma region ReadPage
bool CSimpleFileSystem::ReadPage()
{
  if(!homekit_storage_common_read(EpromPage, 0, (uint8_t*)&mPage, sizeof(mPage)))
    return false;

  uint32_t Crc = crc32((const uint8_t*)&mPage.Entries[0], sizeof(mPage.Entries));
  _PRT("ReadPage CRC %08X  %08X\n", Crc, mPage.Crc);
  if(Crc != mPage.Crc)
  {
    return false;
  }

  const size_t MaxMemorySize = sizeof(mPage.Memory);
  size_t MemOffset = 0;

  for(int i = 0; i < MaxTableEntries; ++i)
  {
    const auto& E = mPage.Entries[i];
    if(E.StartOffset >= MaxMemorySize)
    {
      _PRT("Invalid StartOffset %d\n", E.StartOffset);
      return false;
    }
    if(E.StartOffset + E.Size > MaxMemorySize)
    {
      _PRT("Invalid Size %d\n", E.Size);
      return false;
    }


    MemOffset = std::max<size_t>(MemOffset, E.StartOffset + E.Size);
  }
  mUsedMemorySize = MemOffset;

  if(mUsedMemorySize > MaxMemorySize)
  {
    _PRT("Invalid MemorySize %d\n", mUsedMemorySize);
    return false;
  }

  _PRT("ReadPage OK %d bytes\n", mUsedMemorySize);
  return true;
}
#pragma endregion

#pragma region WritePage
bool CSimpleFileSystem::WritePage()
{
  mPage.Crc = crc32((const uint8_t*)&mPage.Entries[0], sizeof(mPage.Entries));
  _PRT("WritePage CRC %08X\n", mPage.Crc);
  return homekit_storage_common_write(EpromPage, 0, (uint8_t*)&mPage, sizeof(mPage));
}

#pragma endregion

#pragma region Format
void CSimpleFileSystem::Format()
{
  INFO("Formatting file system");
  memset(&mPage, 0, sizeof(mPage));
  mUsedMemorySize = 0;
  Touch();
}

#pragma endregion

#pragma region ReadPageOrFormat
void CSimpleFileSystem::ReadPageOrFormat()
{
  bool State = ReadPage();

  //DebugPrintTable();

  if(!State)
    Format();
}

#pragma endregion

#pragma region FindEntry
int CSimpleFileSystem::FindEntry(const char* pName) const
{
  char InternalName[NameLength]{};
  if(pName)
    strncpy(InternalName, pName, NameLength);

  for(int i = 0; i < MaxTableEntries; ++i)
  {
    if(memcmp(mPage.Entries[i].Name, InternalName, NameLength) == 0)
      return i;
  }
  return -1;
}

#pragma endregion

#pragma region DebugPrintTable
void CSimpleFileSystem::DebugPrintTable()
{
  Serial.printf("(Debug) Table: #%d\n", mUsedMemorySize);
  for(int i = 0; i < MaxTableEntries; ++i)
  {
    const auto& E = mPage.Entries[i];
    if(E.Name[0] != 0)
    {
      Serial.printf
      (
        "\t%d: \"%-4.4s\"  %d #%d\n"
        , i
        , E.Name
        , E.StartOffset
        , E.Size
      );
    }
  }
  Serial.println();
}

#pragma endregion

#undef _PRT
//END CSimpleFileSystem - Implementation
#pragma endregion


#pragma region >>> CHomeKit <<<

#pragma region ResetPairing
/* Reset HomeKit pairing.
* @param reboot If true, the device will reboot after resetting the pairing.
*/
void CHomeKit::ResetPairing(bool reboot)
{
  //LOG_D("HomeKit_ResetPairing");
  mIsPaired = false;
  homekit_storage_reset();
  if(reboot)
    ESP.reset();
}

#pragma endregion

#pragma region Setup
/* Setup the HomeKit device.
* @note This method must be called in the setup() function.
*/
bool CHomeKit::Setup()
{
  Controller.WiFiCtrl.ConnectOrDie();

  SyncDateTime();
  InstallLogging();

  if(CWiFiConnection::IsSTA())
  {
    homekit_server_init(mConfig);
    mHomeKitServerStarted = true;
  }

  Controller.Setup();

  VERBOSE("Free heap: %d bytes", system_get_free_heap_size());
  return true;
}

#pragma endregion

#pragma region Loop
/* Loop the HomeKit device.
* @note This method must be called in the loop() function.
*/
void CHomeKit::Loop()
{
  if(mHomeKitServerStarted)
    arduino_homekit_loop();

  /* Access time-controlled functions only when the device is paired with HomeKit.
  * This improves pairing by eliminating time-consuming functions.
  * It also calls processing functions only when they are applicable.
  */
  Controller.Loop(IsPaired());

  const uint32_t CurTick = millis();
  mLEDTimer.OnTick(CurTick);

  #if 0
  if(IsPaired() && system_get_cpu_freq() == SYS_CPU_160MHZ)
  {
    system_update_cpu_freq(SYS_CPU_80MHZ);
    VERBOSE("Update the CPU to run at 80MHz");
  }
  #endif
}

#pragma endregion

#pragma region UpdateBuildInLED
void CHomeKit::UpdateBuildInLED(CTicker::CEvent e)
{
  /* Blink Patterns:
  * - when at least one client is connected: ##############--
  * - when no client is connected:           ##--------------
  * - unpaired:                              #-------#-------
  * - AP:                                    ##--##----------
  */
  enum : uint16_t
  {
    BlinkPattern1 = 0b1111111111111100,
    BlinkPattern2 = 0b1100000000000000,
    BlinkPattern3 = 0b1000000010000000,
    BlinkPattern4 = 0b1100110000000000,
  };

  uint16_t Pattern = BlinkPattern4;
  auto callCount = e.Calls;


  if(CWiFiConnection::IsAP())
    Pattern = BlinkPattern4;
  else
  {
    if(IsPaired()) // Was the device registered with HomeKit? (gepaired)?
    {
      int Cnt = arduino_homekit_connected_clients_count();
      if(Cnt > 0)
        Pattern = BlinkPattern1;
      else
        Pattern = BlinkPattern2;
    }
    else
    {
      Pattern = BlinkPattern3;
    }
  }

  const bool LEDState = (Pattern & (1 << (callCount & 15))) != 0;
  digitalWrite(BUILTIN_LED, LEDState ? LOW : HIGH);
}
#pragma endregion

#pragma region SyncDateTime
/* Synchronize the date and time with an NTP server.
*/
void CHomeKit::SyncDateTime()
{
  /* get time from NTP server
  * void configTime(int timezone_sec, int daylightOffset_sec, const char* server1, const char* server2, const char* server3)
  * "C:\Users\hb\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.2\cores\esp8266\time.cpp"
  */
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

#pragma endregion



bool CHomeKit::mIsPaired{};
CHomeKit* CHomeKit::Singleton{};

//END CHomeKit
#pragma endregion



#pragma region Epilog
} // namespace HBHomeKit
#pragma endregion
