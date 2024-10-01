#pragma region Prolog
/*******************************************************************
$CRT 18 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_log_menu.cpp<< 30 Sep 2024  14:43:59 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling: logtab
#pragma endregion
#pragma region Includes
#include <Arduino.h>
#include "hb_homekit.h"

namespace HBHomeKit
{
namespace
{
#pragma endregion

#pragma region Log_CCS
CTextEmitter Log_CCS()
{
  return MakeTextEmitter(F(R"(
#logtab td, #logtab th {
  /*border: 1px solid #444;*/
  padding: 0.2em;
}
#logtab th {
  padding-top: 0.5em;
  padding-bottom: 0.5em;
  text-align: center;
  background-color: #888;
  color: black;
}
#logtab .warn {
  background-color: #640;
}
#logtab .error {
  background-color: #a22;
}
#logtab tr:nth-child(even){background-color: #222;}
)"));
}
#pragma endregion

#pragma region Log_JavaScript
CTextEmitter Log_JavaScript()
{
  return MakeTextEmitter(F(R"(

function AddLogEntry(message)
{
  var table = document.getElementById("logtab");
  /* Split message by '\t' and insert cells */
  var cells = message.split('\t');

  if(cells.length > 1)
  {
    var row = table.insertRow(-1);
    if(cells[0].startsWith("Warn"))
      row.className = "warn";
    else if(cells[0].startsWith("Error"))
      row.className = "error";


    for(var i = 0; i < cells.length; ++i)
    {
      var cell = row.insertCell(i);
      cell.className = row.className;
      cell.innerHTML = cells[i];
    }
  }
}

var LastID = 0;
function Update()
{
  ForSetVar('LOG_MESSAGE', LastID, function(text)
    {
      if(text.length > 0)
      {
        AddLogEntry(text);
        LastID++;
        Update();
      }
    });
}



onload = function()
{
  Update();

  setInterval
  (
    function() { Update(); },
    1000
  );
};


)"));
}
#pragma endregion

#pragma region Log_Html
CTextEmitter Log_Html()
{
  return MakeTextEmitter(F(R"(
  <table id='logtab'>
  <tr><th>Lev.</th><th>UTC Date Time</th><th>Message</th></tr>

  </table>
)"));
}
#pragma endregion



#pragma region AddLoggingMenu
} // namespace


CController& AddLoggingMenu(CController& c)
{
  c

    #pragma region Variables
    #pragma region LOG_MESSAGE
    .SetVar("LOG_MESSAGE", [&](auto p)
      {
        String Result;
        if(gbLogging && p.Args[0] != nullptr)
        {
          int ID = convert_value<int>(*p.Args[0]);
          Result = gbLogging->GetLine(ID);
        }
        return MakeTextEmitter(Result);
      })

    #pragma endregion

    //END Variables
    #pragma endregion

    #pragma region Menu
    .AddMenuItem
    (
      {
        .Title = "Device Logging",
        .MenuName = "Log",
        .URI = "/log",
        .LowMemoryUsage = true,
        .SpecialMenu = true,
        .CSS = Log_CCS(),
        .JavaScript = [](Stream& out)
          {
            out << ActionUI_JavaScript();
            out << Log_JavaScript();
          },
        .Body = Log_Html()
      }
    )
    //END Menus
    #pragma endregion

    ;

  return c;
}

#pragma endregion


#pragma region Epilog
} // namespace HBHomeKit
#pragma endregion
