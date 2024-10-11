#pragma region Prolog
/*******************************************************************
$CRT 11 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>Heater.h<< 11 Okt 2024  07:47:03 - (c) proDAD
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
#include <HomeKit_HeaterCooler.h>

/* Display + AHT-Sensor */
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/Org_01.h>
#include <Adafruit_AHTX0.h>

using namespace HBHomeKit;
using namespace HBHomeKit::HeaterCooler;
#pragma endregion

#pragma region Config

// 0.96" I2C IIC 128x64 monochrome OLED Display
#define DISPLAY_ADDRESS 0x3C
Adafruit_SSD1306 display(128, 64, &Wire);

// AHTx0 Sensor
Adafruit_AHTX0 aht;

// Example: Header with humidity and temperature sensor (AHTx0)
constexpr auto Characteristics = ECharacteristicFlags::Heater_HumiditySensor;

#pragma endregion

#pragma region CDisplay
struct CDisplay
{
  #pragma region EAttr
  enum class EAttr : uint8_t
  {
    Normal  = 0x00,
    Marked  = 0x01,
    Hide    = 0x02
  };

  friend constexpr inline bool operator & (EAttr a, EAttr b) { return static_cast<uint8_t>(a) & static_cast<uint8_t>(b); }
  friend constexpr inline EAttr operator | (EAttr a, EAttr b) { return static_cast<EAttr>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }

  #pragma endregion

  #pragma region CContext
  struct CContext
  {
    char            Buf[16];
    bool            NeedDisplay{};
    uint8_t         Tick{};
    COptionalFloat  TargetTemperature{};
    COptionalFloat  Temperature{};
    COptionalFloat  Humidity{};
    HOMEKIT_CURRENT_HEATER_COOLER_STATE CurrentState{ HOMEKIT_CURRENT_HEATER_COOLER_STATE_UNDEF };
    HOMEKIT_CHARACTERISTIC_STATUS       Active{ HOMEKIT_STATUS_UNDEF };


    bool TemperatureChanged{};
    bool HumidityChanged{};
    bool TargetTemperatureChanged{};
    bool CurrentStateChanged{};
    bool ActiveChanged{};
  };

  #pragma endregion

  #pragma region Fields
  uint8_t Tick{};
  float   mLastTemperature{ std::numeric_limits<float>::min() };
  float   mLastTargetTemperature{ std::numeric_limits<float>::min() };
  float   mLastHumidity{ std::numeric_limits<float>::min() };
  HOMEKIT_CURRENT_HEATER_COOLER_STATE mLastState{};
  HOMEKIT_CHARACTERISTIC_STATUS       mLastActive{};

  #pragma endregion

  #pragma region Methods

  #pragma region DrawTemperature
  static void DrawTemperature(CContext& ctx, float value, EAttr attr)
  {
    display.fillRect(0, 0, 92 - 24, 30, BLACK);
    if(attr & EAttr::Hide) return;

    display.setFont(&FreeSans18pt7b);
    display.setCursor(0, 27);
    snprintf_P(ctx.Buf, sizeof(ctx.Buf), PSTR("%0.1f"), value);
    display.print(ctx.Buf);

    //Serial.printf("Cursor: %d %d\n", display.getCursorX(), display.getCursorY());
    display.setCursor(66, 27);

    display.setCursor(display.getCursorX() + 7, display.getCursorY() - 10);
    display.drawCircle(display.getCursorX() - 1, display.getCursorY() - 10, 3, WHITE);
    display.setFont(&FreeSans9pt7b);
    display.println(" C");
    //display.drawRect(0, 0, 92-24, 30, WHITE);
    ctx.NeedDisplay = true;
  }

  #pragma endregion

  #pragma region DrawHumidity
  static void DrawHumidity(CContext& ctx, float value, EAttr attr)
  {
    display.fillRect(21, 42, 70, 22, BLACK);
    if(attr & EAttr::Hide) return;

    display.setFont(&FreeSans12pt7b);
    display.setCursor(23, 60);
    snprintf_P(ctx.Buf, sizeof(ctx.Buf), PSTR("%0.1f"), value);
    display.print(ctx.Buf);
    display.setFont(&FreeSans9pt7b);
    display.println(" %");
    //display.drawRect(21, 42, 70, 22, WHITE);
    ctx.NeedDisplay = true;
  }

  #pragma endregion

  #pragma region DrawDot
  static void DrawDot(CContext& ctx)
  {
    if((ctx.Tick & 3) == 0)
    {
      ctx.NeedDisplay = true;
      display.fillCircle(3, 60, 3, (ctx.Tick & 4) ? WHITE : BLACK);
    }
  }

  #pragma endregion

  #pragma region DrawState
  static void DrawState(CContext& ctx, HOMEKIT_CURRENT_HEATER_COOLER_STATE state, EAttr attr)
  {
    const char* Txt{};
    switch(state)
    {
      case HOMEKIT_CURRENT_HEATER_COOLER_STATE_INACTIVE:  Txt = "OFF"; break;
      case HOMEKIT_CURRENT_HEATER_COOLER_STATE_IDLE:      Txt = "IDLE"; break;
      case HOMEKIT_CURRENT_HEATER_COOLER_STATE_HEATING:   Txt = "HEAT"; break;
      case HOMEKIT_CURRENT_HEATER_COOLER_STATE_COOLING:   Txt = "COOL"; break;
    }

    ctx.NeedDisplay = true;
    display.fillRect(100, 0, 27, 10, BLACK);
    if(attr & EAttr::Hide) return;

    if(Txt)
    {
      display.setFont(&Org_01);
      display.setCursor(102, 7);
      display.print(Txt);

      if(attr & EAttr::Marked)
        display.fillRect(100, 0, 27, 10, INVERSE);
    }
  }

  #pragma endregion

  #pragma region DrawTargetTemperature
  static void DrawTargetTemperature(CContext& ctx, float value, EAttr attr)
  {
    display.fillRect(79, 21, 40, 18, BLACK);
    if(attr & EAttr::Hide) return;

    if(value < 0) return;
    display.setFont(&FreeSans9pt7b);
    display.setCursor(80, 36);
    snprintf_P(ctx.Buf, sizeof(ctx.Buf), PSTR("%0.1f"), value);
    display.print(ctx.Buf);
    if(attr & EAttr::Marked)
      display.fillRect(79, 21, 40, 18, INVERSE);
    //display.drawRect(79, 21, 40, 18, WHITE);
    ctx.NeedDisplay = true;
  }

  #pragma endregion

  #pragma region IdentifyChanges
  void IdentifyChanges(CContext& ctx)
  {
    ctx.ActiveChanged = ctx.Active != mLastActive;
    mLastActive = ctx.Active;
    ctx.CurrentStateChanged = ctx.CurrentState != mLastState;
    mLastState = ctx.CurrentState;

    if(ctx.Humidity)
    {
      ctx.HumidityChanged = fabs(ctx.Humidity.value() - mLastHumidity) >= 0.05f;
      mLastHumidity = ctx.Humidity.value();
    }
    if(ctx.Temperature)
    {
      ctx.TemperatureChanged = fabs(ctx.Temperature.value() - mLastTemperature) >= 0.05f;
      mLastTemperature = ctx.Temperature.value();
    }
    if(ctx.TargetTemperatureChanged)
    {
      ctx.TargetTemperatureChanged = fabs(ctx.TargetTemperature.value() - mLastTargetTemperature) >= 0.05f;
      mLastTargetTemperature = ctx.TargetTemperature.value();
    }
  }
  #pragma endregion

  #pragma region Draw
  void Draw(CContext& ctx)
  {
    auto ClampF = [](float value) { return std::clamp(value, -9.9f, 99.9f); };

    if(ctx.TemperatureChanged)
      DrawTemperature(ctx, ClampF(ctx.Temperature.value_or(0.0f)), ctx.Temperature ? EAttr::Normal : EAttr::Hide);

    if(ctx.HumidityChanged)
      DrawHumidity(ctx, ClampF(ctx.Humidity.value_or(0.0f)), ctx.Humidity ? EAttr::Normal : EAttr::Hide);

    if(ctx.TargetTemperatureChanged || ctx.ActiveChanged || ctx.CurrentStateChanged)
    {
      EAttr Attr = EAttr::Hide;
      if(ctx.TargetTemperature && ctx.Active == HOMEKIT_STATUS_ACTIVE)
      {
        Attr = EAttr::Normal;

        if(ctx.CurrentState == HOMEKIT_CURRENT_HEATER_COOLER_STATE_HEATING
          || ctx.CurrentState == HOMEKIT_CURRENT_HEATER_COOLER_STATE_COOLING)
        {
          Attr = EAttr::Marked;
        }
      }
      DrawTargetTemperature(ctx, ClampF(ctx.TargetTemperature.value_or(0.0f)), Attr);
    }

    if(ctx.CurrentStateChanged)
      DrawState(ctx, ctx.CurrentState, EAttr::Normal);


    DrawDot(ctx);
  }
  #pragma endregion

  #pragma region Update
  void Update(const CChangeInfo& info)
  {
    const auto Flags = info.Flags;
    const bool CanHeating = (Flags & ECharacteristicFlags::HeatingThreshold);
    const bool CanCooling = (Flags & ECharacteristicFlags::CoolingThreshold);

    CContext Ctx
    {
      .Tick               = Tick++,
      .TargetTemperature  = CanHeating ? info.HeatingThresholdTemperature : info.CoolingThresholdTemperature,
      .Temperature        = info.Sensor.Temperature,
      .Humidity           = info.Sensor.Humidity,
      .CurrentState       = info.CurrentState,
      .Active             = info.Active
    };


    IdentifyChanges(Ctx);
    Draw(Ctx);

    if(Ctx.NeedDisplay)
    {
      display.display();
      #if 0
      Serial.printf("Display: %s %s %s\n"
        , Ctx.TemperatureChanged ? "T" : ""
        , Ctx.HumidityChanged ? "H" : ""
        , Ctx.StateChanged ? "S" : ""
      );
      #endif
    }
  }

  #pragma endregion


  //END Methods
  #pragma endregion

};
#pragma endregion


#pragma region HomeKit Config
CHost Host
(
  MakeControlUnit(),
  MakeContinuousReadSensorUnit(
    1000, // [ms] Determine new sensor values every second
    [
      Temperature = CKalman1DFilterF{ .R = 8e-3f /* Measurement variance */ },
      Humidity    = CKalman1DFilterF{ .R = 8e-3f /* Measurement variance */ }
    ]() mutable
    {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

      CSensorInfo Info;
      Info.Temperature = Temperature.Update(temp.temperature);
      Info.Humidity    = Humidity.Update(humidity.relative_humidity);
      return Info;
    }),
  MakeOnChangedUnit(
    500, // [ms] Check for changes every 500ms and let the dot flash
    [Display = CDisplay{}](const CChangeInfo& info) mutable
    {
      Display.Update(info);
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
    .DeviceName{"Heater"}, // available as http://Heater.local
    /*
    * ... more optional device information: You can read more about the CDeviceService in hb_homekit.h
    */
  }
};


/*
* @brief
*/
CHeaterCoolerService HeaterCoolerService
(
  Characteristics,
  &Host
);


/*
* @brief HomeKit provides everything you need to interact with Apple HomeKit and host a homepage.
*/
CHomeKit HomeKit
(
  Device,
  homekit_accessory_category_heater,
  &HeaterCoolerService
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
  display.clearDisplay();
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


  // Installs and configures everything for Heater.
  InstallAndSetupHeaterCooler(HomeKit, HeaterCoolerService, Host);

  // Adds a menu (WiFi) that allows the user to connect to a WiFi network.
  AddWiFiLoginMenu(HomeKit);

  // Adds standard menu items to the controller.
  AddStandardMenus(HomeKit);

  // Installs the action UI, required for web-pages.
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
