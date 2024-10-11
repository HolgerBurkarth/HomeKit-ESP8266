#pragma region Prolog
/*******************************************************************
$CRT 18 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_action_ui.cpp<< 11 Okt 2024  09:43:46 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Includes
#include <Arduino.h>
#include "hb_homekit.h"

namespace HBHomeKit
{
namespace
{
#pragma endregion


#pragma region InstallActionUI
} // namespace


CController& InstallActionUI(CController& c)
{
  c

    #pragma region Variables
    #pragma region ACTION_BUTTON
    .SetVar("ACTION_BUTTON", [](auto p)
      {
        String Result;
        if(p.Args[0] == nullptr || !is_value_typeof<const char*>(*p.Args[0]))
        {
          Result = F("ACTION_BUTTON: argument is missing");
          ERROR("ACTION_BUTTON: argument is missing");
        }
        else
        {
          auto Args = Split(static_value_cast<const char*>(*p.Args[0]), ':');
          if(Args.size() < 2 || Args.size() > 3)
          {
            Result = F("ACTION_BUTTON: invalid args: [id:]action:text");
            ERROR("ACTION_BUTTON: invalid args: [id:]action:text");
          }
          else
          {
            int i = 0;
            if(Args.size() == 2)
            {
              // e.g.: <button onclick = "SetSwitchState(1)">Switch On</button>
              Result += (F("<button"));
            }
            else
            {
              // e.g.: <button id='id' onclick = "SetSwitchState(1)">Switch On</button>
              Result += (F("<button id='"));
              Result += Args[i++];
              Result += (F("'"));
            }

            Result += (F(" onclick = \""));
            Result += Args[i++];
            Result += (F("\">"));
            Result += Args[i++];
            Result += (F("</button>"));
          }
        }
        return MakeTextEmitter(Result);
      })

    #pragma endregion
    #pragma region ACTION_CHECKBOX
    .SetVar("ACTION_CHECKBOX", [](auto p)
      {
        String Result;
        if(p.Args[0] == nullptr || !is_value_typeof<const char*>(*p.Args[0]))
        {
          Result = F("ACTION_CHECKBOX: argument is missing");
          ERROR("ACTION_CHECKBOX: argument is missing");
        }
        else
        {
          auto Args = Split(static_value_cast<const char*>(*p.Args[0]), ':');
          if(Args.size() != 2)
          {
            Result = F("ACTION_CHECKBOX: invalid args: id:action");
            ERROR("ACTION_CHECKBOX: invalid args: id:action");
          }
          else
          {
            /* e.g:
            * <label class="checkbox">
            * <input id='switch' type="checkbox" onclick="SetSwitch(this.checked);" />
            * <span class="slider round"></span>
            * </label>
            */
            Result += (F("<label class='checkbox'><input id='"));
            Result += Args[0];
            Result += (F("' type='checkbox' onclick='"));
            Result += Args[1];
            Result += (F("'/><span class='slider round'></span></label>"));
          }
        }
        return MakeTextEmitter(Result);
      })

    #pragma endregion

    //END Variables
    #pragma endregion

  ;
  return c;
}

#pragma endregion

#pragma region ActionUI_JavaScript
CTextEmitter ActionUI_JavaScript()
{
  return MakeTextEmitter(F(R"(
function InvokeAction(varLine, functor)
{
  fetch('/var?'+ varLine)
    .then(response =>
      {
        if(!response.ok)
        {
          throw new Error('Network response was not OK ' + response.statusText);
        }
        return response.text();
      })
    .then(text =>
      {
        functor(text);
      })
    .catch(error =>
      {
        console.error('Error:', error);
        functor('Error: ' + error.message);
      })
  ;
}

function SetVar(varName, varValue)
{
  InvokeAction(varName + '=' + varValue, function(text)
    {
    });
}

function ForVar(varName, functor)
{
  InvokeAction(varName, functor);
}

function ForSetVar(varName, varValue, functor)
{
  InvokeAction(varName + '=' + varValue, functor);
}


)"));
}
#pragma endregion

#pragma region ActionUI_CSS
CTextEmitter ActionUI_CSS()
{
  return MakeTextEmitter(F(R"(
.checkbox {
  position: relative;
  display: inline-block;
  vertical-align: middle;
  width: 5em;
  height: 2em;
}

.checkbox input {
  opacity: 0;
  width: 0;
  height: 0;
}

.slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: #aaa;
  -webkit-transition: .4s;
  transition: .4s;
}

.slider:before {
  position: absolute;
  content: "";
  height: 1.8em;
  width: 1.8em;
  left: 0.1em;
  bottom: 0.1em;
  background-color: white;
  -webkit-transition: .4s;
  transition: .4s;
}

input:checked + .slider {
  background-color: #ea0;
}

input:focus + .slider {
  box-shadow: 0 0 1px #ea0;
}

input:checked + .slider:before {
  -webkit-transform: translateX(3em);
  -ms-transform: translateX(3em);
  transform: translateX(3em);
}

/* Rounded sliders */
.slider.round {
  border-radius: 2em;
}

.slider.round:before {
  border-radius: 50%;
}

/* Input as Text with larger font */
input[type="text"] {
  font-size: 1.2em;
  padding: 0.2em;
  margin: 0.2em;
}
)"));
}

#pragma endregion

#pragma region UIUtils_JavaScript
/* @info: JavaScript functions for manipulating the UI
*  The following functions are available:
*  param idOrHd : id or HTMLElement: e.g: 'myId' or document.getElementById('myId')
*
*  HideElement(idOrHd, hide)
*  DisableElement(idOrHd, disable)
*  SetElementInnerHTML(idOrHd, text)
*  LogDebug(txt)
*/
CTextEmitter UIUtils_JavaScript()
{
  return MakeTextEmitter(F(R"(

const Unit_cm = '<span class="unit">cm</span>';
const Unit_s = '<span class="unit">s</span>';

function HideElement(idOrHd, hide)
{
  var element = (typeof idOrHd === "string") ? document.getElementById(idOrHd) : idOrHd;
  if(typeof hide === "string") hide = hide.toLowerCase() === "true";
  if(element) element.style.display = hide ? "none" : "block";
}
function VisibleElement(idOrHd, visible)
{
  var element = (typeof idOrHd === "string") ? document.getElementById(idOrHd) : idOrHd;
  if(typeof visible === "string") visible = visible.toLowerCase() === "true";
  if(element) element.style.display = visible ? "block" : "none";
}
function DisableElement(idOrHd, disable)
{
  var element = (typeof idOrHd === "string") ? document.getElementById(idOrHd) : idOrHd;
  if(typeof disable === "string") disable = disable.toLowerCase() === "true";
  if(element) element.disabled = disable;
}
function SetElementInnerHTML(idOrHd, text)
{
  var element = (typeof idOrHd === "string") ? document.getElementById(idOrHd) : idOrHd;
  if(element) element.innerHTML = text;
}
function SetElementChecked(idOrHd, value)
{
  var element = (typeof idOrHd === "string") ? document.getElementById(idOrHd) : idOrHd;
  if(typeof value === "string") value = value.toLowerCase() === "true";
  if(element) element.checked = value;
}
function LogDebug(txt)
{
  console.log(txt);
  SetElementInnerHTML('Debug', text);
}

)"));
}

#pragma endregion


#pragma region Epilog
} // namespace HBHomeKit
#pragma endregion
