#pragma region Prolog
/*******************************************************************
$CRT 05 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Sensor.cpp<< 05 Okt 2024  11:32:10 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Includes
#include <HomeKit_Sensor.h>

namespace HBHomeKit
{
namespace Sensor
{
#pragma endregion

#pragma region +Unit - Objects

#pragma region CControlUnit - Implementation
class CControlUnit : public CUnitBase
{
  #pragma region Fields
  CSensorInfo mSensorInfo{};

public:
  #pragma endregion

  #pragma region Override Methods

  #pragma region ReadSensorInfo
  void ReadSensorInfo(CSensorInfoArgs& args) override
  {
    args = mSensorInfo;
    args.Handled = true;
  }
  #pragma endregion

  #pragma region WriteSensorInfo
  void WriteSensorInfo(CSensorInfoArgs& args) override
  {
    args.Handled = true;
    if(args.Value.Temperature)
    {
      mSensorInfo.Temperature = *args.Value.Temperature;
      //Serial.printf("Temperature: %0.2f\n", *args.Value.Temperature);
    }
    if(args.Value.Humidity)
    {
      mSensorInfo.Humidity = *args.Value.Humidity;
      //Serial.printf("Humidity: %0.2f\n", *args.Value.Humidity);
    }

  }

  #pragma endregion


  //END Override Methods
  #pragma endregion
};

IUnit_Ptr MakeControlUnit()
{
  return std::make_shared<CControlUnit>();
}

//END CControlUnit
#pragma endregion

#pragma region CContinuousReadSensorUnit - Implementation
class CContinuousReadSensorUnit : public CUnitBase
{
  using GetterFunc = std::function<CSensorInfo(void)>;

  #pragma region Fields
  GetterFunc mGetter;
  uint32_t mInterval{};

  #pragma endregion

  #pragma region Construction
public:
  CContinuousReadSensorUnit(uint32_t interval, GetterFunc&& func)
    : mGetter(std::move(func)), mInterval(interval)
  {}

  #pragma endregion

  #pragma region Override Methods

  #pragma region Start

  void Start(CVoidArgs&) override
  {
    Ctrl->Ticker.Start(mInterval, [this]()
      {
        CSensorInfoArgs Args;
        Args.Value = mGetter();
        Super->WriteSensorInfo(Args);
      });
  }

  #pragma endregion

  //END Override Methods
  #pragma endregion
};

IUnit_Ptr MakeContinuousReadSensorUnit(uint32_t interval, std::function<CSensorInfo(void)> func)
{
  return std::make_shared<CContinuousReadSensorUnit>(interval, std::move(func));
}

//END CContinuousReadSensorUnit
#pragma endregion

#pragma region COnSensorChangedUnit - Implementation
class COnSensorChangedUnit : public CUnitBase
{
  using NotifyFunc = std::function<void(const CSensorInfo&)>;

  #pragma region Fields
  NotifyFunc mNotify;

  #pragma endregion

  #pragma region Construction
public:
  COnSensorChangedUnit(NotifyFunc&& func)
    : mNotify(std::move(func))
  {}

  #pragma endregion

  #pragma region Override Methods

  #pragma region WriteSensorInfo

  void WriteSensorInfo(CSensorInfoArgs& args) override
  {
    mNotify(args.Value);
  }

  #pragma endregion

  //END Override Methods
  #pragma endregion
};

IUnit_Ptr MakeOnSensorChangedUnit(std::function<void(const CSensorInfo&)> func)
{
  return std::make_shared<COnSensorChangedUnit>(std::move(func));
}

//END COnSensorChangedUnit
#pragma endregion


//END Unit - Objects
#pragma endregion

#pragma region +Website - Elements

#pragma region EventChart_JavaScript
CTextEmitter EventChart_JavaScript()
{
  return MakeTextEmitter(F(R"(

var EventChartCanvasHD;
var EventChart;
var Temperates = []; // array of floats
var Humidities = []; // array of floats
const MaxEntries = 180;


function MakeEventChart()
{
  EventChartCanvasHD = document.getElementById('Canvas');
  EventChart = new CChart(EventChartCanvasHD);
  EventChart.AutoMargins('', 'numbers left right');
}

function ResizeEventChart()
{
  if(EventChartCanvasHD)
    EventChartCanvasHD.width = window.innerWidth;
}


/*
*/
function UpdateEventChart()
{
  var MinTemp = 0;
  var MaxTemp = 0;
  var MinHum  = 0;
  var MaxHum  = 0;

  if(Temperates.length > 1)
  {
    MinTemp = Math.min(...Temperates);
    MaxTemp = Math.max(...Temperates);
  }
  if(Humidities.length > 1)
  {
    MinHum = Math.min(...Humidities);
    MaxHum = Math.max(...Humidities);
  }


  MinTemp= MinTemp-0;
  MaxTemp= MaxTemp+1;
  MinTemp = Math.floor(MinTemp / 1) * 1;
  MaxTemp = Math.ceil(MaxTemp  / 1) * 1;

  MinHum= MinHum-4;
  MaxHum= MaxHum+0;
  MinHum = Math.floor(MinHum / 4) * 4;
  MaxHum = Math.ceil(MaxHum  / 4) * 4;

  EventChart.Clear();
  EventChart.ResetGridColor();
  EventChart.ResetAxisColors();

  EventChart.SetHrzAxis(0,  MaxEntries,   10);
  EventChart.DrawHrzGrid();
  EventChart.DrawHrzAxisLabel('Time', -1);

  if(Temperates.length > 1)
  {
    EventChart.SetAxisColors('#e4e', '#f4f');
    if(MaxTemp-MinTemp < 3)
    {
      EventChart.SetGridColor('#606');
      EventChart.SetVrtAxis(MinTemp, MaxTemp, 0.25);
      EventChart.DrawVrtGrid();
    }

    EventChart.SetGridColor('#818');
    EventChart.SetVrtAxis(MinTemp, MaxTemp, 1);
    EventChart.DrawVrtGrid();
    EventChart.DrawVrtAxisNumbers(true);
    EventChart.DrawVrtAxisLabel("Gard Celsius", true, -1);
    EventChart.SetDataPoints(Temperates);
    EventChart.DataColor = '#f4f';
    EventChart.DataLineWidth = 3;
    EventChart.DrawCurve();
  }

  if(Humidities.length > 1)
  {
    EventChart.SetVrtAxis(MinHum, MaxHum, 2);
    EventChart.SetAxisColors('#0aa', '#0ff');
    EventChart.SetGridColor('#066');
    EventChart.DrawVrtGrid();
    EventChart.DrawVrtAxisNumbers(false);
    EventChart.DrawVrtAxisLabel("Humidities", false, -1);
    EventChart.SetDataPoints(Humidities);
    EventChart.DataColor = '#0ff';
    EventChart.DataLineWidth = 3;
    EventChart.DrawCurve();
  }

}

function AddTemperature(value)
{
  if(Temperates.length > MaxEntries)
    Temperates.shift();
  Temperates.push(parseFloat(value));
  UpdateEventChart();
}
function AddHumidity(value)
{
  if(Humidities.length > MaxEntries)
    Humidities.shift();
  Humidities.push(parseFloat(value));
  //UpdateEventChart();
}


window.addEventListener('resize', ResizeEventChart);

window.addEventListener('load', (event) =>
{
  MakeEventChart();

  if(EventChart)
  {
    ResizeEventChart();

    setInterval
    (
      function()
        {
          ForVar('TEMPERATURE', responseText => AddTemperature(responseText) );
          ForVar('HUMIDITY',    responseText => AddHumidity(responseText) );

        },
        1000
    );

  }
});

)"));
}
#pragma endregion

#pragma region Command_JavaScript
CTextEmitter Command_JavaScript()
{
  return MakeTextEmitter(F(R"(
function UIUpdateTemperature(value)
{
  value = parseFloat(value).toFixed(2) + " \u00B0C";
  SetElementInnerHTML('CurTemp', value);
}
function UIUpdateHumidity(value)
{
  value = parseFloat(value).toFixed(2) + " %";
  SetElementInnerHTML('CurHum', value);
}

function SetTemperature(value)
{
  ForSetVar('TEMPERATURE', value, responseText => UIUpdateTemperature(responseText) );
}



function Update()
{
  ForVar('TEMPERATURE', responseText => UIUpdateTemperature(responseText) );
  ForVar('HUMIDITY',    responseText => UIUpdateHumidity(responseText) );
}


window.addEventListener('load', (event) =>
{
  Update();

  setInterval
  ( 
    function() 
      { 
        Update(); 
      },
      1000
    );
});

)"));
}
#pragma endregion

#pragma region MainPage_HtmlBody
CTextEmitter MainPage_HtmlBody()
{
  return MakeTextEmitter(F(R"(
<div>
  <canvas id="Canvas" width="360" height="200"></canvas>
</div>
<table class='entrytab'>
<caption>Current</caption>
  <tr>
    <th>Temperature</th>
    <td><div id="CurTemp"></div></td>
  </tr>
  <tr>
    <th>Humidity</th>
    <td><div id="CurHum"></div></td>
  </tr>
</table>

)"));
}
#pragma endregion

//END Website - Elements
#pragma endregion

#pragma region +Install and Setup

#pragma region InstallVarsAndCmds
void InstallVarsAndCmds(CController& c, CHost& host)
{
  c
    .SetVar("TEMPERATURE", [&host](auto p)
      {
        IUnit::CSensorInfoArgs Args;
        host.ReadSensorInfo(Args);
        return MakeTextEmitter(String(Args.Value.Temperature.value_or(0.0f)));
      })

    .SetVar("HUMIDITY", [&host](auto p)
      {
        IUnit::CSensorInfoArgs Args;
        host.ReadSensorInfo(Args);
        return MakeTextEmitter(String(Args.Value.Humidity.value_or(0.0f)));
      })
    ;
}
#pragma endregion

#pragma region AddMenuItems
void AddMenuItems(CController& c)
{
  c
    .AddMenuItem
    (
      {
        .MenuName = "Start",
        .URI = "/",
        .CSS = ActionUI_CSS(),
        .JavaScript = [](Stream& out)
        {
          out << ActionUI_JavaScript();
          out << UIUtils_JavaScript();
          out << Chart_JavaScript();
          out << Command_JavaScript();
          out << EventChart_JavaScript();
        },
        .Body = MainPage_HtmlBody(),
      }
    )
    ;
}

#pragma endregion

//END Install and Setup
#pragma endregion





#pragma region Epilog
} // namespace Sensor
} // namespace HBHomeKit
#pragma endregion
