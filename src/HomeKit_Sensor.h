#pragma region Prolog
/*******************************************************************
$CRT 05 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Sensor.h<< 11 Okt 2024  15:52:17 - (c) proDAD

using namespace HBHomeKit::Sensor;
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
namespace Sensor
{
#pragma endregion

#pragma region +Support
using COptionalFloat = std::optional<float>;

#pragma region ECharacteristicFlags
/* The type of the sensor.
* @note The values are used as bit flags.
*/
enum class ECharacteristicFlags : uint16_t
{
  None        = 0x0000,
  Temperature = 0x0001,
  Humidity    = 0x0002,
  Event       = 0x0080, // see EEventType

  /* Useful combinations */
  Temperature_Humidity = Temperature | Humidity,

};

constexpr inline bool operator & (ECharacteristicFlags a, ECharacteristicFlags b)
{
  return (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) != 0;
}
constexpr inline ECharacteristicFlags operator | (ECharacteristicFlags a, ECharacteristicFlags b)
{
  return static_cast<ECharacteristicFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

#pragma endregion

#pragma region EEventType
enum class EEventType : uint8_t
{
  None,
  SmokeDetected,
  CarbonMonoxide,
  CarbonDioxide,
};

#pragma endregion

#pragma region CSensorInfo
struct CSensorInfo
{
  COptionalFloat  Temperature;
  COptionalFloat  Humidity;

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

#pragma region CEventInfo
struct CEventInfo
{
  EEventType      Event;
  homekit_value_t Value{};

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

  virtual ECharacteristicFlags GetFlags() const = 0;
  
  virtual void SetEventValue(homekit_value_t) = 0;

};

#pragma endregion

#pragma region IUnit
/* The base class for all implementations of the sensor.
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
  using CSensorInfoArgs = CArgs<CSensorInfo>;
  using CEventRecorderArgs = CArgs<EventRecorder_Ptr>;
  using CEventInfoArgs = CArgs<CEventInfo>;

  struct CSetupArgs
  {
    CController* Ctrl{};
    IUnit* Super{};
    ISystem* System{};
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
  virtual void OnEvent(CEventInfoArgs&) {}

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
  void OnEvent(CEventInfoArgs& args) override
  {
    InvokeMethod(&IUnit::OnEvent, args);
  }

  #pragma endregion

};

#pragma endregion


#pragma region CSensorService
/* A HomeKit A. Humidity Temperature service
* @example
*
CSensorService AHT
({
    .TemperatureGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
    {
      return static_value_cast<float>(GetTemperature());
    },
    .HumidityGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
    {
      return static_value_cast<float>(GetHumidity());
    }
});

*/
struct CSensorService : public ISystem
{
  #pragma region Fields
  const ECharacteristicFlags Flags;
  homekit_characteristic_t TemperatureName;
  homekit_characteristic_t HumidityName;
  homekit_characteristic_t Temperature;
  homekit_characteristic_t Humidity;
  homekit_characteristic_t EventName;
  homekit_characteristic_t Event;

  CService<2> TemperatureService;
  CService<2> HumidityService;
  CService<2> EventService;

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
    const char* TemperatureName{ "Temperature" };
    const char* HumidityName{ "Humidity" };
    const char* EventName{ "Event" };

    /* Free to use by user
    */
    void* UserPtr{};

    EEventType Event{};

    /* Optional */
    homekit_value_t(*TemperatureGetter)(const homekit_characteristic_t*) {};
    /* Optional */
    homekit_value_t(*HumidityGetter)(const homekit_characteristic_t*) {};
  };

  #pragma endregion

  #pragma region Construction
  CSensorService(const CSensorService&) = delete;
  CSensorService(CSensorService&&) = delete;

  #pragma region MakeCharacteristicEvent
  constexpr static homekit_characteristic_t MakeCharacteristicEvent(EEventType e)
  {
    switch(e)
    {
      case EEventType::SmokeDetected: return { HOMEKIT_DECLARE_CHARACTERISTIC_SMOKE_DETECTED(0) };
      case EEventType::CarbonMonoxide: return { HOMEKIT_DECLARE_CHARACTERISTIC_CARBON_MONOXIDE_DETECTED(0) };
      case EEventType::CarbonDioxide: return { HOMEKIT_DECLARE_CHARACTERISTIC_CARBON_DIOXIDE_DETECTED(0) };
    }
    return {};
  }

  #pragma endregion

  #pragma region MakeEventService
  constexpr static const char* MakeEventService(EEventType e)
  {
    switch(e)
    {
      case EEventType::SmokeDetected: return HOMEKIT_SERVICE_SMOKE_SENSOR;
      case EEventType::CarbonMonoxide: return HOMEKIT_SERVICE_CARBON_MONOXIDE_SENSOR;
      case EEventType::CarbonDioxide: return HOMEKIT_SERVICE_CARBON_DIOXIDE_SENSOR;
    }
    return {};
  }

  #pragma endregion



  constexpr CSensorService(ECharacteristicFlags flags, Params_t params) noexcept
    : Flags{ flags }
    , TemperatureName{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.TemperatureName) }
    , HumidityName{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.HumidityName) }
    , EventName{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.EventName) }
    , Temperature{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_TEMPERATURE(params.TemperatureName, 0, .getter_ex = params.TemperatureGetter, .UserPtr = params.UserPtr) }
    , Humidity{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_RELATIVE_HUMIDITY(params.HumidityName, 0, .getter_ex = params.HumidityGetter, .UserPtr = params.UserPtr) }
    , Event{ MakeCharacteristicEvent(params.Event)}
    , TemperatureService
    {
      {
        .type = HOMEKIT_SERVICE_TEMPERATURE_SENSOR,
        .primary = true
      },

      &TemperatureName,
      &Temperature
    }
    , HumidityService
    {
      {
        .type = HOMEKIT_SERVICE_HUMIDITY_SENSOR,
        .primary = true
      },

      &HumidityName,
      &Humidity
    }
    , EventService
    {
      {
        .type = MakeEventService(params.Event),
        .primary = true
      },

      &EventName,
      &Event
    }

  {
  }


  CSensorService(IUnit* pCtrl, ECharacteristicFlags availableSensor) noexcept
  : CSensorService
  (
    availableSensor,
    {
      .UserPtr = reinterpret_cast<void*>(pCtrl),

      .TemperatureGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
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
      .HumidityGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
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
      }

    })
  {
    uint32_t AS = static_cast<uint32_t>(availableSensor);

    if(!(AS & static_cast<uint32_t>(ECharacteristicFlags::Temperature)))
      Temperature.getter_ex = nullptr;
    if(!(AS & static_cast<uint32_t>(ECharacteristicFlags::Humidity)))
      Humidity.getter_ex = nullptr;
  }

  CSensorService(IUnit* pCtrl, EEventType event, ECharacteristicFlags flags = ECharacteristicFlags::Event) noexcept
    : CSensorService
    (
      flags,
      {
        .UserPtr = reinterpret_cast<void*>(pCtrl),
        .Event = event,

      })
  {
  }



  #pragma endregion

  #pragma region Properties

  constexpr const bool TemperatureDefined() const noexcept
  {
    return Temperature.getter_ex != nullptr;
  }
  constexpr const bool HumidityDefined() const noexcept
  {
    return Humidity.getter_ex != nullptr;
  }
  constexpr const bool EventDefined() const noexcept
  {
    return Flags & ECharacteristicFlags::Event;
  }

  #pragma region GetUnit
  IUnit* GetUnit() const noexcept
  {
    return static_cast<IUnit*>(Temperature.UserPtr);
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

  #pragma region SetEventValue
  void SetEventValue(homekit_value_t value) override
  {
    /* Prevent the implicit call of Unit->OnTargetStateChanged() by setting UserPtr to null.
     * This is important because a call to OnTriggered() by OnTargetStateChanged()
     * will cause OnTargetStateChanged() to be called again.
    */
    {
      void* UserPtr = std::exchange(Event.UserPtr, nullptr);
      modify_value_and_notify(&Event, value);
      Event.UserPtr = UserPtr;
    }
  }
  #pragma endregion



  //END ISystem-Methods
  #pragma endregion

  #pragma region Methods

  #pragma region Setup
  /* Setup the sensor.
  * @note This method must be called in the setup() function.
  */
  void Setup()
  {
    auto Unit = GetUnit();
    if(Unit)
    {
      if(!CHomeKit::Singleton)
      {
        ERROR("CSensorService::Setup: CHomeKit not yet created");
        return;
      }

      Unit->Setup
      (
        {
          .Ctrl = &CHomeKit::Singleton->Controller,
          .Super = Unit,
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

  #pragma region GetService
  /* Get the service at the specified index
  * @param idx The index of the service
  * @return The service or nullptr (if idx is out of range or the service is not defined)
  */
  constexpr const homekit_service_t* GetService(int idx) const noexcept
  {
    std::array Services
    {
      TemperatureDefined()  ? &TemperatureService : nullptr,
      HumidityDefined()     ? &HumidityService : nullptr,
      EventDefined()        ? &EventService : nullptr
    };

    for(int i = 0; i < Services.size(); ++i)
    {
      if(Services[i] != nullptr)
      {
        if(idx == 0)
          return Services[i];

        --idx;
      }
    }

    return nullptr;
  }

  #pragma endregion

  //END Methods
  #pragma endregion

  #pragma region Operators
  constexpr const homekit_service_t* operator [] (int idx) const noexcept
  {
    return GetService(idx);
  }

  #pragma endregion

};

#pragma endregion


#pragma region +Functions

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
      1000, // [ms] Determine new sensor values every second
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

#pragma region MakeContinuousReadEventUnit
IUnit_Ptr MakeContinuousReadEventUnit(uint32_t interval, std::function<CEventInfo(void)> func);

#pragma endregion

#pragma region MakeOnSensorChangedUnit
/*
* @brief
* @example
    MakeOnSensorChangedUnit([](const CSensorInfo& info)
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
IUnit_Ptr MakeOnSensorChangedUnit(std::function<void(const CSensorInfo&)> func);

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
void InstallVarsAndCmds(CController& c, CHost& host);


/*
* @brief Installs and configures everything for CSensorService.
* @note: This function must be invoked in setup().
*/
inline void InstallAndSetupSensor(CController& ctrl, CSensorService& svr, CHost& host)
{
  svr.Setup();
  InstallVarsAndCmds(ctrl, host);
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

#pragma endregion



//END +Functions
#pragma endregion


#pragma region Epilog
} // namespace Sensor
} // namespace HBHomeKit
#pragma endregion
