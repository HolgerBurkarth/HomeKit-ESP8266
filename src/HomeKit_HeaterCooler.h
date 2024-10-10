#pragma region Prolog
/*******************************************************************
$CRT 10 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_HeaterCooler.h<< 10 Okt 2024  16:46:54 - (c) proDAD

using namespace HBHomeKit::HeaterCooler;
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling: aht
#pragma endregion
#pragma region Includes
#pragma once

#include <hb_homekit.h>

namespace HBHomeKit
{
namespace HeaterCooler
{
#pragma endregion

#pragma region +Support

#pragma region ECharacteristicFlags
/*
* @note The values are used as bit flags.
*/
enum class ECharacteristicFlags : uint16_t
{
  None              = 0x0000,
  HumiditySensor    = 0x0001,
  HumidityActor     = 0x0002,
  CoolingThreshold  = 0x0004,
  HeatingThreshold  = 0x0008,


  /* Useful combinations */

  Heater = HeatingThreshold,
  Heater_HumiditySensor = Heater | HumiditySensor,

};

constexpr inline bool operator & (ECharacteristicFlags a, ECharacteristicFlags b)
{
  return (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) != 0;
}

#pragma endregion

#pragma region CSensorInfo
struct CSensorInfo
{
  std::optional<float> Temperature;
  std::optional<float> Humidity;

  bool Assign(const CSensorInfo& info)
  {
    bool Changed{};

    if(info.Temperature)
    {
      if(!Temperature || *Temperature != *info.Temperature)
      {
        Temperature = *info.Temperature;
        Changed = true;
      }
    }

    if(info.Humidity)
    {
      if(!Humidity || *Humidity != *info.Humidity)
      {
        Humidity = *info.Humidity;
        Changed = true;
      }
    }

    return Changed;
  }

};
#pragma endregion

#pragma region CMessage
struct CMessage
{
  std::optional<HOMEKIT_CURRENT_HEATER_COOLER_STATE> HCState{};
};

#pragma endregion

#pragma region CChangeInfo
struct CChangeInfo
{
  ECharacteristicFlags Flags{};
  CSensorInfo Sensor;
  float       CoolingThresholdTemperature{};
  float       HeatingThresholdTemperature{};

  HOMEKIT_TEMPERATURE_DISPLAY_UNIT      DisplayUnit{ HOMEKIT_TEMPERATURE_DISPLAY_UNIT_CELSIUS };
  HOMEKIT_CURRENT_HEATER_COOLER_STATE   CurrentState{ HOMEKIT_CURRENT_HEATER_COOLER_STATE_INACTIVE };
  HOMEKIT_TARGET_HEATER_COOLER_STATE    TargetState{ HOMEKIT_TARGET_HEATER_COOLER_STATE_AUTO };
  HOMEKIT_CHARACTERISTIC_STATUS         Active{ HOMEKIT_STATUS_INACTIVE };

  bool Changed{};
};

#pragma endregion


#pragma region CFixPoint
template<typename T, int TShift>
struct CFixPoint
{
  T Value{};
  static constexpr int Shift = TShift;

  constexpr CFixPoint() = default;
  constexpr CFixPoint(T value) : Value(value) {}
  constexpr CFixPoint(float v) { assign(v); }

  constexpr void assign(float v)
  {
    v = std::clamp(v, min(), max());
    Value = static_cast<T>(v * (1 << Shift));
  }

  constexpr static float max() { return std::numeric_limits<T>::max() / (1 << Shift); }
  constexpr static float min() { return std::numeric_limits<T>::min() / (1 << Shift); }


  constexpr operator T() const { return Value; }
  constexpr operator float() const { return static_cast<float>(Value) / (1 << Shift); }

  constexpr CFixPoint& operator=(T value)
  {
    Value = value;
    return *this;
  }
  constexpr CFixPoint& operator=(float value)
  {
    assign(value);
    return *this;
  }


  constexpr CFixPoint& operator=(const CFixPoint& value)
  {
    Value = value.Value;
    return *this;
  }

};

using fix16_16_t = CFixPoint<int32_t, 16>;
using fix10_6_t = CFixPoint<int16_t, 6>;
using fix8_8_t = CFixPoint<int16_t, 8>;

#pragma endregion


#pragma region CEventRecorder
struct CEventRecorder
{
  #pragma region Types
  struct CEvent // 8 bytes
  {
    #pragma region Fields
    minute16_t  Time{}; // 16bit minutes relative to device start
    fix8_8_t    Temperature{};
    fix10_6_t   Humidity{};

    bool HasTemperature : 1;
    bool HasHumidity    : 1;

    #pragma endregion

    #pragma region Construction
    CEvent() = default;
    CEvent(const CSensorInfo& info)
    {
      Time.SetNow();
      if(info.Temperature)
      {
        Temperature = *info.Temperature;
        HasTemperature = true;
      }
      if(info.Humidity)
      {
        Humidity = *info.Humidity;
        HasHumidity = true;
      }
    }

    #pragma endregion

    #pragma region Methods

    #pragma region ToInfo
    CSensorInfo ToInfo() const
    {
      CSensorInfo Info;
      if(HasTemperature)
        Info.Temperature = Temperature;
      if(HasHumidity)
        Info.Humidity = Humidity;
      return Info;
    }

    #pragma endregion

    #pragma region ToString
    void ToString(char* pBuf, size_t bufLen) const
    {
      size_t Len;
      tm TM;
      smart_gmtime(&TM, Time);
      strftime(pBuf, bufLen, "%Y-%m-%dT%H:%M:%SZ;", &TM);
      pBuf[bufLen-1] = 0;
      Len = strlen(pBuf);
      pBuf += Len;
      bufLen -= Len;

      if(HasTemperature)
      {
        snprintf_P(pBuf, bufLen, PSTR("T:%0.2f;"), static_cast<float>(Temperature));
        pBuf[bufLen - 1] = 0;
        Len = strlen(pBuf);
        pBuf += Len;
        bufLen -= Len;
      }
      if(HasHumidity)
      {
        snprintf_P(pBuf, bufLen, PSTR("H:%0.2f;"), static_cast<float>(Humidity));
        pBuf[bufLen - 1] = 0;
        Len = strlen(pBuf);
        pBuf += Len;
        bufLen -= Len;
      }
    }

    #pragma endregion


    //END CEvent
    #pragma endregion

  };

  #pragma endregion

  #pragma region Fields
  std::vector<CEvent> mEntries;
  size_t MaxEntries{ 288 }; // 24h by 5min steps | sizeof(CEvent) * 288 = 2304 bytes
  int RecordIntervalSec{ 60 }; // 1 minute

  #pragma endregion

  #pragma region clear
  void clear()
  {
    mEntries.clear();
  }

  #pragma endregion

  #pragma region reserve
  void reserve(size_t count)
  {
    MaxEntries = count;
    mEntries.reserve(count);
  }
  #pragma endregion

  #pragma region push_back
  void push_back(const CSensorInfo& info)
  {
    while(mEntries.size() >= MaxEntries)
      mEntries.erase(mEntries.begin());
    mEntries.push_back(CEvent(info));
  }

  #pragma endregion

  #pragma region EntriesEmitter
  /*
  * @return The emitter for the entries:
  */
  CTextEmitter EntriesEmitter() const
  {
    return [&Entries = mEntries](Stream& out)
      {
        char Buf[64];
        for(const auto& e : Entries)
        {
          e.ToString(Buf, sizeof(Buf));
          out << Buf << F("\n");
        }
      };
  }

  #pragma endregion


};

using EventRecorder_Ptr = std::shared_ptr<CEventRecorder>;

/* Creates a shared pointer from a static object that never calls a delete.
* @example
*  CEventRecorder EventRecorder;
*  auto Ptr = MakeStaticPtr(EventRecorder)
*/
inline EventRecorder_Ptr MakeStaticPtr(CEventRecorder& pV)
{
  return EventRecorder_Ptr(&pV, [](CEventRecorder*) {});
}

//END CEventRecorder
#pragma endregion


//END Support
#pragma endregion

#pragma region ISystem
/* Get and set the HomeKit states.
* @note The value is sent to HomeKit when it is set.
*/
struct ISystem
{
  /* @note Do not use a virtual destructor to enable constexpr
   *       for the definition of the service.
   * virtual ~ISystem() = default;
  */

  virtual HOMEKIT_TARGET_HEATER_COOLER_STATE GetTargetState() const = 0;

  virtual void SetTargetState(HOMEKIT_TARGET_HEATER_COOLER_STATE value) = 0;

  virtual HOMEKIT_CURRENT_HEATER_COOLER_STATE GetCurrentState() const = 0;

  virtual void SetCurrentState(HOMEKIT_CURRENT_HEATER_COOLER_STATE value) = 0;

  virtual HOMEKIT_TEMPERATURE_DISPLAY_UNIT GetDisplayUnit() const = 0;

  virtual ECharacteristicFlags GetFlags() const = 0;
  virtual HOMEKIT_CHARACTERISTIC_STATUS GetActive() const = 0;
  
  virtual float GetCoolingThresholdTemperature() const = 0;
  virtual float GetHeatingThresholdTemperature() const = 0;


};

#pragma endregion

#pragma region IUnit
/* The base class for all implementations of the thermostat.
*
* A method is always called with a CArgs argument. Where CArgs::Handled
* indicates whether the method was processed.
*
* @note that CArgs::Handled = false must be set before the call.
* @note Last added units are called first.
*/
struct IUnit
{
  #pragma region Types
  using CMessageArgs = CArgs<CMessage>;
  using CSensorInfoArgs = CArgs<CSensorInfo>;
  using CEventRecorderArgs = CArgs<EventRecorder_Ptr>;

  struct CSetupArgs
  {
    CController* Ctrl{};
    ISystem* System{};
    IUnit* Super{};
  };


  #pragma region CSuperInvoke
  /* Call another method within a member method using
  * the super reference without triggering a recursion.
  * @see CContinuousPositionRecorderUnit
  */
  class CSuperInvoke
  {
    int mForbidEnterCount{};
  public:

    /* Call a method of the super class.
    * @param pSuper The super class
    * @param method The method to call
    * @param args The arguments to pass to the method
    * @return True if the method was called and the arguments were handled.
    */
    template<typename T>
    bool operator () (IUnit* pSuper, void (IUnit::* method)(CArgs<T>&), CArgs<T>& args)
    {
      if(mForbidEnterCount == 0 && !args)
      {
        ++mForbidEnterCount;
        (pSuper->*method)(args);
        --mForbidEnterCount;
        return args.Handled;
      }
      return false;
    }
  };

  #pragma endregion

  //END Types
  #pragma endregion

  #pragma region Construction
  virtual ~IUnit() = default;

  #pragma endregion

  #pragma region Virtual Methods
  virtual void Setup(const CSetupArgs&) {}
  virtual void Start(CVoidArgs&) {}
  virtual void ReadSensorInfo(CSensorInfoArgs&) {}
  virtual void WriteSensorInfo(CSensorInfoArgs&) {}
  virtual void QueryEventRecorder(CEventRecorderArgs&) {}
  virtual void QueryNextMessage(CMessageArgs&) {}


  //END Virtual Methods
  #pragma endregion

};

using IUnit_Ptr = std::shared_ptr<IUnit>;

/* Creates a shared pointer from a static object that never calls a delete.
* @example
*  CMyUnit MyUnit;
*  auto Ptr = MakeStaticPtr(MyUnit)
*/
inline IUnit_Ptr MakeStaticPtr(IUnit& pUnit)
{
  return IUnit_Ptr(&pUnit, [](IUnit*) {});
}

#pragma endregion

#pragma region CUnitBase
/* The base class for all implementations of IUnit.
*/
struct CUnitBase : IUnit
{
  CController* Ctrl{};
  IUnit* Super{};
  ISystem* System{};

  void Setup(const CSetupArgs& args) override
  {
    Ctrl = args.Ctrl;
    Super = args.Super;
    System = args.System;
  }

};
#pragma endregion

#pragma region CHost
class CHost : public IUnit
{
  #pragma region Fields
  std::array< IUnit_Ptr, 8> Units;

  #pragma endregion

  #pragma region Construction
public:
  CHost() = default;
  CHost(IUnit_Ptr unit)
    : Units{ std::move(unit) }
  {
  }
  CHost(IUnit_Ptr unit0, IUnit_Ptr unit1)
    : Units{ std::move(unit0), std::move(unit1) }
  {
  }
  CHost(IUnit_Ptr unit0, IUnit_Ptr unit1, IUnit_Ptr unit2)
    : Units{ std::move(unit0), std::move(unit1), std::move(unit2) }
  {
  }
  CHost(IUnit_Ptr unit0, IUnit_Ptr unit1, IUnit_Ptr unit2, IUnit_Ptr unit3)
    : Units{ std::move(unit0), std::move(unit1), std::move(unit2), std::move(unit3) }
  {
  }
  CHost(IUnit_Ptr unit0, IUnit_Ptr unit1, IUnit_Ptr unit2, IUnit_Ptr unit3, IUnit_Ptr unit4)
    : Units{ std::move(unit0), std::move(unit1), std::move(unit2), std::move(unit3), std::move(unit4) }
  {
  }
  CHost(IUnit_Ptr unit0, IUnit_Ptr unit1, IUnit_Ptr unit2, IUnit_Ptr unit3, IUnit_Ptr unit4, IUnit_Ptr unit5)
    : Units{ std::move(unit0), std::move(unit1), std::move(unit2), std::move(unit3), std::move(unit4), std::move(unit5) }
  {
  }
  CHost(IUnit_Ptr unit0, IUnit_Ptr unit1, IUnit_Ptr unit2, IUnit_Ptr unit3, IUnit_Ptr unit4, IUnit_Ptr unit5, IUnit_Ptr unit6)
    : Units{ std::move(unit0), std::move(unit1), std::move(unit2), std::move(unit3), std::move(unit4), std::move(unit5), std::move(unit6) }
  {
  }
  CHost(IUnit_Ptr unit0, IUnit_Ptr unit1, IUnit_Ptr unit2, IUnit_Ptr unit3, IUnit_Ptr unit4, IUnit_Ptr unit5, IUnit_Ptr unit6, IUnit_Ptr unit7)
    : Units{ std::move(unit0), std::move(unit1), std::move(unit2), std::move(unit3), std::move(unit4), std::move(unit5), std::move(unit6), std::move(unit7) }
  {
  }

  #pragma endregion

  #pragma region Public Methods

  //END Public Methods
  #pragma endregion

  #pragma region Private Methods
private:
  template<typename T>
  bool InvokeMethod(void (IUnit::* methode)(CArgs<T>&), CArgs<T>& args)
  {
    for(int i = Units.size(); --i >= 0;)
    {
      auto& u = Units[i];
      if(u)
      {
        (u.get()->*methode)(args);
        if(args.Handled)
          return true;
      }
    }
    return false;
  }

  #pragma endregion

  #pragma region Override Methods
public:
  void Setup(const CSetupArgs& args) override
  {
    CSetupArgs Args = args;

    Args.Super = this;

    for(auto& u : Units)
    {
      if(u)
        u->Setup(Args);
    }
  }
  void Start(CVoidArgs& args)
  {
    InvokeMethod(&IUnit::Start, args);
  }
  void ReadSensorInfo(CSensorInfoArgs& args) override
  {
    InvokeMethod(&IUnit::ReadSensorInfo, args);
  }
  void WriteSensorInfo(CSensorInfoArgs& args) override
  {
    InvokeMethod(&IUnit::WriteSensorInfo, args);
  }
  void QueryEventRecorder(CEventRecorderArgs& args) override
  {
    InvokeMethod(&IUnit::QueryEventRecorder, args);
  }
  void QueryNextMessage(CMessageArgs& args) override
  {
    InvokeMethod(&IUnit::QueryNextMessage, args);
  }



  #pragma endregion

};

#pragma endregion


#pragma region CHeaterCoolerService
/* A HomeKit A. Humidity Temperature service
* @example
*
CThermostatService AHT
({
    .CurrentTemperatureGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
    {
      return static_value_cast<float>(GetTemperature());
    },
    .CurrentHumidityGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
    {
      return static_value_cast<float>(GetHumidity());
    }
});

*/
struct CHeaterCoolerService : public ISystem
{
  #pragma region Fields
  const ECharacteristicFlags Flags;
  homekit_characteristic_t Name;
  homekit_characteristic_t ActiveState;
  homekit_characteristic_t CurrentTemperature;
  homekit_characteristic_t CurrentHeaterCoolerState;
  homekit_characteristic_t TargetHeaterCoolerState;
  homekit_characteristic_t CurrentHumidity;
  homekit_characteristic_t TemperatureDisplayUnit;
  homekit_characteristic_t CoolingThresholdTemperature;
  homekit_characteristic_t HeatingThresholdTemperature;

  CService<9> Service;

  #pragma endregion

  #pragma region Params_t
  /*
  * @example
      .TemperatureGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
      {
        return static_value_cast<float>(GetTemperature());
      },
      .HumidityGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
      {
        return static_value_cast<float>(GetHumidity());
      }
  */
  struct Params_t
  {
    /* The name of the sensor value
    */
    const char* Name{ "Thermostat" };

    /* Free to use by user
    */
    void* UserPtr{};

    homekit_value_t(*CurrentTemperatureGetter)(const homekit_characteristic_t*) {};
    homekit_value_t(*CurrentHumidityGetter)(const homekit_characteristic_t*) {};
  };

  #pragma endregion

  #pragma region Construction
  CHeaterCoolerService(const CHeaterCoolerService&) = delete;
  CHeaterCoolerService(CHeaterCoolerService&&) = delete;

  constexpr CHeaterCoolerService(ECharacteristicFlags flags, Params_t params) noexcept
    : Flags{ flags }
    , Name{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.Name) }
    , CurrentTemperature{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_TEMPERATURE("Current Temperature", 22, .getter_ex = params.CurrentTemperatureGetter, .UserPtr = params.UserPtr) }
    , CurrentHumidity{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_RELATIVE_HUMIDITY("Current Humidity", 48, .getter_ex = params.CurrentHumidityGetter, .UserPtr = params.UserPtr) }
    , TemperatureDisplayUnit{ HOMEKIT_DECLARE_CHARACTERISTIC_TEMPERATURE_DISPLAY_UNITS(HOMEKIT_TEMPERATURE_DISPLAY_UNIT_CELSIUS, .UserPtr = params.UserPtr) }
    , CoolingThresholdTemperature{ HOMEKIT_DECLARE_CHARACTERISTIC_COOLING_THRESHOLD_TEMPERATURE("Cooling Threshold Temperature", 30, .UserPtr = params.UserPtr) }
    , HeatingThresholdTemperature{ HOMEKIT_DECLARE_CHARACTERISTIC_HEATING_THRESHOLD_TEMPERATURE("Heating Threshold Temperature", 20, .UserPtr = params.UserPtr) }
    , ActiveState{ HOMEKIT_DECLARE_CHARACTERISTIC_ACTIVE(HOMEKIT_STATUS_ACTIVE, .UserPtr = params.UserPtr) }
    , CurrentHeaterCoolerState{ HOMEKIT_CURRENT_HEATER_COOLER_STATE(0, .UserPtr = params.UserPtr) }
    , TargetHeaterCoolerState{ HOMEKIT_TARGET_HEATER_COOLER_STATE( 0, .UserPtr = params.UserPtr) }

    , Service
    {
      {
        .type = HOMEKIT_SERVICE_HEATER_COOLER,
        .primary = true
      },

      &Name,
      &TemperatureDisplayUnit,
      &CurrentTemperature,
      &CurrentHeaterCoolerState,
      &TargetHeaterCoolerState,
      &ActiveState,

      (params.CurrentHumidityGetter != nullptr && (flags & ECharacteristicFlags::HumiditySensor))
        ? &CurrentHumidity : nullptr,
      (flags & ECharacteristicFlags::CoolingThreshold)
        ? &CoolingThresholdTemperature : nullptr,
      (flags & ECharacteristicFlags::HeatingThreshold)
        ? &HeatingThresholdTemperature : nullptr
    }
  {
  }

  CHeaterCoolerService(ECharacteristicFlags flags, IUnit* pCtrl) noexcept
  : CHeaterCoolerService
    (
      flags,
      {
        .UserPtr = reinterpret_cast<void*>(pCtrl),

        .CurrentTemperatureGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
        {
          auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
          IUnit::CSensorInfoArgs Args;
          Unit->ReadSensorInfo(Args);
          if(Args.Value.Temperature)
          {
            /*
            * There are two advantages to using modify_value to change the underlying `value'
            * rather than returning the new value directly:
            * 1. A log entry is added when a change is made.
            * 2. Functions that only look at the underlying 'value',
            *    such as 'to_string', always take the current value.
            */
            modify_value(pC, *Args.Value.Temperature);
          }
          return pC->value;
        },

        .CurrentHumidityGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
        {
          auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
          IUnit::CSensorInfoArgs Args;
          Unit->ReadSensorInfo(Args);
          if(Args.Value.Humidity)
          {
            /*
            * There are two advantages to using modify_value to change the underlying `value'
            * rather than returning the new value directly:
            * 1. A log entry is added when a change is made.
            * 2. Functions that only look at the underlying 'value',
            *    such as 'to_string', always take the current value.
            */
            modify_value(pC, *Args.Value.Humidity);
          }
          return pC->value;
        },

      }
    )
  {
  }

  #pragma endregion

  #pragma region Properties

  #pragma region GetUnit
  IUnit* GetUnit() const noexcept
  {
    return static_cast<IUnit*>(CurrentTemperature.UserPtr);
  }
  #pragma endregion


  //END Properties
  #pragma endregion

  #pragma region ISystem-Methods (overrides)

  #pragma region GetFlags
  ECharacteristicFlags GetFlags() const override
  {
    return Flags;
  }
  #pragma endregion

  #pragma region GetTargetState
  HOMEKIT_TARGET_HEATER_COOLER_STATE GetTargetState() const override
  {
    return static_value_cast<HOMEKIT_TARGET_HEATER_COOLER_STATE>(&TargetHeaterCoolerState);
  }

  #pragma endregion
  #pragma region GetCurrentState
  HOMEKIT_CURRENT_HEATER_COOLER_STATE GetCurrentState() const override
  {
    return static_value_cast<HOMEKIT_CURRENT_HEATER_COOLER_STATE>(&CurrentHeaterCoolerState);
  }
  #pragma endregion
  #pragma region SetCurrentState
  void SetCurrentState(HOMEKIT_CURRENT_HEATER_COOLER_STATE value) override
  {
    modify_value_and_notify(&CurrentHeaterCoolerState, value);
  }
  #pragma endregion
  #pragma region SetTargetState
  void SetTargetState(HOMEKIT_TARGET_HEATER_COOLER_STATE value) override
  {
    /* Prevent the implicit call of Unit->OnTargetStateChanged() by setting UserPtr to null.
     * This is important because a call to OnTriggered() by OnTargetStateChanged()
     * will cause OnTargetStateChanged() to be called again.
    */
    {
      void* UserPtr = std::exchange(TargetHeaterCoolerState.UserPtr, nullptr);
      modify_value_and_notify(&TargetHeaterCoolerState, value);
      TargetHeaterCoolerState.UserPtr = UserPtr;
    }
  }

  #pragma endregion
  #pragma region GetActive 
  HOMEKIT_CHARACTERISTIC_STATUS GetActive() const override
  {
    return static_value_cast<HOMEKIT_CHARACTERISTIC_STATUS>(&ActiveState);
  }
  #pragma endregion

  #pragma region GetDisplayUnit
  HOMEKIT_TEMPERATURE_DISPLAY_UNIT GetDisplayUnit() const override
  {
    return static_value_cast<HOMEKIT_TEMPERATURE_DISPLAY_UNIT>(&TemperatureDisplayUnit);
  }
  #pragma endregion
  #pragma region GetCoolingThresholdTemperature
  float GetCoolingThresholdTemperature() const override
  {
    return static_value_cast<float>(&CoolingThresholdTemperature);
  }

  #pragma endregion
  #pragma region GetHeatingThresholdTemperature
  float GetHeatingThresholdTemperature() const override
  {
    return static_value_cast<float>(&HeatingThresholdTemperature);
  }

  #pragma endregion


  //END ISystem-Methods
  #pragma endregion

  #pragma region Methods

  #pragma region Setup
  /*
  * @note This method must be called in the setup() function.
  */
  void Setup()
  {
    auto Unit = GetUnit();
    if(Unit)
    {
      if(!CHomeKit::Singleton)
      {
        ERROR("CThermostatService::Setup: CHomeKit not yet created");
        return;
      }

      Unit->Setup
      (
        {
          .Ctrl = &CHomeKit::Singleton->Controller,
          .System = this
        }
      );
      {
        CVoidArgs Args;
        Unit->Start(Args);
      }
    }
  }

  #pragma endregion


  //END Methods
  #pragma endregion

  #pragma region Operators
  constexpr const homekit_service_t* operator&() const noexcept
  {
    return &Service;
  }

  #pragma endregion

};

#pragma endregion


#pragma region +Functions

#pragma region to_string
const char* to_string(HOMEKIT_TARGET_HEATING_COOLING_STATE state);
const char* to_string(HOMEKIT_CURRENT_HEATING_COOLING_STATE state);
const char* to_string(HOMEKIT_TEMPERATURE_DISPLAY_UNIT state);

#pragma endregion


#pragma region Unit - Functions

#pragma region MakeControlUnit
/*
* @brief
*/
IUnit_Ptr MakeControlUnit();

#pragma endregion

#pragma region MakeContinuousReadSensorUnit
/*
* @brief
* @example
    MakeContinuousReadSensorUnit
    ( 
      1000,
      [
        Temperature = CKalman1DFilterF{ .R = 5e-3f },
        Humidity    = CKalman1DFilterF{ .R = 5e-3f }
      ]() mutable
      {
        //extern Adafruit_AHTX0 aht;
        sensors_event_t humidity, temp;
        aht.getEvent(&humidity, &temp);

        CSensorInfo Info;
        Info.Temperature = Temperature.Update(temp.temperature);
        Info.Humidity = Humidity.Update(humidity.relative_humidity);
        return Info;
      }
    )

*/
IUnit_Ptr MakeContinuousReadSensorUnit(uint32_t intervalMS, std::function<CSensorInfo(void)> func);

#pragma endregion

#pragma region MakeOnChangedUnit
/*
* @brief
* @example
    MakeOnChangedUnit([](const CChangeInfo& info)
      {
        display.clearDisplay();

        if(info.Temperature)
        {
          display.setCursor(0, 20);
          display.print("Temp: ");
          display.print(*info.Temperature);
          display.println(" C");
        }
        if(info.Humidity)
        {
          display.setCursor(0, 45);
          display.print("Hum:  ");
          display.print(*info.Humidity);
          display.println(" %");
        }
        display.display();

      })
*/
IUnit_Ptr MakeOnChangedUnit(uint32_t intervalMS, std::function<void(const CChangeInfo&)> func);

#pragma endregion

#pragma region MakeContinuousEventRecorderUnit
/*
* @brief The unit records the door position/state and motor state in
* a continuous event recorder.
*/
IUnit_Ptr MakeContinuousEventRecorderUnit(int intervalSec);
#
#pragma endregion


//END Unit - Functions
#pragma endregion

#pragma region Install - Functions

/*
* @brief Adds the Start, Settings, HomeKit menu items to the controller.
*/
void AddMenuItems(CController& c);

/*
* @brief Installs the variables and commands for the HomeKit.
* @vars TEMPERATURE, HUMIDITY
*/
void InstallVarsAndCmds(CController& c, CHeaterCoolerService& svr, CHost& host);


/*
* @brief Installs and configures everything for CThermostatService.
* @note: This function must be invoked in setup().
*/
inline void InstallAndSetupThermostat(CController& ctrl, CHeaterCoolerService& svr, CHost& host)
{
  svr.Setup();
  InstallVarsAndCmds(ctrl, svr, host);
  AddMenuItems(ctrl);
}



//END Install - Functions
#pragma endregion

#pragma region JavaScript - Functions
CTextEmitter Command_JavaScript();
CTextEmitter EventChart_JavaScript();

#pragma endregion

#pragma region HtmlBody - Functions
CTextEmitter MainPage_HtmlBody();
CTextEmitter HomeKit_HtmlBody();

#pragma endregion



//END +Functions
#pragma endregion


#pragma region Epilog
} // namespace HeaterCooler
} // namespace HBHomeKit
#pragma endregion
