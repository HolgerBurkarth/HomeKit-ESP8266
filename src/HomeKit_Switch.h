#pragma region Prolog
/*******************************************************************
$CRT 27 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Switch.h<< 09 Dez 2024  11:31:02 - (c) proDAD

using namespace HBHomeKit::Switch;
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
namespace Switch
{
#pragma endregion

#pragma region +Support

#pragma region CPrgSwitchEventController
struct CPrgSwitchEventController
{
  #pragma region static const
  constexpr static uint32_t ToShortMS     =    1; // [ms]
  constexpr static uint32_t DoubleClickMS =  500; // [ms]
  constexpr static uint32_t LongPressMS   = 3000; // [ms]
  constexpr static uint32_t MaxEventCount = 4;

  #pragma endregion

  #pragma region Fields
  std::array<uint32_t, MaxEventCount> mEventTime{};
  uint16_t mEventIndex{};
  uint8_t  mNeedClear{};

  #pragma endregion

  #pragma region NextEvent

  std::optional<HOMEKIT_PROGRAMMABLE_SWITCH_EVENT> NextEvent()
  {
    /*
      Press     Release   Press    Release   Press   Release
        |---------|---------|---------|--------|--------|
      Single:#                             #
      Double:#         #         #         #
      Long:  #                                          #....
    */
    if(mNeedClear == 0)
    {
      HOMEKIT_PROGRAMMABLE_SWITCH_EVENT Evt = HOMEKIT_PROGRAMMABLE_SWITCH_EVENT_UNDEF;
      uint32_t CurMS = millis();
      switch(mEventIndex)
      {
        case 1: // Pressed an not released
          if(CurMS - mEventTime[0] >= LongPressMS)
            Evt = HOMEKIT_PROGRAMMABLE_SWITCH_EVENT_LONG_PRESS;
          break;

        case 2: // Pressed and released
          if(CurMS - mEventTime[0] > DoubleClickMS)
            Evt = HOMEKIT_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS;
          break;

        case 4: // Pressed, released, pressed and released
          if(mEventTime[3] - mEventTime[0] > DoubleClickMS)
            Evt = HOMEKIT_PROGRAMMABLE_SWITCH_EVENT_SINGLE_PRESS;
          else
            Evt = HOMEKIT_PROGRAMMABLE_SWITCH_EVENT_DOUBLE_PRESS;
          break;
      }

      if(Evt != HOMEKIT_PROGRAMMABLE_SWITCH_EVENT_UNDEF)
      {
        mNeedClear = 1;
        return Evt;
      }
    }
    return {};
  }

  #pragma endregion

  #pragma region Trigger
  void Trigger(bool pressed)
  {
    uint32_t CurMS = millis();

L_Enter:
    if(mNeedClear)
    {
      mEventIndex = 0;

      if(pressed)
      {
        mNeedClear = 0; // force pressed always on index=0
        mEventTime[0] = CurMS;
        mEventIndex = 1;
      }
    }
    else if(mEventIndex < MaxEventCount)
    {
      if(ToShortMS > 0 && CurMS - mEventTime[mEventIndex - 1] < ToShortMS)
      {
        mNeedClear = 1;
        goto L_Enter;
      }
      else
      {
        static_assert((MaxEventCount & 1) == 0); // must be even
        if(pressed)
          mEventIndex &= ~1; // even index for pressed
        else
          mEventIndex |= 1; // odd index for released


        mEventTime[mEventIndex] = CurMS;
        ++mEventIndex;
      }

    }
  }

  #pragma endregion

};

#pragma endregion

#pragma region ForEach_Switch
struct CSwitchDsc
{
  const homekit_service_t*  pService{};
  homekit_characteristic_t* pSwitch{};
  const char*               Name{};

  enum EType : uint8_t
  {
    Type_Switch = 1,
    Type_StatelessProgrammableSwitch = 2

  } Type{};

};

/* Calls the functor for each switch service 
* (homekit_accessory_category_switch, homekit_accessory_category_programmable_switch)
* @param functor The functor to call: void functor(const homekit_service_t*)
*/
void ForEach_Switch(std::function<void(const homekit_service_t*)> functor);

/* Calls the functor for each switch service
* (homekit_accessory_category_switch, homekit_accessory_category_programmable_switch)
* @param functor The functor to call: void functor(CSwitchDsc)
* @example
* ForEach_Switch([](CSwitchDsc Dsc)
*   {
*     INFO("Switch: %s", Dsc.Name);
*   });
*/
void ForEach_Switch(std::function<void(CSwitchDsc)> functor);

#pragma endregion





//END Support
#pragma endregion

#pragma region CSwitchService
/* A HomeKit switch service
*  Allows to switch a device on or off
* @example
*
CSwitchService Switch
({
  .Name = "SwitchParam",
  //.Default = false,
  .Setter = [](const homekit_value_t value)
  {
    bool Stat = static_value_cast<bool>(value);
    digitalWrite(LED_PIN, Stat ? HIGH : LOW);
  },
  .Getter = []() -> homekit_value_t
  {
    return static_value_cast<bool>(digitalRead(LED_PIN) == HIGH);
  }
});

*/
struct CSwitchService
{
  #pragma region Fields
  homekit_characteristic_t State;
  homekit_characteristic_t Name;

  CService<2> Service;

  #pragma endregion

  #pragma region Params_t
  struct Params_t
  {
    /* The name of the switch
    */
    const char* Name{ "Switch" };

    /* The default value of the switch
    */
    bool Default{};

    /* Optional setter function:
    *   void setter(homekit_characteristic_t*, homekit_value_t value);
    *
    * Called when the switch is switched on or off.
    * The User can use this function to switch the device on or off.
    * @param value The new value of the switch
    * @example
        .Setter = [](homekit_characteristic_t* pC, homekit_value_t value)
        {
          if(modify_value(pC, value))
          {
            bool Stat = static_value_cast<bool>(value);
            digitalWrite(LED_PIN, Stat ? HIGH : LOW);
          }
        },
    *
    */
    void(*Setter)(homekit_characteristic_t*, homekit_value_t) {};

    /* Optional getter function:
    *   homekit_value_t getter(const homekit_characteristic_t*);
    *
    * Called when the value of the switch is requested.
    *   The User should return the current value of the switch.
    * @return The current value of the switch
    * @example
        .Getter = [](const homekit_characteristic_t* pC) -> homekit_value_t
        {
          return static_value_cast<bool>(digitalRead(LED_PIN) == HIGH);
        }
    */
    homekit_value_t(*Getter)(const homekit_characteristic_t*) {};
  };

  #pragma endregion

  #pragma region Construction
  CSwitchService(const CSwitchService&) = delete;
  CSwitchService(CSwitchService&&) = delete;

  constexpr CSwitchService(Params_t params) noexcept
    :
    State{ HOMEKIT_DECLARE_CHARACTERISTIC_ON(params.Name, params.Default, .getter_ex = params.Getter, .setter_ex = params.Setter) },
    Name{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.Name) },
    Service
    {
      {
        .type = HOMEKIT_SERVICE_SWITCH,
        .primary = true
      },

      &State,
      &Name
    }
  {
  }

  #pragma endregion

  #pragma region Properties
  const char* GetName() const noexcept
  {
    return static_value_cast<const char*>(&Name);
  }


  #pragma endregion

  #pragma region Operators
  constexpr const homekit_service_t* operator&() const noexcept
  {
    return &Service;
  }

  #pragma endregion

};

#pragma endregion

#pragma region CStatelessSwitchService
/* A HomeKit switch service
* @example
*
CStatelessSwitchService Switch
({
  .Name = "SwitchParam",
});

*/
struct CStatelessSwitchService
{
  #pragma region Fields
  homekit_characteristic_t Event;
  homekit_characteristic_t Name;

  CService<2> Service;

  #pragma endregion

  #pragma region Params_t
  struct Params_t
  {
    /* The name of the switch
    */
    const char* Name{ "StatelessSwitch" };
  };

  #pragma endregion

  #pragma region Construction
  CStatelessSwitchService(const CStatelessSwitchService&) = delete;
  CStatelessSwitchService(CStatelessSwitchService&&) = delete;

  constexpr CStatelessSwitchService(Params_t params) noexcept
    :
    Event{ HOMEKIT_DECLARE_CHARACTERISTIC_PROGRAMMABLE_SWITCH_EVENT(params.Name, 0, .getter_ex = NullGetter) },
    Name{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.Name) },
    Service
    {
      {
        .type = HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH,
        .primary = true
      },

      &Event,
      &Name
    }
  {
  }

  #pragma endregion

  #pragma region Properties
  const char* GetName() const noexcept
  {
    return static_value_cast<const char*>(&Name);
  }

  void SetEvent(HOMEKIT_PROGRAMMABLE_SWITCH_EVENT value) noexcept
  {
    Event.value = HOMEKIT_NULL_CPP(); // clear the value and force a notification
    modify_value_and_notify(&Event, static_value_cast(value));
  }


  #pragma endregion

  #pragma region Operators
  constexpr const homekit_service_t* operator&() const noexcept
  {
    return &Service;
  }

  #pragma endregion

  #pragma region NullGetter
  static homekit_value_t NullGetter(const homekit_characteristic_t*)
  {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  }

  #pragma endregion


};

#pragma endregion

#pragma region

/*
*/
void InstallVarsAndCmds(CController& c);

/*
*/
CTextEmitter Switch_JavaScript();

#pragma endregion


#pragma region Epilog
} // namespace Switch
} // namespace HBHomeKit
#pragma endregion
