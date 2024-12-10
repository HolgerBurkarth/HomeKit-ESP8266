#pragma region Prolog
/*******************************************************************
$CRT 08 Dez 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Switch.cpp<< 10 Dez 2024  16:29:12 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Includes
#include <HomeKit_Switch.h>

namespace HBHomeKit
{
namespace Switch
{
#pragma endregion

#pragma region Support

#pragma region ForEach_Switch

void ForEach_Switch(std::function<void(const homekit_service_t*)> functor)
{
  ForEachAccessory(GetHomeKitConfig(), [&](const homekit_accessory_t* pAccessory)
    {
      if(  pAccessory->category == homekit_accessory_category_switch
        || pAccessory->category == homekit_accessory_category_programmable_switch)
      {
        ForEachService(pAccessory, functor);
      }
    });
}


void ForEach_Switch(std::function<void(CSwitchDsc)> functor)
{
  ForEach_Switch([&](const homekit_service_t* pService)
    {
      CSwitchDsc Dsc{ pService };

      if(strcmp(pService->type, HOMEKIT_SERVICE_SWITCH) == 0)
        Dsc.Type = CSwitchDsc::Type_Switch;
      else if(strcmp(pService->type, HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH) == 0)
        Dsc.Type = CSwitchDsc::Type_StatelessProgrammableSwitch;
      else
        return;

      ForEachCharacteristic(pService, [&](const homekit_characteristic_t* pCh)
        {
          if(strcmp(pCh->type, HOMEKIT_CHARACTERISTIC_NAME) == 0)
            Dsc.Name = static_value_cast<const char*>(pCh);
          else if(strcmp(pCh->type, HOMEKIT_CHARACTERISTIC_ON) == 0
               || strcmp(pCh->type, HOMEKIT_CHARACTERISTIC_PROGRAMMABLE_SWITCH_EVENT) == 0)
          {
            Dsc.pSwitch = const_cast<homekit_characteristic_t*>(pCh);
          }
        });


      if(Dsc.Name && Dsc.pSwitch)
        functor(Dsc);

    });
}

#pragma endregion


//END Support
#pragma endregion

#pragma region +Website - Elements

#pragma region Switch_JavaScript
CTextEmitter Switch_JavaScript()
{
  return MakeTextEmitter(F(R"(

// vector of all switches
var SwitchNames = [];
var SwitchTypes = [];

function UIUpdateSwitch(name, state)
{
  var State = (state == 'true');
  SetElementChecked(name+'Checkbox', State);
}

function SetSwitch(name, state)
{
  ForSetVar('SWITCH_STATE', name + '|' + state, responseText => UIUpdateSwitch(responseText) );
}
function SetStatelessSwitch(name, state)
{
  ForSetVar('SWITCH_STATE', name + '|' + state, responseText => {} );
}

function Update()
{
  for(var i = 0; i < SwitchNames.length; ++i)
  {
    var Name = SwitchNames[i];
    var Type = SwitchTypes[i];
    switch(Type)
    {
      case 1:
        ForSetVar('SWITCH_STATE', Name, responseText => UIUpdateSwitch(Name, responseText) );
        break;
    }
  }
}

function BuildSwitches()
{
  ForVar('SWITCHES', responseText => 
    {
      var Switches = responseText.split('|');
      for(var i = 0; i < Switches.length; ++i)
      {
        var Switch = Switches[i].split(';');
        if(Switch.length == 2)
        {
          var Name = Switch[0];
          var Type = parseInt(Switch[1]);

          SwitchNames.push(Name);
          SwitchTypes.push(Type);
        }
      }
    });
}



window.addEventListener('load', (event) =>
{
  setTimeout(BuildSwitches, 100);

  setInterval
  ( 
    function() { Update(); },
    1000
  );
});

)"));
}
#pragma endregion


//END Website - Elements
#pragma endregion

#pragma region +Install and Setup

#pragma region InstallVarsAndCmds
void InstallVarsAndCmds(CController& c)
{
  c
    #pragma region SWITCH_STATE
    .SetVar("SWITCH_STATE", [](auto p)
      {
        if(p.Args[0] != nullptr) // case of: /?SWITCH_STATE=name|true
        {
          if(!is_value_typeof<const char*>(*p.Args[0]))
          {
            ERROR("SWITCH_STATE: Invalid argument");
          }
          else
          {
            CSwitchDsc Dsc{};
            auto Params = Split(static_value_cast<const char*>(*p.Args[0]), '|');
            if(Params.size() < 1)
            {
              ERROR("SWITCH_STATE: Invalid argument");
            }
            else
            {
              ForEach_Switch([&](CSwitchDsc dsc)
                {
                  if(strcmp(dsc.Name, Params[0].c_str()) == 0)
                    Dsc = dsc;
                });
            }


            if(!Dsc.pSwitch)
            {
              ERROR("SWITCH_STATE \"%s\": Switch not found", Params[0].c_str());
            }
            else
            {
              if(Params.size() == 2)
              {
                homekit_value_t Value{};
                Value = static_value_cast<const char*>(Params[1].c_str());

                VERBOSE("SWITCH_STATE Set %s = %s (Type:%d)", Dsc.Name, Params[1].c_str(), Dsc.Type);

                switch(Dsc.Type)
                {
                  case CSwitchDsc::Type_Switch:
                    Value = homekit_value_cast<bool>(Value);
                    modify_value_and_notify(Dsc.pSwitch, Value);
                    break;

                  case CSwitchDsc::Type_StatelessProgrammableSwitch:
                    Value = homekit_value_cast<uint8_t>(Value);
                    Dsc.pSwitch->value = HOMEKIT_NULL_CPP(); // clear the value and force a notification
                    modify_value_and_notify(Dsc.pSwitch, Value);
                    break;
                }
              }


              switch(Dsc.Type)
              {
                case CSwitchDsc::Type_Switch:
                  //VERBOSE("SWITCH_STATE %s: %d", Dsc.Name, static_value_cast<bool>(Dsc.pSwitch));
                  return MakeTextEmitter(convert_value<bool>(Dsc.pSwitch));
                  break;

                case CSwitchDsc::Type_StatelessProgrammableSwitch:
                  //VERBOSE("SWITCH_STATE %s: %d", Dsc.Name);
                  return MakeTextEmitter();
                  break;
              }
            }
          }

        }
        return MakeTextEmitter();
      })

    #pragma endregion

    #pragma region SWITCHES
    /*
    * @return a list of all switches: "name;type|name;type|..."
    */
    .SetVar("SWITCHES", [](auto p)
      {
        String Text;
        ForEach_Switch([&](CSwitchDsc dsc)
          {
            char Buf[64];
            sprintf_P(Buf, PSTR("%.32s;%d|")
              , dsc.Name
              , dsc.Type
            );
            Text += Buf;
          });

        VERBOSE("SWITCHES: %s", Text.c_str());
        return MakeTextEmitter(Text);
      })

    #pragma endregion

  ;
}

#pragma endregion


//END Install and Setup
#pragma endregion



#pragma region Epilog
} // namespace Switch
} // namespace HBHomeKit
#pragma endregion
