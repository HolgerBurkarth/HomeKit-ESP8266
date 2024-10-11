#pragma region Prolog
/*******************************************************************
$CRT 09 Okt 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_Thermostat.cpp<< 11 Okt 2024  11:21:22 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Includes
#include <HomeKit_Thermostat.h>

namespace HBHomeKit
{
namespace Thermostat
{
#pragma endregion

#pragma region +Unit - Objects

#pragma region CControlUnit - Implementation
class CControlUnit : public CUnitBase
{
  #pragma region Fields
  const uint32_t TickerInterval{ 1000 }; // 1 Hz

  CSensorInfo mSensorInfo{};
  CMessage    mLastMessage{};

public:
  #pragma endregion

  #pragma region Override Methods

  #pragma region Start
  void Start(CVoidArgs&) override
  {

    Ctrl->Ticker.Start(TickerInterval, [this](auto args)
      {
        SendNextNessageToHomeKit();
      });

  }

  #pragma endregion

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
    mSensorInfo.Assign(args.Value);
  }

  #pragma endregion

  #pragma region QueryNextMessage
  void QueryNextMessage(CMessageArgs& args) override
  {
    const ECharacteristicFlags Flags = System->GetFlags();
    const auto TargetState = System->GetTargetState();
    auto& Msg = args.Value;
    const auto TargetTemperature = System->GetTargetTemperature();
    const auto HeatingThresholdTemperature = System->GetHeatingThresholdTemperature();
    const auto CoolingThresholdTemperature = System->GetCoolingThresholdTemperature();

    switch(TargetState)
    {
      default:
      case HOMEKIT_TARGET_HEATING_COOLING_STATE_OFF:
        Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_OFF;
        break;

      case HOMEKIT_TARGET_HEATING_COOLING_STATE_HEAT:
        if(TargetTemperature) 
        {
          if(mSensorInfo.Temperature < *TargetTemperature)
            Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_HEAT;
        }
        else if(HeatingThresholdTemperature)
        {
          if(mSensorInfo.Temperature < *HeatingThresholdTemperature)
            Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_HEAT;
        }
        break;

      case HOMEKIT_TARGET_HEATING_COOLING_STATE_COOL:
        if(TargetTemperature)
        {
          if(mSensorInfo.Temperature > *TargetTemperature)
            Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_COOL;
        }
        else if(CoolingThresholdTemperature)
        {
          if(mSensorInfo.Temperature > *CoolingThresholdTemperature)
            Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_COOL;
        }
        break;

      case HOMEKIT_TARGET_HEATING_COOLING_STATE_AUTO:
        if(HeatingThresholdTemperature)
        {
          if(mSensorInfo.Temperature < *HeatingThresholdTemperature)
            Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_HEAT;
        }
        else if(TargetTemperature)
        {
          if(mSensorInfo.Temperature < *TargetTemperature)
            Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_HEAT;
        }

        if(CoolingThresholdTemperature)
        {
          if(mSensorInfo.Temperature > *CoolingThresholdTemperature)
            Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_COOL;
        }
        else if(TargetTemperature)
        {
          if(mSensorInfo.Temperature > *TargetTemperature)
            Msg.HCState = HOMEKIT_CURRENT_HEATING_COOLING_STATE_COOL;
        }
        break;
    }

    //Serial.printf("HCState = %d\n", Msg.HCState.value_or(HOMEKIT_CURRENT_HEATING_COOLING_STATE_OFF));
    args.Handled = true;
  }
  #pragma endregion

  #pragma region SetCurrentState
  void SetCurrentState(CCurrentStateArgs& args) override
  {
    INFO("SetCurrentState: %s", to_string(args.Value));
    System->SetCurrentState(args.Value);
  }
  #pragma endregion

  #pragma region OnTargetStateChanged
  void OnTargetStateChanged(CTargetStateArgs& args) override
  {
    CorrectTargetState(args.Value);
  }
  #pragma endregion



  //END Override Methods
  #pragma endregion

  #pragma region SendNextNessageToHomeKit
  void SendNextNessageToHomeKit()
  {
    CMessageArgs MsgArgs;
    Super->QueryNextMessage(MsgArgs);
    if(!MsgArgs.Handled)
      return;
    const CMessage& Msg = MsgArgs;

    /* Send values to HomeKit only when they change.
    * Never do this permanently, or external value changes
    * will be overwritten and cannot be processed.
    */
    {
      auto HCState = Msg.HCState.value_or(HOMEKIT_CURRENT_HEATING_COOLING_STATE_OFF);
      if(!mLastMessage.HCState || HCState != *mLastMessage.HCState)
      {
        CCurrentStateArgs Args = HCState;
        Super->SetCurrentState(Args);
        mLastMessage.HCState = HCState;
      }
    }
  }

  #pragma endregion

  #pragma region CorrectTargetState
  void CorrectTargetState(HOMEKIT_TARGET_HEATING_COOLING_STATE state)
  {
    const auto TargetState = state;
    auto Flags = System->GetFlags();
    const bool SupportsHeating = (Flags & ECharacteristicFlags::SupportsHeating);
    const bool SupportsCooling = (Flags & ECharacteristicFlags::SupportsCooling);
    HOMEKIT_TARGET_HEATING_COOLING_STATE NewState = TargetState;
    switch(TargetState)
    {
      default:
      case HOMEKIT_TARGET_HEATING_COOLING_STATE_OFF:
        break;

      case HOMEKIT_TARGET_HEATING_COOLING_STATE_HEAT:
        if(!SupportsHeating)
          NewState = HOMEKIT_TARGET_HEATING_COOLING_STATE_OFF;
        break;

      case HOMEKIT_TARGET_HEATING_COOLING_STATE_COOL:
        if(!SupportsCooling)
          NewState = HOMEKIT_TARGET_HEATING_COOLING_STATE_OFF;
        break;

      case HOMEKIT_TARGET_HEATING_COOLING_STATE_AUTO:
        if(!SupportsHeating && SupportsCooling)
          NewState = HOMEKIT_TARGET_HEATING_COOLING_STATE_COOL;
        else if(SupportsHeating && !SupportsCooling)
          NewState = HOMEKIT_TARGET_HEATING_COOLING_STATE_HEAT;
        break;
    }

    if(NewState != TargetState)
    {
      INFO("TargetState corrected: %s", to_string(NewState));
      System->SetTargetState(NewState);
    }
  }
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

#pragma region COnChangedUnit - Implementation
class COnChangedUnit : public CUnitBase
{
  using NotifyFunc = std::function<void(const CChangeInfo&)>;

  #pragma region Fields
  uint32_t    mIntervalMS;
  NotifyFunc  mNotify;
  CChangeInfo mInfo;

  #pragma endregion

  #pragma region Construction
public:
  COnChangedUnit(uint32_t intervalMS, NotifyFunc&& func)
    : mIntervalMS(intervalMS)
    , mNotify(std::move(func))
  {}

  #pragma endregion

  #pragma region Override Methods

  #pragma region Start
  void Start(CVoidArgs&) override
  {

    Ctrl->Ticker.Start(mIntervalMS ? mIntervalMS:1000, [this](auto args)
      {
        bool OnlyChanges = (mIntervalMS == 0);

        if(OnlyChanges && !mInfo.Changed)
          return;

        mInfo.Flags = System->GetFlags();
        mInfo.CurrentState = System->GetCurrentState();
        mInfo.DisplayUnit = System->GetDisplayUnit();
        mInfo.TargetState = System->GetTargetState();
        mInfo.CoolingThresholdTemperature = System->GetCoolingThresholdTemperature();
        mInfo.HeatingThresholdTemperature = System->GetHeatingThresholdTemperature();
        mInfo.TargetHumidity = System->GetTargetHumidity();
        mInfo.TargetTemperature = System->GetTargetTemperature();

        mNotify(mInfo);
        mInfo.Changed = false;
      });

  }

  #pragma endregion

  #pragma region WriteSensorInfo

  void WriteSensorInfo(CSensorInfoArgs& args) override
  {
    if(mInfo.Sensor.Assign(args.Value))
      mInfo.Changed = true;
  }

  #pragma endregion


  //END Override Methods
  #pragma endregion
};

IUnit_Ptr MakeOnChangedUnit(uint32_t intervalMS, std::function<void(const CChangeInfo&)> func)
{
  return std::make_shared<COnChangedUnit>(intervalMS, std::move(func));
}

//END COnChangedUnit
#pragma endregion

#pragma region COnStateChangedUnit - Implementation
class COnStateChangedUnit : public CUnitBase
{
  using NotifyFunc = std::function<void(HOMEKIT_CURRENT_HEATING_COOLING_STATE)>;

  #pragma region Fields
  NotifyFunc  mNotify;

  #pragma endregion

  #pragma region Construction
public:
  COnStateChangedUnit(NotifyFunc&& func)
    : mNotify(std::move(func))
  {}

  #pragma endregion

  #pragma region Override Methods

  #pragma region SetCurrentState
  void SetCurrentState(CCurrentStateArgs& args) override
  {
    mNotify(args.Value);
  }

  #pragma endregion


  //END Override Methods
  #pragma endregion
};

IUnit_Ptr MakeOnStateChangedUnit(std::function<void(HOMEKIT_CURRENT_HEATING_COOLING_STATE)> func)
{
  return std::make_shared<COnStateChangedUnit>(std::move(func));
}

//END COnChangedUnit
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
  value = parseFloat(value).toFixed(2) + "&nbsp;\u00B0C";
  SetElementInnerHTML('CurTemp', value);
}
function UIUpdateHumidity(value)
{
  value = parseFloat(value).toFixed(2) + "&nbsp;%";
  SetElementInnerHTML('CurHum', value);
}

function UIUpdateTargetState(value)
{
  SetElementInnerHTML('TargetState', value);
}
function UIUpdateCurrentState(value)
{
  SetElementInnerHTML('CurrentState', value);
}

function UIUpdateTargetTemperature(value)
{
  value = parseFloat(value).toFixed(2) + "&nbsp;\u00B0C";
  SetElementInnerHTML('TargetTemp', value);
}
function UIUpdateTargetHumidity(value)
{
  value = parseFloat(value).toFixed(2) + "&nbsp;%";
  SetElementInnerHTML('TargetHum', value);
}

function UIUpdateCoolingThresholdTemperature(value)
{
  value = parseFloat(value).toFixed(2) + "&nbsp;\u00B0C";
  SetElementInnerHTML('CoolingTemp', value);
}
function UIUpdateHeatingThresholdTemperature(value)
{
  value = parseFloat(value).toFixed(2) + "&nbsp;\u00B0C";
  SetElementInnerHTML('HeatingTemp', value);
}



function SetTemperature(value)
{
  ForSetVar('TEMPERATURE', value, responseText => UIUpdateTemperature(responseText) );
}
function SetTargetState(value)
{
  ForSetVar('TARGET_STATE', value, responseText => UIUpdateTargetState(responseText) );
}
function SetTargetTemperature(value)
{
  ForSetVar('TARGET_TEMPERATURE', value, responseText => UIUpdateTargetTemperature(responseText) );
}
function SetTargetHumidity(value)
{
  ForSetVar('TARGET_HUMIDITY', value, responseText => UIUpdateTargetHumidity(responseText) );
}
function SetCoolingThresholdTemperature(value)
{
  ForSetVar('COOLING_THRESHOLD_TEMPERATURE', value, responseText => UIUpdateCoolingThresholdTemperature(responseText) );
}
function SetHeatingThresholdTemperature(value)
{
  ForSetVar('HEATING_THRESHOLD_TEMPERATURE', value, responseText => UIUpdateHeatingThresholdTemperature(responseText) );
}



function Update()
{
  ForVar('TEMPERATURE', responseText => UIUpdateTemperature(responseText) );
  ForVar('HUMIDITY',    responseText => UIUpdateHumidity(responseText) );
  ForVar('TARGET_STATE', responseText => UIUpdateTargetState(responseText) );
  ForVar('CURRENT_STATE', responseText => UIUpdateCurrentState(responseText) );
  ForVar('TARGET_TEMPERATURE', responseText => UIUpdateTargetTemperature(responseText) );
  ForVar('TARGET_HUMIDITY', responseText => UIUpdateTargetHumidity(responseText) );
  ForVar('COOLING_THRESHOLD_TEMPERATURE', responseText => UIUpdateCoolingThresholdTemperature(responseText) );
  ForVar('HEATING_THRESHOLD_TEMPERATURE', responseText => UIUpdateHeatingThresholdTemperature(responseText) );


}



window.addEventListener('load', (event) =>
{
  Update();

  ForVar('SUPPORTS_HEATING_THRESHOLD',  responseText => VisibleElement('HeatingThresholdTemperatureButtons', responseText) );
  ForVar('SUPPORTS_COOLING_THRESHOLD',  responseText => VisibleElement('CoolingThresholdTemperatureButtons', responseText) );
  ForVar('SUPPORTS_HUMIDITY_ACTOR',     responseText => VisibleElement('TargetHumidityButtons', responseText) );

  setInterval
  ( 
    function() 
      { 
        Update(); 
      },
      2000
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

#pragma region HomeKit_HtmlBody
CTextEmitter HomeKit_HtmlBody()
{
  return MakeTextEmitter(F(R"(
<div>
  <canvas id="Canvas" width="360" height="120"></canvas>
</div>

<table class='entrytab'>
<tr>
  <th>Target State</th>
  <td>
    {ACTION_BUTTON:SetTargetState('off'):OFF}
    {ACTION_BUTTON:SetTargetState('heat'):HEAT}
    {ACTION_BUTTON:SetTargetState('cool'):COOL}
    {ACTION_BUTTON:SetTargetState('auto'):AUTO}
  </td>
</tr>
<tr>
  <th>Target Temperature</th>
  <td>
    {ACTION_BUTTON:SetTargetTemperature('15'):15&deg;C}
    {ACTION_BUTTON:SetTargetTemperature('28'):28&deg;C}
  </td>
</tr>
<tr id='TargetHumidityButtons'>
  <th>Target Humidity</th>
  <td>
    {ACTION_BUTTON:SetTargetHumidity('30'):30%}
    {ACTION_BUTTON:SetTargetHumidity('60'):60%}
  </td>
</tr>
<tr id='HeatingThresholdTemperatureButtons'>
  <th>Heating Threshold Temperature</th>
  <td>
    {ACTION_BUTTON:SetHeatingThresholdTemperature('15'):15&deg;C}
    {ACTION_BUTTON:SetHeatingThresholdTemperature('24'):24&deg;C}
  </td>
</tr>
<tr id='CoolingThresholdTemperatureButtons'>
  <th>Cooling Threshold Temperature</th>
  <td>
    {ACTION_BUTTON:SetCoolingThresholdTemperature('20'):20&deg;C}
    {ACTION_BUTTON:SetCoolingThresholdTemperature('30'):30&deg;C}
  </td>
</tr>
</table>
<br/>


<table class='entrytab'>
<caption>HomeKit States</caption>
<tr>
  <th>Current State</th> <td><div id="CurrentState">...</div></td>
  <th>Target State</th> <td><div id="TargetState">...</div></td>
</tr>

<tr>
  <th>Current Temperature</th> <td><div id="CurTemp">...</div></td>
  <th>Current Humidity</th> <td><div id="CurHum">...</div></td>
</tr>

<tr>
  <th>Target Temperature</th> <td><div id="TargetTemp">...</div></td>
  <th>Target Humidity</th> <td><div id="TargetHum">...</div></td>
</tr>

<tr>
  <th>Cooling Threshold</th> <td><div id="CoolingTemp">...</div></td>
  <th>Heating Threshold</th> <td><div id="HeatingTemp">...</div></td>
</tr>
</table>


<div id="Debug"> </div>
)"));
}

#pragma endregion


//END Website - Elements
#pragma endregion

#pragma region +Install and Setup

#pragma region InstallVarsAndCmds
void InstallVarsAndCmds(CController& c, CThermostatService& svr, CHost& host)
{
  c
    #pragma region TARGET_STATE
    .SetVar("TARGET_STATE", [&](auto p)
      {
        if(p.Args[0] != nullptr)
        {
          HOMEKIT_TARGET_HEATING_COOLING_STATE State{};
          const char* Arg0Text = static_value_cast<const char*>(*p.Args[0]);
//Serial.printf("Set Target State: \"%s\"\n", Arg0Text);
          if(strcmp_P(Arg0Text, PSTR("off")) == 0)
            State = HOMEKIT_TARGET_HEATING_COOLING_STATE_OFF;
          else if(strcmp_P(Arg0Text, PSTR("heat")) == 0)
            State = HOMEKIT_TARGET_HEATING_COOLING_STATE_HEAT;
          else if(strcmp_P(Arg0Text, PSTR("cool")) == 0)
            State = HOMEKIT_TARGET_HEATING_COOLING_STATE_COOL;
          else if(strcmp_P(Arg0Text, PSTR("auto")) == 0)
            State = HOMEKIT_TARGET_HEATING_COOLING_STATE_AUTO;
          else
            ERROR("Unknown target state: \"%s\"", Arg0Text);

          modify_value_and_notify(&svr.TargetHeatingCoolingState, State);
        }

        return [State = static_value_cast<HOMEKIT_TARGET_HEATING_COOLING_STATE>(&svr.TargetHeatingCoolingState)](Stream& out)
          {
            switch(State)
            {
              default:
              case HOMEKIT_TARGET_HEATING_COOLING_STATE_OFF:
                out << F("off");
                break;
              case HOMEKIT_TARGET_HEATING_COOLING_STATE_HEAT:
                out << F("heat");
                break;
              case HOMEKIT_TARGET_HEATING_COOLING_STATE_COOL:
                out << F("cool");
                break;
              case HOMEKIT_TARGET_HEATING_COOLING_STATE_AUTO:
                out << F("auto");
                break;
            }
          };
      })

    #pragma endregion

    #pragma region CURRENT_STATE
    .SetVar("CURRENT_STATE", [&](auto p)
      {
        if(p.Args[0] != nullptr)
        {
          HOMEKIT_CURRENT_HEATING_COOLING_STATE State{};
          const char* Arg0Text = static_value_cast<const char*>(*p.Args[0]);
          if(strcmp_P(Arg0Text, PSTR("off")) == 0)
            State = HOMEKIT_CURRENT_HEATING_COOLING_STATE_OFF;
          else if(strcmp_P(Arg0Text, PSTR("heat")) == 0)
            State = HOMEKIT_CURRENT_HEATING_COOLING_STATE_HEAT;
          else if(strcmp_P(Arg0Text, PSTR("cool")) == 0)
            State = HOMEKIT_CURRENT_HEATING_COOLING_STATE_COOL;
          else
            ERROR("Unknown current state: \"%s\"", Arg0Text);

          modify_value_and_notify(&svr.CurrentHeatingCoolingState, State);
        }

        return [State = static_value_cast<HOMEKIT_CURRENT_HEATING_COOLING_STATE>(&svr.CurrentHeatingCoolingState)](Stream& out)
          {
            switch(State)
            {
              default:
              case HOMEKIT_CURRENT_HEATING_COOLING_STATE_OFF:
                out << F("off");
                break;
              case HOMEKIT_CURRENT_HEATING_COOLING_STATE_HEAT:
                out << F("heat");
                break;
              case HOMEKIT_CURRENT_HEATING_COOLING_STATE_COOL:
                out << F("cool");
                break;
            }
          };
      })

    #pragma endregion

    #pragma region TARGET_TEMPERATURE
    .SetVar("TARGET_TEMPERATURE", [&](auto p)
      {
        if(p.Args[0] != nullptr)
        {
          float Temp = convert_value<float>(*p.Args[0]);
          modify_value_and_notify(&svr.TargetTemperature, Temp);
        }
        return MakeTextEmitter(svr.TargetTemperature);
      })

    #pragma endregion

    #pragma region TARGET_HUMIDITY
    .SetVar("TARGET_HUMIDITY", [&](auto p)
      {
        if(p.Args[0] != nullptr)
        {
          float Temp = convert_value<float>(*p.Args[0]);
          modify_value_and_notify(&svr.TargetHumidity, Temp);
        }
        return MakeTextEmitter(svr.TargetHumidity);
      })

    #pragma endregion

    #pragma region COOLING_THRESHOLD_TEMPERATURE
    .SetVar("COOLING_THRESHOLD_TEMPERATURE", [&](auto p)
    {
      if(p.Args[0] != nullptr)
      {
        float Temp = convert_value<float>(*p.Args[0]);
        modify_value_and_notify(&svr.CoolingThresholdTemperature, Temp);
      }

      return MakeTextEmitter(svr.CoolingThresholdTemperature);
    })

    #pragma endregion

    #pragma region HEATING_THRESHOLD_TEMPERATURE
    .SetVar("HEATING_THRESHOLD_TEMPERATURE", [&](auto p)
    {
      if(p.Args[0] != nullptr)
      {
        float Temp = convert_value<float>(*p.Args[0]);
        modify_value_and_notify(&svr.HeatingThresholdTemperature, Temp);
      }

      return MakeTextEmitter(svr.HeatingThresholdTemperature);
    })

    #pragma endregion


    #pragma region TEMPERATURE
    .SetVar("TEMPERATURE", [&host](auto p)
      {
        IUnit::CSensorInfoArgs Args;
        host.ReadSensorInfo(Args);
        return MakeTextEmitter(String(Args.Value.Temperature.value_or(0.0f)));
      })

    #pragma endregion

    #pragma region HUMIDITY
    .SetVar("HUMIDITY", [&host](auto p)
      {
        IUnit::CSensorInfoArgs Args;
        host.ReadSensorInfo(Args);
        return MakeTextEmitter(String(Args.Value.Humidity.value_or(0.0f)));
      })

    #pragma endregion

    #pragma region ENTRIES
    .SetVar("ENTRIES", [&host](auto p)
      {
        IUnit::CEventRecorderArgs Args;
        host.QueryEventRecorder(Args);
        if(!Args.Value)
          return MakeTextEmitter();

        return Args.Value->EntriesEmitter();
      })

    #pragma endregion

    #pragma region RECORD_INTERVAL
    .SetVar("RECORD_INTERVAL", [&host](auto p)
      {
        int Interval = 1;
        IUnit::CEventRecorderArgs Args;
        host.QueryEventRecorder(Args);
        if(Args.Value)
          Interval = Args.Value->RecordIntervalSec;
        return MakeTextEmitter(String(Interval * 1000));
      })

    #pragma endregion

    #pragma region RECORD_INTERVAL_STR
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

    #pragma endregion

    #pragma region MAX_RECORD_ENTRIES
    .SetVar("MAX_RECORD_ENTRIES", [&host](auto p)
      {
        int MaxEntries = 1;
        IUnit::CEventRecorderArgs Args;
        host.QueryEventRecorder(Args);
        if(Args.Value)
          MaxEntries = Args.Value->MaxEntries;
        return MakeTextEmitter(String(MaxEntries));
      })

    #pragma endregion

    #pragma region TOTAL_RECORD_TIME_STR
    .SetVar("TOTAL_RECORD_TIME_STR", [&host](auto p)
    {
      int Secs = 1, Mins = 0, Hours = 0;
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

    #pragma endregion

    #pragma region SUPPORTS_HUMIDITY_ACTOR
    .SetVar("SUPPORTS_HUMIDITY_ACTOR", [&svr](auto p)
      {
        auto Flags = svr.Flags;
        return MakeTextEmitter(Flags & ECharacteristicFlags::HumidityActor);
      })

    #pragma endregion

    #pragma region SUPPORTS_COOLING_THRESHOLD
    .SetVar("SUPPORTS_COOLING_THRESHOLD", [&svr](auto p)
      {
        auto Flags = svr.Flags;
        return MakeTextEmitter(Flags & ECharacteristicFlags::CoolingThreshold);
      })

    #pragma endregion

    #pragma region SUPPORTS_HEATING_THRESHOLD
    .SetVar("SUPPORTS_HEATING_THRESHOLD", [&svr](auto p)
      {
        auto Flags = svr.Flags;
        return MakeTextEmitter(Flags & ECharacteristicFlags::HeatingThreshold);
      })

    #pragma endregion


  ;
}
#pragma endregion

#pragma region AddMenuItems
void AddMenuItems(CController& c)
{
  c
    #pragma region Start
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

    #pragma endregion

    #pragma region HomeKit
    .AddMenuItem
    (
      {
        .MenuName = "HomeKit",
        .URI = "/homekit",
        .CSS = ActionUI_CSS(),
        .JavaScript = [](Stream& out)
        {
          out << ActionUI_JavaScript();
          out << UIUtils_JavaScript();
          out << Chart_JavaScript();
          out << EventChart_JavaScript();
          out << Command_JavaScript();
        },
        .Body = [](Stream& out)
        {
          out << HomeKit_HtmlBody();
        },
      }
    )

    #pragma endregion

  ;
}

#pragma endregion

//END Install and Setup
#pragma endregion

#pragma region to_string
const char* to_string(HOMEKIT_TARGET_HEATING_COOLING_STATE state)
{
  switch(state)
  {
    case HOMEKIT_TARGET_HEATING_COOLING_STATE_OFF: return "OFF";
    case HOMEKIT_TARGET_HEATING_COOLING_STATE_HEAT: return "HEAT";
    case HOMEKIT_TARGET_HEATING_COOLING_STATE_COOL: return "COOL";
    case HOMEKIT_TARGET_HEATING_COOLING_STATE_AUTO: return "AUTO";
    default: return "UNKNOWN";
  }
}

const char* to_string(HOMEKIT_CURRENT_HEATING_COOLING_STATE state)
{
  switch(state)
  {
    case HOMEKIT_CURRENT_HEATING_COOLING_STATE_OFF: return "OFF";
    case HOMEKIT_CURRENT_HEATING_COOLING_STATE_HEAT: return "HEAT";
    case HOMEKIT_CURRENT_HEATING_COOLING_STATE_COOL: return "COOL";
    default: return "UNKNOWN";
  }
}

#pragma endregion



#pragma region Epilog
} // namespace Thermostat
} // namespace HBHomeKit
#pragma endregion
