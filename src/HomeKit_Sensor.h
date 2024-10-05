#pragma region Prolog
/*******************************************************************
$CRT 05 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Sensor.h<< 05 Okt 2024  11:31:41 - (c) proDAD

using namespace HBHomeKit::Sensor;
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Includes
#pragma once

#include <hb_homekit.h>

namespace HBHomeKit
{
namespace Sensor
{
#pragma endregion

#pragma region CSensorInfo
struct CSensorInfo
{
  std::optional<float> Temperature;
  std::optional<float> Humidity;
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

  struct CSetupArgs
  {
    CController* Ctrl{};
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

  void Setup(const CSetupArgs& args) override
  {
    Ctrl = args.Ctrl;
    Super = args.Super;
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
struct CSensorService
{
  #pragma region Fields
  homekit_characteristic_t TemperatureName;
  homekit_characteristic_t HumidityName;
  homekit_characteristic_t Temperature;
  homekit_characteristic_t Humidity;

  CService<2> TemperatureService;
  CService<2> HumidityService;

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

    /* Free to use by user
    */
    void* UserPtr{};

    /* Optional */
    homekit_value_t(*TemperatureGetter)(const homekit_characteristic_t*) {};
    /* Optional */
    homekit_value_t(*HumidityGetter)(const homekit_characteristic_t*) {};
  };

  #pragma endregion

  #pragma region Construction
  CSensorService(const CSensorService&) = delete;
  CSensorService(CSensorService&&) = delete;

  constexpr CSensorService(Params_t params) noexcept
    : TemperatureName{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.TemperatureName) }
    , HumidityName{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.HumidityName) }
    , Temperature{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_TEMPERATURE(params.TemperatureName, 0, .getter_ex = params.TemperatureGetter, .UserPtr = params.UserPtr) }
    , Humidity{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_RELATIVE_HUMIDITY(params.HumidityName, 0, .getter_ex = params.HumidityGetter, .UserPtr = params.UserPtr) }
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
  {
  }


  CSensorService(IUnit* pCtrl) noexcept
    : CSensorService
    ({
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

  #pragma region GetUnit
  IUnit* GetUnit() const noexcept
  {
    return static_cast<IUnit*>(Temperature.UserPtr);
  }
  #pragma endregion


  //END Properties
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
          .Ctrl = &CHomeKit::Singleton->Controller
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
      TemperatureDefined() ? &TemperatureService : nullptr,
      HumidityDefined() ? &HumidityService : nullptr
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
/*
* @brief
*/
IUnit_Ptr MakeControlUnit();

/*
* @brief
*/
IUnit_Ptr MakeContinuousReadSensorUnit(uint32_t interval, std::function<CSensorInfo(void)> func);

/*
* @brief
*/
IUnit_Ptr MakeOnSensorChangedUnit(std::function<void(const CSensorInfo&)> func);

#pragma endregion

#pragma region Install - Functions

void InstallVarsAndCmds(CController& c, CHost& host);

void AddMenuItems(CController& c);

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
