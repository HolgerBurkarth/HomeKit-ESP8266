#pragma region Prolog
/*******************************************************************
$CRT 05 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Sensor.cpp<< 08 Okt 2024  06:31:41 - (c) proDAD
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

    if(mInterval > 1000)
    {
      CSensorInfoArgs Args;
      Args.Value = mGetter();
      Super->WriteSensorInfo(Args);
    }
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

#pragma region CContinuousEventRecorderUnit - Implementation
struct CContinuousEventRecorderUnit : CUnitBase
{
  #pragma region Fields
  CEventRecorder Record;
  CSensorInfo mLastInfo;
  CSuperInvoke mSuperInvoke{};
  time_t mLastTime{};

  #pragma endregion

  #pragma region Construction
  CContinuousEventRecorderUnit(int intervalSec)
  {
    Record.RecordIntervalSec = std::max(1, intervalSec);
  }
  #pragma endregion

  #pragma region Start
  virtual void Start(CVoidArgs&) override
  {
    //Serial.printf("Size of CEvent = %d bytes\n", sizeof(CEvent));
    Record.reserve(288/2);
  }

  #pragma endregion

  #pragma region WriteMotorInfo
  void WriteSensorInfo(CSensorInfoArgs& args) override
  {
    if(mSuperInvoke(Super, &IUnit::WriteSensorInfo, args))
    {
      if(mLastInfo.Assign(args.Value))
      {
        time_t Cur; time(&Cur);
        if(difftime(Cur, mLastTime) >= Record.RecordIntervalSec)
        {
          Record.push_back(mLastInfo);
          mLastTime = Cur;

          #if 0
          struct tm timeinfo;
          if(smart_gmtime(&timeinfo))
          {
            char TimeStamp[32];
            strftime(TimeStamp, sizeof(TimeStamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

            VERBOSE("%s Record sensor readings: T=%0.1f H=%0.1f"
              , TimeStamp
              , mLastInfo.Temperature.value_or(0.0f)
              , mLastInfo.Humidity.value_or(0.0f)
            );
          }
          #endif
        }
      }
    }
  }
  #pragma endregion

  #pragma region QueryEventRecorder
  void QueryEventRecorder(CEventRecorderArgs& args) override
  {
    args.Value = MakeStaticPtr(Record);
    args.Handled = true;
  }

  #pragma endregion

};

IUnit_Ptr MakeContinuousEventRecorderUnit(int intervalSec)
{
  return std::make_shared<CContinuousEventRecorderUnit>(intervalSec);
}

//END
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
var EventTime = []; // array of longs
var Temperates = []; // array of floats
var Humidities = []; // array of floats
var MaxEntries = 288;
var IntervalMS = 1000;
var MaxTempIndex = -1;
var MinTempIndex = -1;
var MaxHumIndex = -1;
var MinHumIndex = -1;


function MakeEventChart()
{
  EventChartCanvasHD = document.getElementById('Canvas');
  EventChart = new CChart(EventChartCanvasHD);
  EventChart.AutoMargins('numbers', 'numbers left right');
  EventChart.XDataToString = function(x) 
    {
      if(x >= 0 && x < EventTime.length)
      {
        var d = new Date(EventTime[x]);
        return d.toLocaleTimeString().substring(0, 5);
      }
      return '';
    };

  /* Redraw Canvas if needed */
  EventChartCanvasHD.addEventListener('resize', ResizeEventChart);
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

  MaxTempIndex = -1;
  MinTempIndex = -1;
  MaxHumIndex = -1;
  MinHumIndex = -1;


  if(Temperates.length > 0)
  {
    if(Temperates.length > 1)
    {
      MaxTemp = Temperates[0];
      MinTemp = Temperates[0];
      MaxTempIndex = 0;
      MinTempIndex = 0;
      for(var i = 1; i < Temperates.length; ++i)
      {
        if(Temperates[i] > MaxTemp)
        {
          MaxTemp = Temperates[i];
          MaxTempIndex = i;
        }
        if(Temperates[i] < MinTemp)
        {
          MinTemp = Temperates[i];
          MinTempIndex = i;
        }
      }
    }
    else
    {
      MinTemp = Math.min(...Temperates);
      MaxTemp = Math.max(...Temperates);
    }
  }
  if(Humidities.length > 0)
  {
    if(Humidities.length > 1)
    {
      MaxHum = Humidities[0];
      MinHum = Humidities[0];
      MaxHumIndex = 0;
      MinHumIndex = 0;
      for(var i = 1; i < Humidities.length; ++i)
      {
        if(Humidities[i] > MaxHum)
        {
          MaxHum = Humidities[i];
          MaxHumIndex = i;
        }
        if(Humidities[i] < MinHum)
        {
          MinHum = Humidities[i];
          MinHumIndex = i;
        }
      }
    }
    else
    {
      MinHum = Math.min(...Humidities);
      MaxHum = Math.max(...Humidities);
    }
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

  EventChart.SetHrzAxis(0,  MaxEntries,  MaxEntries/8);
  EventChart.DrawHrzGrid();
  EventChart.DrawHrzAxisNumbers();

  if(Temperates.length > 0)
  {
    EventChart.SetAxisColors('magenta');
    if(MaxTemp-MinTemp < 3)
    {
      EventChart.SetGridColor('#606');
      EventChart.SetVrtAxis(MinTemp, MaxTemp, 0.25);
      EventChart.DrawVrtGrid();
    }

    EventChart.SetGridColor('magenta');
    EventChart.SetVrtAxis(MinTemp, MaxTemp, 1);
    EventChart.DrawBoundaryBox(Temperates[MinTempIndex], Temperates[MaxTempIndex], '#f4f', 0.2);
    EventChart.DrawVrtGrid();
    EventChart.DrawVrtAxisNumbers(true);
    EventChart.DrawVrtAxisLabel("Gard Celsius", true, -1);
  }

  if(Humidities.length > 0)
  {
    EventChart.SetAxisColors('cyan');
    EventChart.SetVrtAxis(MinHum, MaxHum, 2);
    EventChart.SetGridColor('cyan');
    EventChart.DrawBoundaryBox(Humidities[MinHumIndex], Humidities[MaxHumIndex], 'cyan', 0.15);
    EventChart.DrawVrtGrid();
    EventChart.DrawVrtAxisNumbers(false);
    EventChart.DrawVrtAxisLabel("Humidities", false, -1);
  }


  if(Temperates.length > 0)
  {
    EventChart.SetVrtAxis(MinTemp, MaxTemp, 1);
    EventChart.SetDataPoints(Temperates);
    EventChart.SetDataColor('magenta');
    EventChart.DrawCurve();
  }

  if(Humidities.length > 0)
  {
    EventChart.SetVrtAxis(MinHum, MaxHum, 2);
    EventChart.SetDataPoints(Humidities);
    EventChart.SetDataColor('cyan');
    EventChart.DrawCurve();
  }

  if(MinTempIndex >= 0)
  {
    SetElementInnerHTML('MinTemp', FormatTemperatureAt(MinTempIndex));
    SetElementInnerHTML('MaxTemp', FormatTemperatureAt(MaxTempIndex));
  }

  if(MaxHumIndex >= 0)
  {
    SetElementInnerHTML('MaxHum', FormatHumidityAt(MaxHumIndex));
    SetElementInnerHTML('MinHum', FormatHumidityAt(MinHumIndex));
  }
}

function FormatTemperatureAt(index)
{
  return parseFloat(Temperates[index]).toFixed(2) + " \u00B0C <span class='unit'>" 
    + EventChart.XDataToString(index) + "</span>";
}
function FormatHumidityAt(index)
{
  return parseFloat(Humidities[index]).toFixed(2) + " % <span class='unit'>" 
    + EventChart.XDataToString(index) + "</span>";
}


/*
responseText: "2024-10-09T14:12:41Z;T:0.0;H:0.0;\n" ...
*/
function ParseEntries(responseText)
{
  EventTime = [];
  Humidities = [];
  Temperates = [];

  var lines = responseText.split('\n');
  var Now = new Date();

  for(var i = 0; i < lines.length; ++i)
  {
    var line = lines[i];
    if(line.length < 1)
      continue;

    var parts = line.split(';');
    if(parts.length < 2)
      continue;

    var time = new Date(parts[0]);

    EventTime.push(time.getTime());

    for(var k=1; k < parts.length; ++k)
    {
      var Args = parts[k].split(':');
      if(Args.length < 2)
        continue;
      if(Args[0] == 'T')
        Temperates.push(parseFloat(Args[1]));
      else if(Args[0] == 'H')
        Humidities.push(parseFloat(Args[1]));
    }
  }
  UpdateEventChart();
}

function StartMaxEntries(value)
{
  MaxEntries = parseFloat(value);
  if(isNaN(MaxEntries) || MaxEntries < 10)
    MaxEntries = 288;
}

function StartPeriodicalUpdate(value)
{
  IntervalMS = parseFloat(value);
  if(isNaN(IntervalMS) || IntervalMS < 100)
    IntervalMS = 1000;

  ForVar('ENTRIES',  responseText => AutoUpdateEntries(responseText) );
}

function AutoUpdateEntries(responseText)
{
  ParseEntries(responseText);

  setTimeout 
  (
    function()
    {
      ForVar('ENTRIES',  responseText => AutoUpdateEntries(responseText) );
    },
    IntervalMS
  );
}



window.addEventListener('load', (event) =>
{
  MakeEventChart();

  if(EventChart)
  {
    ResizeEventChart();

    setTimeout
    (
      function()
      {
        ForVar('MAX_RECORD_ENTRIES', responseText => StartMaxEntries(responseText) );
        ForVar('RECORD_INTERVAL', responseText => StartPeriodicalUpdate(responseText) );

      },
      500
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
      5000
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
<caption>Measurements</caption>
  <tr>
    <th>Temperature</th>
    <td><div id="CurTemp"></div></td>
  </tr>
  <tr>
    <th>Humidity</th>
    <td><div id="CurHum"></div></td>
  </tr>

  <tr><td><span class = 'unit'>&nbsp;</span></td></tr>

  <tr>
    <th>Max. Temperature</th>
    <td><div id="MaxTemp"></div></td>
  </tr>
  <tr>
    <th>Min. Temperature</th>
    <td><div id="MinTemp"></div></td>
  </tr>

  <tr>
    <th>Max. Humidity</th>
    <td><div id="MaxHum"></div></td>
  </tr>
  <tr>
    <th>Min. Humidity</th>
    <td><div id="MinHum"></div></td>
  </tr>

</table>
<br/>
<table class='entrytab'>
<caption>Settings</caption>
  <tr>
    <th>Recording interval</th>
    <td>{RECORD_INTERVAL_STR}</td>
  </tr>
  <tr>
    <th>Total recording time</th>
    <td>{TOTAL_RECORD_TIME_STR}</td>
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

    .SetVar("ENTRIES", [&host](auto p)
      {
        IUnit::CEventRecorderArgs Args;
        host.QueryEventRecorder(Args);
        if(!Args.Value)
          return MakeTextEmitter();

        return Args.Value->EntriesEmitter();
      })

    .SetVar("RECORD_INTERVAL", [&host](auto p)
      {
        int Interval = 1;
        IUnit::CEventRecorderArgs Args;
        host.QueryEventRecorder(Args);
        if(Args.Value)
          Interval = Args.Value->RecordIntervalSec;
        return MakeTextEmitter(String(Interval * 1000));
      })

    .SetVar("RECORD_INTERVAL_STR", [&host](auto p)
      {
        int Mins = 0, Secs = 1;
        char Buf[32];
        IUnit::CEventRecorderArgs Args;
        host.QueryEventRecorder(Args);
        if(Args.Value)
          Secs = Args.Value->RecordIntervalSec;

        Mins = Secs / 60;
        Secs -= Mins * 60;

        snprintf_P(Buf, sizeof(Buf), PSTR("%dm %ds"), Mins, Secs);
        return MakeTextEmitter(Buf);
      })

    .SetVar("MAX_RECORD_ENTRIES", [&host](auto p)
      {
        int MaxEntries = 1;
        IUnit::CEventRecorderArgs Args;
        host.QueryEventRecorder(Args);
        if(Args.Value)
          MaxEntries = Args.Value->MaxEntries;
        return MakeTextEmitter(String(MaxEntries));
      })

    .SetVar("TOTAL_RECORD_TIME_STR", [&host](auto p)
      {
        int Secs = 1, Mins =0, Hours = 0;
        char Buf[32];
        IUnit::CEventRecorderArgs Args;
        host.QueryEventRecorder(Args);
        if(Args.Value)
          Secs = Args.Value->MaxEntries * Args.Value->RecordIntervalSec;

        Hours = Secs / 3600;
        Secs -= Hours * 3600;
        Mins = Secs / 60;

        snprintf_P(Buf, sizeof(Buf), PSTR("%dh %dm"), Hours, Mins);
        return MakeTextEmitter(Buf);
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
