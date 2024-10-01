#pragma region Prolog
/*******************************************************************
$CRT 27 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Switch.h<< 27 Sep 2024  17:17:13 - (c) proDAD

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



#pragma region Epilog
} // namespace Switch
} // namespace HBHomeKit
#pragma endregion
