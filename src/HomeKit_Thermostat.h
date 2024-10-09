#pragma region Prolog
/*******************************************************************
$CRT 09 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Thermostat.h<< 09 Okt 2024  07:34:28 - (c) proDAD

using namespace HBHomeKit::Thermostat;
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
namespace Thermostat
{
#pragma endregion

#pragma region +Support

#pragma region EThermostatType
/* The type of the thermostat.
* @note The values are used as bit flags.
*/
enum class EThermostatType : uint16_t
{
  None              = 0x0000,
  HumiditySensor    = 0x0001,
  HumidityActor     = 0x0002,
  CoolingThreshold  = 0x0004,
  HeatingThreshold  = 0x0008,

  All = 0xffff,

  /* Useful combinations */
  Humidity = HumiditySensor | HumidityActor,
  HumiditySensor_HeatingThreshold = HumiditySensor | HeatingThreshold,

};

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

#pragma region CThermostatInfo
struct CThermostatInfo
{
  std::optional<float> TargetTemperature;
  std::optional<float> TargetHumidity;
  std::optional<HOMEKIT_TARGET_HEATING_COOLING_STATE> TargetState;
  std::optional<HOMEKIT_TEMPERATURE_DISPLAY_UNIT> DisplayUnit;
  std::optional<float> CoolingThresholdTemperature;
  std::optional<float> HeatingThresholdTemperature;

  auto DisplayUnit_OrDefault() const { return DisplayUnit.value_or(HOMEKIT_TEMPERATURE_DISPLAY_UNIT_CELSIUS); }
  auto TargetState_OrDefault() const { return TargetState.value_or(HOMEKIT_TARGET_HEATING_COOLING_STATE_OFF); }
  auto TargetTemperature_OrDefault() const { return TargetTemperature.value_or(22.0f); }
  auto TargetHumidity_OrDefault() const { return TargetHumidity.value_or(48.0f); }


  bool Assign(const CThermostatInfo& info)
  {
    bool Changed{};

    if(info.TargetTemperature)
    {
      if(!TargetTemperature || *TargetTemperature != *info.TargetTemperature)
      {
        TargetTemperature = *info.TargetTemperature;
        Changed = true;
      }
    }

    if(info.TargetHumidity)
    {
      if(!TargetHumidity || *TargetHumidity != *info.TargetHumidity)
      {
        TargetHumidity = *info.TargetHumidity;
        Changed = true;
      }
    }

    if(info.TargetState)
    {
      if(!TargetState || *TargetState != *info.TargetState)
      {
        TargetState = *info.TargetState;
        Changed = true;
      }
    }

    if(info.DisplayUnit)
    {
      if(!DisplayUnit || *DisplayUnit != *info.DisplayUnit)
      {
        DisplayUnit = *info.DisplayUnit;
        Changed = true;
      }
    }

    if(info.CoolingThresholdTemperature)
    {
      if(!CoolingThresholdTemperature || *CoolingThresholdTemperature != *info.CoolingThresholdTemperature)
      {
        CoolingThresholdTemperature = *info.CoolingThresholdTemperature;
        Changed = true;
      }
    }

    if(info.HeatingThresholdTemperature)
    {
      if(!HeatingThresholdTemperature || *HeatingThresholdTemperature != *info.HeatingThresholdTemperature)
      {
        HeatingThresholdTemperature = *info.HeatingThresholdTemperature;
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
  std::optional<HOMEKIT_CURRENT_HEATING_COOLING_STATE> HCState{};
};

#pragma endregion

#pragma region CChangeInfo
struct CChangeInfo
{
  CSensorInfo     Sensor;
  CThermostatInfo Thermostat;
  HOMEKIT_CURRENT_HEATING_COOLING_STATE CurrentState{ HOMEKIT_CURRENT_HEATING_COOLING_STATE_OFF };
  HOMEKIT_TEMPERATURE_DISPLAY_UNIT      DisplayUnit{ HOMEKIT_TEMPERATURE_DISPLAY_UNIT_CELSIUS };

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

  virtual HOMEKIT_TARGET_HEATING_COOLING_STATE GetTargetState() const = 0;

  virtual void SetTargetState(HOMEKIT_TARGET_HEATING_COOLING_STATE value) = 0;

  virtual HOMEKIT_CURRENT_HEATING_COOLING_STATE GetCurrentState() const = 0;

  virtual void SetCurrentState(HOMEKIT_CURRENT_HEATING_COOLING_STATE value) = 0;

  virtual HOMEKIT_TEMPERATURE_DISPLAY_UNIT GetDisplayUnit() const = 0;

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
  using CThermostatInfoArgs = CArgs<CThermostatInfo>;
  using CEventRecorderArgs = CArgs<EventRecorder_Ptr>;
  using CTargetTemperatureArgs = CArgs<float>;
  using CTargetHumidityArgs = CArgs<float>;
  using CHeatingThresholdTemperatureArgs = CArgs<float>;
  using CCoolingThresholdTemperatureArgs = CArgs<float>;
  using CTargetStateArgs = CArgs<HOMEKIT_TARGET_HEATING_COOLING_STATE>;
  using CTemperatureUnitArgs = CArgs<HOMEKIT_TEMPERATURE_DISPLAY_UNIT>;

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
  virtual void ReadThermostatInfo(CThermostatInfoArgs&) {}
  virtual void WriteThermostatInfo(CThermostatInfoArgs&) {}
  virtual void QueryEventRecorder(CEventRecorderArgs&) {}
  virtual void OnTargetTemperatureChanged(CTargetTemperatureArgs&) {}
  virtual void OnTargetHumidityChanged(CTargetHumidityArgs&) {}
  virtual void OnTargetStateChanged(CTargetStateArgs&) {}
  virtual void OnTemperatureUnitChanged(CTemperatureUnitArgs&) {}
  virtual void OnHeatingThresholdTemperatureChanged(CHeatingThresholdTemperatureArgs&) {}
  virtual void OnCoolingThresholdTemperatureChanged(CCoolingThresholdTemperatureArgs&) {}
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
  void OnTargetTemperatureChanged(CTargetTemperatureArgs& args) override
  {
    InvokeMethod(&IUnit::OnTargetTemperatureChanged, args);
  }
  void OnTargetHumidityChanged(CTargetHumidityArgs& args) override
  {
    InvokeMethod(&IUnit::OnTargetHumidityChanged, args);
  }
  void OnTargetStateChanged(CTargetStateArgs& args) override
  {
    InvokeMethod(&IUnit::OnTargetStateChanged, args);
  }
  void OnTemperatureUnitChanged(CTemperatureUnitArgs& args) override
  {
    InvokeMethod(&IUnit::OnTemperatureUnitChanged, args);
  }
  void OnHeatingThresholdTemperatureChanged(CHeatingThresholdTemperatureArgs& args) override
  {
    InvokeMethod(&IUnit::OnHeatingThresholdTemperatureChanged, args);
  }
  void OnCoolingThresholdTemperatureChanged(CCoolingThresholdTemperatureArgs& args) override
  {
    InvokeMethod(&IUnit::OnCoolingThresholdTemperatureChanged, args);
  }
  void ReadThermostatInfo(CThermostatInfoArgs& args) override
  {
    InvokeMethod(&IUnit::ReadThermostatInfo, args);
  }
  void WriteThermostatInfo(CThermostatInfoArgs& args) override
  {
    InvokeMethod(&IUnit::WriteThermostatInfo, args);
  }
  void QueryNextMessage(CMessageArgs& args) override
  {
    InvokeMethod(&IUnit::QueryNextMessage, args);
  }



  #pragma endregion

};

#pragma endregion


#pragma region CThermostatService
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
struct CThermostatService : public ISystem
{
  #pragma region Fields
  homekit_characteristic_t Name;
  homekit_characteristic_t CurrentTemperature;
  homekit_characteristic_t CurrentHumidity;
  homekit_characteristic_t TargetTemperature;
  homekit_characteristic_t TargetHumidity;
  homekit_characteristic_t CurrentHeatingCoolingState;
  homekit_characteristic_t TargetHeatingCoolingState;
  homekit_characteristic_t TemperatureDisplayUnit;
  homekit_characteristic_t CoolingThresholdTemperature;
  homekit_characteristic_t HeatingThresholdTemperature;

  CService<10> Service;

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
    HOMEKIT_CURRENT_HEATING_COOLING_STATE CurrentState{ HOMEKIT_CURRENT_HEATING_COOLING_STATE_OFF };

    /* Free to use by user
    */
    void* UserPtr{};

    homekit_value_t(*CurrentTemperatureGetter)(const homekit_characteristic_t*) {};
    void(*TargetTemperatureSetter)(homekit_characteristic_t*, homekit_value_t) {};

    homekit_value_t(*CurrentStateGetter)(const homekit_characteristic_t*) {};
    void(*TargetStateSetter)(homekit_characteristic_t*, homekit_value_t) {};

    void(*TemperatureUnitSetter)(homekit_characteristic_t*, homekit_value_t) {};

    /* Optional */
    homekit_value_t(*CurrentHumidityGetter)(const homekit_characteristic_t*) {};
    void(*TargetHumiditySetter)(homekit_characteristic_t*, homekit_value_t) {};

    void(*CoolingThresholdTemperatureSetter)(homekit_characteristic_t*, homekit_value_t) {};
    void(*HeatingThresholdTemperatureSetter)(homekit_characteristic_t*, homekit_value_t) {};
  };

  #pragma endregion

  #pragma region Construction
  CThermostatService(const CThermostatService&) = delete;
  CThermostatService(CThermostatService&&) = delete;

  constexpr CThermostatService(Params_t params, EThermostatType flags = EThermostatType::All) noexcept
    : Name{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.Name) }
    , CurrentTemperature{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_TEMPERATURE("Current Temperature", 0, .getter_ex = params.CurrentTemperatureGetter, .UserPtr = params.UserPtr) }
    , CurrentHumidity{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_RELATIVE_HUMIDITY("Current Humidity", 0, .getter_ex = params.CurrentHumidityGetter, .UserPtr = params.UserPtr) }
    , TargetTemperature{ HOMEKIT_DECLARE_CHARACTERISTIC_TARGET_TEMPERATURE("Target Temperature", 22, .setter_ex = params.TargetTemperatureSetter, .UserPtr = params.UserPtr) }
    , TargetHumidity{ HOMEKIT_DECLARE_CHARACTERISTIC_TARGET_RELATIVE_HUMIDITY("Target Humidity", 48, .setter_ex = params.TargetHumiditySetter, .UserPtr = params.UserPtr) }
    , CurrentHeatingCoolingState{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_HEATING_COOLING_STATE("Current Heating Cooling State", params.CurrentState, .getter_ex = params.CurrentStateGetter, .UserPtr = params.UserPtr) }
    , TargetHeatingCoolingState{ HOMEKIT_DECLARE_CHARACTERISTIC_TARGET_HEATING_COOLING_STATE("Target Heating Cooling State", 0, .setter_ex = params.TargetStateSetter, .UserPtr = params.UserPtr) }
    , TemperatureDisplayUnit{ HOMEKIT_DECLARE_CHARACTERISTIC_TEMPERATURE_DISPLAY_UNITS(HOMEKIT_TEMPERATURE_DISPLAY_UNIT_CELSIUS, .setter_ex = params.TemperatureUnitSetter, .UserPtr = params.UserPtr) }
    , CoolingThresholdTemperature{ HOMEKIT_DECLARE_CHARACTERISTIC_COOLING_THRESHOLD_TEMPERATURE("Cooling Threshold Temperature", 30, .setter_ex = params.CoolingThresholdTemperatureSetter, .UserPtr = params.UserPtr) }
    , HeatingThresholdTemperature{ HOMEKIT_DECLARE_CHARACTERISTIC_HEATING_THRESHOLD_TEMPERATURE("Heating Threshold Temperature", 20, .setter_ex = params.HeatingThresholdTemperatureSetter, .UserPtr = params.UserPtr) }
    , Service
    {
      {
        .type = HOMEKIT_SERVICE_THERMOSTAT,
        .primary = true
      },

      &Name,
      &CurrentTemperature,
      &TargetTemperature,
      &CurrentHeatingCoolingState,
      &TargetHeatingCoolingState,
      &TemperatureDisplayUnit,

      (params.CurrentHumidityGetter != nullptr && (static_cast<uint32_t>(flags) & static_cast<uint32_t>(EThermostatType::HumiditySensor)))
        ? &CurrentHumidity : nullptr,
      (params.TargetHumiditySetter != nullptr && (static_cast<uint32_t>(flags) & static_cast<uint32_t>(EThermostatType::HumidityActor)))
        ? &TargetHumidity : nullptr,
      (params.CoolingThresholdTemperatureSetter != nullptr && (static_cast<uint32_t>(flags) & static_cast<uint32_t>(EThermostatType::CoolingThreshold)))
        ? &CoolingThresholdTemperature : nullptr,
      (params.HeatingThresholdTemperatureSetter != nullptr && (static_cast<uint32_t>(flags) & static_cast<uint32_t>(EThermostatType::HeatingThreshold)))
        ? &HeatingThresholdTemperature : nullptr
    }
  {
  }

  CThermostatService(IUnit* pCtrl, EThermostatType availableFlags) noexcept
  : CThermostatService
    ({
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
      .TargetTemperatureSetter = [](homekit_characteristic_t* pC, homekit_value_t value)
      {
        auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
        if(modify_value(pC, value) && Unit)
        {
          float Temp = static_value_cast<float>(value);
          IUnit::CTargetTemperatureArgs Args = Temp;
          Unit->OnTargetTemperatureChanged(Args);
        }
      },


      //.CurrentStateGetter = [](const homekit_characteristic_t* pC)->homekit_value_t
      //{
      //  auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
      //  IUnit::CThermostatInfoArgs Args;
      //  Unit->ReadThermostatInfo(Args);
      //  return static_value_cast<HOMEKIT_CURRENT_HEATING_COOLING_STATE>(Args.Value.CurrentState_OrDefault());
      //},
      .TargetStateSetter = [](homekit_characteristic_t* pC, homekit_value_t value)
      {
        auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
        if(modify_value(pC, value) && Unit)
        {
          HOMEKIT_TARGET_HEATING_COOLING_STATE State = static_value_cast<HOMEKIT_TARGET_HEATING_COOLING_STATE>(value);
          IUnit::CTargetStateArgs Args = State;
          Unit->OnTargetStateChanged(Args);
        }
      },


      .TemperatureUnitSetter = [](homekit_characteristic_t* pC, homekit_value_t value)
      {
        auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
        if(modify_value(pC, value) && Unit)
        {
          HOMEKIT_TEMPERATURE_DISPLAY_UNIT State = static_value_cast<HOMEKIT_TEMPERATURE_DISPLAY_UNIT>(value);
          IUnit::CTemperatureUnitArgs Args = State;
          Unit->OnTemperatureUnitChanged(Args);
        }
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
      .TargetHumiditySetter = [](homekit_characteristic_t* pC, homekit_value_t value)
      {
        auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
        if(modify_value(pC, value) && Unit)
        {
          float Temp = static_value_cast<float>(value);
          IUnit::CTargetHumidityArgs Args = Temp;
          Unit->OnTargetHumidityChanged(Args);
        }
      },


      .CoolingThresholdTemperatureSetter = [](homekit_characteristic_t* pC, homekit_value_t value)
      {
        auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
        if(modify_value(pC, value) && Unit)
        {
          float Temp = static_value_cast<float>(value);
          IUnit::CCoolingThresholdTemperatureArgs Args = Temp;
          Unit->OnCoolingThresholdTemperatureChanged(Args);
        }
      },
      .HeatingThresholdTemperatureSetter = [](homekit_characteristic_t* pC, homekit_value_t value)
      {
        auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
        if(modify_value(pC, value) && Unit)
        {
          float Temp = static_value_cast<float>(value);
          IUnit::CHeatingThresholdTemperatureArgs Args = Temp;
          Unit->OnHeatingThresholdTemperatureChanged(Args);
        }
      }

    }, availableFlags)
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

  #pragma region GetTargetState
  HOMEKIT_TARGET_HEATING_COOLING_STATE GetTargetState() const override
  {
    return static_value_cast<HOMEKIT_TARGET_HEATING_COOLING_STATE>(&TargetHeatingCoolingState);
  }

  #pragma endregion

  #pragma region GetCurrentState
  HOMEKIT_CURRENT_HEATING_COOLING_STATE GetCurrentState() const override
  {
    return static_value_cast<HOMEKIT_CURRENT_HEATING_COOLING_STATE>(&CurrentHeatingCoolingState);
  }
  #pragma endregion


  #pragma region SetCurrentState
  void SetCurrentState(HOMEKIT_CURRENT_HEATING_COOLING_STATE value) override
  {
    modify_value_and_notify(&CurrentHeatingCoolingState, value);
  }
  #pragma endregion

  #pragma region SetTargetState
  void SetTargetState(HOMEKIT_TARGET_HEATING_COOLING_STATE value) override
  {
    /* Prevent the implicit call of Unit->OnTargetStateChanged() by setting UserPtr to null.
     * This is important because a call to OnTriggered() by OnTargetStateChanged()
     * will cause OnTargetStateChanged() to be called again.
    */
    void* UserPtr = std::exchange(TargetHeatingCoolingState.UserPtr, nullptr);
    modify_value_and_notify(&TargetHeatingCoolingState, value);
    TargetHeatingCoolingState.UserPtr = UserPtr;
  }

  #pragma endregion

  #pragma region GetDisplayUnit
  HOMEKIT_TEMPERATURE_DISPLAY_UNIT GetDisplayUnit() const override
  {
    return static_value_cast<HOMEKIT_TEMPERATURE_DISPLAY_UNIT>(&TemperatureDisplayUnit);
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
void InstallVarsAndCmds(CController& c, CThermostatService& svr, CHost& host);


/*
* @brief Installs and configures everything for CThermostatService.
* @note: This function must be invoked in setup().
*/
inline void InstallAndSetupThermostat(CController& ctrl, CThermostatService& svr, CHost& host)
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
} // namespace Thermostat
} // namespace HBHomeKit
#pragma endregion
