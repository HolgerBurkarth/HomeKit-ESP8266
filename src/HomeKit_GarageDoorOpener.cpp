#pragma region Prolog
/*******************************************************************
$CRT 27 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_GarageDoorOpener.cpp<< 30 Sep 2024  06:33:20 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling: Obstru
#pragma endregion
#pragma region Includes
#include <HomeKit_GarageDoorOpener.h>

namespace HBHomeKit
{
namespace GarageDoorOpener
{
#pragma endregion

#pragma region to_string
const char* to_string(EDoorState state)
{
  switch(state)
  {
    case EDoorState::Open: return "Open";
    case EDoorState::Closed: return "Closed";
    case EDoorState::MovingOrStopped: return "MovingOrStopped";
    case EDoorState::Failed: return "Failed";
    case EDoorState::Stalled: return "Stalled";
  }
  return "Unknown";
}

const char* to_string(EMotorState state)
{
  switch(state)
  {
    case EMotorState::Opening: return "Opening";
    case EMotorState::Closing: return "Closing";
    case EMotorState::Stopped: return "Stopped";
  }
  return "Unknown";
}

#pragma endregion

#pragma region +Support

#pragma region CEventRecorder - Implementation

#pragma region AddEntry
/* Add an entry to the list.
* @param vec The list to add the entry to.
* @param pos (optional) The position in cm.
* @param evt (optional) The event.
*/
void CEventRecorder::AddEntry(std::vector<CEntry>& vec, pos_t pos, uint8_t evt)
{
  /* Limit the number of entries.
  * @note The oldest entries are removed first.
  */
  if(vec.size() > MaxEntries)
    vec.erase(vec.begin(), vec.begin() + (vec.size() - MaxEntries));

  vec.push_back
  (
    {
      .CS = MS2CS(millis()),
      .Position = pos,
      .Flags = evt
    }
  );
}
#pragma endregion

#pragma region push_back_pos / push_back_evt
/* Add an position to the list.
*/
void CEventRecorder::push_back_pos(pos_t pos, bool isRaw)
{
  AddEntry(EventEntries, pos, isRaw ? FLG_RawPosition : 0);
}
/* Add an event to the list.
*/
void CEventRecorder::push_back_evt(uint8_t evt)
{
  AddEntry(EventEntries, 0, evt);

  switch(evt)
  {
    case EVT_DoorOpen:
    case EVT_DoorClosed:
    case EVT_DoorMovOrSt:

    case EVT_MotorOpening:
    case EVT_MotorClosing:
    case EVT_MotorStopped:

      AddEntry(DoorEntries, 0, evt);
      break;
  }
}

#pragma endregion

#pragma region EventEntriesEmitter
/*
* @return The emitter for the entries: "Idx;Position;Event\n" ...
*/
CTextEmitter CEventRecorder::EventEntriesEmitter() const
{
  return [this](Stream& out)
    {
      char Buffer[64];
      uint32_t PsyMS = 0;

      for(const auto& Entry : EventEntries)
      {
        snprintf_P
        (
          Buffer, sizeof(Buffer)
          , PSTR("%u;%u;%u\n")
          , PsyMS++
          , Entry.Position
          , (Entry.Flags & EVT_Mask)
        );
        out << Buffer;
      }
    };
}

#pragma endregion

#pragma region PositionStdDev
CEventRecorder::CStdDev CEventRecorder::PositionStdDev() const
{
  CStdDev S{};
  float Variance{};
  size_t Size{};

  for(const auto& E : EventEntries)
  {
    if(E.Position > 0 && !(E.Flags & FLG_RawPosition))
    {
      S.Mean += E.Position;
      ++Size;
    }
  }

  if(Size > 0)
  {
    S.Mean /= Size;

    for(const auto& E : EventEntries)
    {
      if(E.Position > 0 && !(E.Flags & FLG_RawPosition))
      {
        auto Diff = E.Position - S.Mean;
        Variance += Diff * Diff;
      }
    }

    Variance /= Size;
    S.Sigma = sqrt(Variance);
  }
  return S;
}

#pragma endregion

#pragma region TryGetStopPositionRange
std::optional<CRangeInt> CEventRecorder::TryGetStopPositionRange(const CStdDev& sd, float sigmaFactor) const
{
  if(sd.Sigma < Config.PositionSigmaRange.Maximum)
  {
    auto SD = std::max(Config.PositionSigmaRange.Minimum, sd.Sigma);
    auto Radius = SD * sigmaFactor;
    Radius += Config.PositionRangeMargin; // Add a safety margin
    //VERBOSE("Mean: %.2f, Sigma: %.2f", sd.Mean, sd.Sigma);
    return CRangeInt
    {
      .Minimum = std::max(1, static_cast<int>(sd.Mean - Radius)),
      .Maximum = std::max(1, static_cast<int>(sd.Mean + Radius))
    };
  }
  return std::nullopt;
}

#pragma endregion

#pragma region TravelInfo
/*
* @return The average travel time in seconds or 0 if the travel time could not be determined.
*/
CTravelInfo CEventRecorder::TravelInfo() const
{
  #pragma region Definitions
  constexpr int MaxEventEntries = 6;
  constexpr int MaxRangeSecs = 1000; // Maximum valid time period for a run (open-closed)

  struct CTimeArray
  {
    std::array<uint16_t, MaxEventEntries> Secs{};
    uint16_t Count{};
  };

  #pragma endregion

  #pragma region Main Variables
  CTravelInfo Info{};
  CTimeArray TotalDuration, StartOpenDuration, StartCloseDuration;

  #pragma endregion

  #pragma region Lambdas

  #pragma region AddTime
  auto AddTime = [](CTimeArray& a, uint16_t Secs)
    {
      if(a.Count < a.Secs.size())
        a.Secs[a.Count++] = Secs;
      else
      {
        for(size_t i = 1; i < a.Count; ++i)
          a.Secs[i - 1] = a.Secs[i];
        a.Secs[a.Count - 1] = Secs;
      }
    };

  #pragma endregion

  #pragma region MedianOf
  auto MedianOf = [](CTimeArray& a) -> uint16_t
    {
      if(a.Count > 0)
      {
        std::sort(a.Secs.begin(), a.Secs.begin() + a.Count);
        return a.Secs[a.Count / 2];
      }
      return 0;
    };

  #pragma endregion


  //END Lambdas
  #pragma endregion


  for(int i = 0; i < DoorEntries.size(); ++i)
  {
    /*
    * EDoorState::MovingOrStopped: Door position has left the stop range
    * EDoorState::Open: Door position has reached the open position range
    * EDoorState::Closed: Door position has reached the closed position range
    *
    * EMotorState::Stopped: Motor has stopped
    * EMotorState::Opening: Motor is opening
    * EMotorState::Closing: Motor is closing
    *
    *
    * Actions (EVTs):
    *   OPEN:  MotorOpening -> DoorMovOrSt -> MotorStopped -> DoorOpen
    *   CLOSE: MotorClosing -> DoorMovOrSt -> MotorStopped -> DoorClosed
    */

    if(i >= 4)
    {
      const auto& E0 = DoorEntries[i - 3];
      const auto& E1 = DoorEntries[i - 2];
      const auto& E2 = DoorEntries[i - 1];
      const auto& E3 = DoorEntries[i - 0];
      const auto  Evt0 = (E0.Flags & EVT_Mask);
      const auto  Evt1 = (E1.Flags & EVT_Mask);
      const auto  Evt2 = (E2.Flags & EVT_Mask);
      const auto  Evt3 = (E3.Flags & EVT_Mask);
      int Kind{};

      // check if OPEN
      if(Evt0 == EVT_MotorOpening
        && Evt1 == EVT_DoorMovOrSt
        && Evt2 == EVT_MotorStopped
        && Evt3 == EVT_DoorOpen)
      {
        Kind = 1;
      }
      // check if CLOSE
      else if(Evt0 == EVT_MotorClosing
        && Evt1 == EVT_DoorMovOrSt
        && Evt2 == EVT_MotorStopped
        && Evt3 == EVT_DoorClosed)
      {
        Kind = 2;
      }

      if(Kind != 0)
      {
        auto TotalCS = abs((int)(E3.CS - E0.CS)); // Total time in centiseconds
        auto StartCS = abs((int)(E1.CS - E0.CS)); // Time from start to movement in centiseconds

        /*
        * @note StartCS > 10:
        *    Make sure that starting and leaving the stop range are plausible.
        *    For example, when the trigger button is pressed.
        */
        if(TotalCS > 100 && TotalCS < MaxRangeSecs * 100 && StartCS > 10)
        {
          AddTime(TotalDuration, static_cast<uint16_t>(TotalCS / 100));
          AddTime(Kind == 1 ? StartOpenDuration : StartCloseDuration, static_cast<uint16_t>(StartCS / 100));
        }
      }
    }
  }

  Info.TotalSecs = MedianOf(TotalDuration);
  Info.OpeningSecs = MedianOf(StartOpenDuration);
  Info.ClosingSecs = MedianOf(StartCloseDuration);

  #if 0
  Serial.printf("TravelInfo: TotalSecs: %u  OpeningSecs: %u  ClosingSecs: %u\n"
    , Info.TotalSecs
    , Info.OpeningSecs
    , Info.ClosingSecs
  );

  #endif


  return Info;
}

#pragma endregion


//END CEventRecorder
#pragma endregion

#pragma region CPositionMarkers - Implementation

#pragma region Emitter
/*
* @return The emitter for the position markers:
*  "OpenMinimum;OpenMaximum;ClosedMinimum;ClosedMaximum;TotalSecs:OpeningSecs;ClosingSecs"
*/
CTextEmitter CPositionMarkers::Emitter() const
{
  return [Open = Open, Closed = Closed, Travel = Travel](Stream& out)
    {
      char Buffer[sizeof("65535;") * 7];
      snprintf_P(Buffer, sizeof(Buffer)
        , PSTR("%u;%u;%u;%u;%u;%u;%u")
        , Open.Minimum, Open.Maximum
        , Closed.Minimum, Closed.Maximum
        , Travel.TotalSecs
        , Travel.OpeningSecs
        , Travel.ClosingSecs
      );
      out << Buffer;

      //VERBOSE("PositionMarkers: \"%s\"", Buffer);
    };
}

#pragma endregion

#pragma region EPROM_Load / EPROM_Save
struct CPositionMarkers::CEPROM_Data
{
  CPositionMarkers Value{};
  uint8_t Reserved[32 - sizeof(CPositionMarkers)]{};
} __attribute__((packed));


bool CPositionMarkers::EPROM_Load()
{
  CEPROM_Data Data;
  if(!SimpleFileSystem::ReadFile("PMAK", Data))
    return false;
  if(IsDisabledOrInvalid(Data.Value))
  {
    Data.Value =
    {
      //CPosRange{ 10, 100 }, CPosRange{ 320, 420 },
      CPosRange{ 10, 80 }, CPosRange{ 350, 500 },
      CTravelInfo{15,4,4}
    };
  }
  *this = Data.Value;
  return true;
}

bool CPositionMarkers::EPROM_Save() const
{
  CEPROM_Data Data;
  Data.Value = *this;
  return SimpleFileSystem::WriteFile("PMAK", Data);
}

#pragma endregion

//END CPositionMarkers
#pragma endregion




//END Support
#pragma endregion

#pragma region +Unit - Objects

#pragma region CControlUnit - Implementation
class CControlUnit : public CUnitBase
{
  #pragma region Fields
  const uint32_t TickerInterval{ 1000 / 2 }; // 2 Hz

  EDoorState    mLastDoorState{ EDoorState::Unknown };
  CMessage        mLastMessage{};
  CMotorInfo    mMotorInfo{};
  CTravelInfo   mTravelInfo{};
  time_t        mLastAtMarkerTime{ 0 }; // Time when the door was last at a marker

  //int16_t mMaxTravelSecs{ 10 }; // Maximum time to travel between stops.
  int16_t mUpdateDoorStateDelayCounter{ 0 }; // Prevent flickering: MovingOrStopped <-> Open/Closed
  int16_t mCountMotorOnByNoMovementTicks{ 0 }; // Checks door is in the end position when the motor is running

  CPositionMarkers mPositionMarkers{};


public:
  #pragma endregion

  #pragma region Override Methods

  #pragma region Start
  void Start(CVoidArgs&) override
  {
    mPositionMarkers.EPROM_Load();

    Ctrl->Ticker.Start(TickerInterval, [this](auto args)
      {
        CDoorStateArgs DoorStateArgs;
        Super->QueryDoorState(DoorStateArgs);
        if(!DoorStateArgs)
        {
          WARN("CControlUnit: DoorState not available");
          return;
        }

        if(args.Calls % 4 == 0)
          UpdateTravelInfo();

        OnDoorStateChanged(DoorStateArgs);
        SendNextNessageToHomeKit();
      });

  }

  #pragma endregion

  #pragma region QueryNextMessage
  void QueryNextMessage(CMessageArgs& args) override
  {
    CMessage& Msg = args;

    switch(mLastDoorState)
    {
      case EDoorState::Open:
        Msg.CurrentState = HOMEKIT_CURRENT_DOOR_STATE_OPEN;
        Msg.TargetState = HOMEKIT_TARGET_DOOR_STATE_OPEN;
        Msg.ObstructionDetected = false;
        break;

      case EDoorState::Closed:
        Msg.CurrentState = HOMEKIT_CURRENT_DOOR_STATE_CLOSED;
        Msg.TargetState = HOMEKIT_TARGET_DOOR_STATE_CLOSE;
        Msg.ObstructionDetected = false;
        break;

      case EDoorState::Stalled:
        Msg.ObstructionDetected = true;
        break;

      case EDoorState::MovingOrStopped:
        {
          CMotorInfoArgs MotorInfo;
          Super->ReadMotorInfo(MotorInfo);
          const auto& CurrentMotorState = MotorInfo.Value.Current;
          if(CurrentMotorState)
          {
            if(CurrentMotorState == EMotorState::Opening)
              Msg.CurrentState = HOMEKIT_CURRENT_DOOR_STATE_OPENING;
            else if(CurrentMotorState == EMotorState::Closing)
              Msg.CurrentState = HOMEKIT_CURRENT_DOOR_STATE_CLOSING;
          }
        }
        break;
    }

    args.Handled = true;
  }

  #pragma endregion

  #pragma region ReadMotorInfo
  void ReadMotorInfo(CMotorInfoArgs& args) override
  {
    args = mMotorInfo;
    args.Handled = true;
  }

  #pragma endregion

  #pragma region WriteMotorInfo
  void WriteMotorInfo(CMotorInfoArgs& args) override
  {
    args.Handled = true;
    if(args.Value.LastDirection)
      mMotorInfo.LastDirection = args.Value.LastDirection;
    if(args.Value.Current)
      mMotorInfo.Current = args.Value.Current;
  }

  #pragma endregion

  #pragma region OnTargetStateChanged
  void OnTargetStateChanged(CTargetStateArgs& args) override
  {
    HOMEKIT_TARGET_DOOR_STATE State = args;
    CMotorInfoArgs MotorInfo;
    CDoorStateArgs DoorStateArgs;
    Super->QueryDoorState(DoorStateArgs);
    if(!DoorStateArgs)
    {
      WARN("CControlUnit: DoorState not available");
      return;
    }

    /* The motor must be started if a new target state is to
    * be reached and the door has not yet reached that state.
    */

    if(State == HOMEKIT_TARGET_DOOR_STATE_OPEN)
    {
      if(DoorStateArgs != EDoorState::Open) // Door is not open
        MotorInfo.Value.Current = EMotorState::Opening;
    }
    else if(State == HOMEKIT_TARGET_DOOR_STATE_CLOSE)
    {
      if(DoorStateArgs != EDoorState::Closed) // Door is not closed
        MotorInfo.Value.Current = EMotorState::Closing;
    }

    if(MotorInfo.Value.Current != EMotorState::Unknown)
    {
      MotorInfo.Value.LastDirection = MotorInfo.Value.Current;
      Super->WriteMotorInfo(MotorInfo);

      /* Trigger the door to start moving. */
      CVoidArgs VoidArgs;
      Super->OnTriggered(VoidArgs);
    }
  }

  #pragma endregion

  #pragma region OnTriggered
  void OnTriggered(CVoidArgs&) override
  {
    /* Reset the time when the door is triggered to clear EDoorState::Stalled.  */
    time(&mLastAtMarkerTime);
  }

  #pragma endregion

  #pragma region QueryDoorState
  void QueryDoorState(CDoorStateArgs& args) override
  {
    if(mUpdateDoorStateDelayCounter > 0)
    {
      --mUpdateDoorStateDelayCounter;
      args = mLastDoorState;
      return;
    }

    CSensorDataArgs SensorDataArgs;
    Super->QuerySensorData(SensorDataArgs);
    if(!SensorDataArgs)
      return;

    if(SensorDataArgs.Value.DoorPosition)
    {
      auto Position = *SensorDataArgs.Value.DoorPosition;
      CPositionMarkersArgs MarkersArgs;
      Super->ReadPositionMarkers(MarkersArgs);
      if(!MarkersArgs)
      {
        WARN("CControlUnit: DoorPositionMarkers not available");
        args = EDoorState::Unknown;
        return;
      }

      const CPositionMarkers& Markers = MarkersArgs;
      EDoorState NewState{ EDoorState::Failed };

      if(Markers.Open.Contains(Position))
        NewState = EDoorState::Open;
      else if(Markers.Closed.Contains(Position))
        NewState = EDoorState::Closed;
      else if(Markers.IsBetweenStopPoints(Position))
        NewState = EDoorState::MovingOrStopped;

      switch(NewState)
      {
        case EDoorState::Open:
        case EDoorState::Closed:
          time(&mLastAtMarkerTime); // Save the time when the door is at a marker
          mUpdateDoorStateDelayCounter = 1000 / TickerInterval; // Delay (1s) the next update to prevent flickering
          break;

        case EDoorState::MovingOrStopped:
          if(mTravelInfo.TotalSecs > 0 && difftime(time(nullptr), mLastAtMarkerTime) > mTravelInfo.TotalSecs)
            NewState = EDoorState::Stalled;
          break;
      }


      mLastDoorState = NewState; // Save the last door state
      args = NewState;
    }
  }
  #pragma endregion

  #pragma region ReadPositionMarkers
  void ReadPositionMarkers(CPositionMarkersArgs& args) override
  {
    args = mPositionMarkers;
    args.Handled = true;
  }
  #pragma endregion

  #pragma region WritePositionMarkers
  void WritePositionMarkers(CPositionMarkersArgs& args) override
  {
    args.Handled = true;
    mPositionMarkers = args;
  }

  #pragma endregion


  //END Override Methods
  #pragma endregion

  #pragma region Private Methods
private:

  #pragma region OnDoorStateChanged
  /*
  * @brief Store the new state in mLastDoorState and
  *        stop the motor when an end position is reached.
  * @param state from QueryDoorState()
  */
  void OnDoorStateChanged(EDoorState state)
  {
    const auto StopMotorTicks = 10 * 1000 / TickerInterval; // 10 sec.

    CMotorInfoArgs MotorInfo;

    if(state == EDoorState::Unknown)
    {
      WARN("CControlUnit: DoorState is unknown");
      return;
    }

    Super->ReadMotorInfo(MotorInfo);
    if(!MotorInfo)
    {
      WARN("CControlUnit: MotorInfo not available");
    }
    else
    {
      /* Stop the motor when an end position is reached. */
      switch(MotorInfo.Value.Current.value_or(EMotorState::Stopped))
      {
        case EMotorState::Opening:
          if(state == EDoorState::Open)
          {
            INFO("Door reaches open position: Stop motor");
            MotorInfo.reset();
            MotorInfo.Value.Current = EMotorState::Stopped;
            Super->WriteMotorInfo(MotorInfo);
          }
          else if(state == EDoorState::Closed)
            ++mCountMotorOnByNoMovementTicks;
          else if(state == EDoorState::Stalled)
            mCountMotorOnByNoMovementTicks = StopMotorTicks; // force stop motor
          break;

        case EMotorState::Closing:
          if(state == EDoorState::Closed)
          {
            INFO("Door reaches closed position: Stop motor");
            MotorInfo.reset();
            MotorInfo.Value.Current = EMotorState::Stopped;
            Super->WriteMotorInfo(MotorInfo);
          }
          else if(state == EDoorState::Open)
            ++mCountMotorOnByNoMovementTicks;
          else if(state == EDoorState::Stalled)
            mCountMotorOnByNoMovementTicks = StopMotorTicks; // force stop motor
          break;

        case EMotorState::Stopped:
          mCountMotorOnByNoMovementTicks = 0;
          if(state == EDoorState::MovingOrStopped)
          {
            INFO("Door is moving or stopped: Start motor");
            EMotorState LastDirection = MotorInfo.Value.LastDirection.value_or(EMotorState::Stopped);
            EMotorState NewState = EMotorState::Unknown;
            if(LastDirection != EMotorState::Opening)
              NewState = EMotorState::Opening;
            else
              NewState = EMotorState::Closing;

            MotorInfo.Value.Current = NewState;
            MotorInfo.Value.LastDirection = NewState;
            MotorInfo.Handled = false;
            Super->WriteMotorInfo(MotorInfo);
          }
          break;

      }

      if(mCountMotorOnByNoMovementTicks >= StopMotorTicks)
      {
        if(state == EDoorState::Stalled)
          INFO("The door is stalled: Stop motor");
        else
          INFO("The door is in the end position when the motor is running: Stop motor");
        mCountMotorOnByNoMovementTicks = 0;
        MotorInfo.reset();
        MotorInfo.Value.Current = EMotorState::Stopped;
        Super->WriteMotorInfo(MotorInfo);
      }

    }
  }

  #pragma endregion

  #pragma region SendNextNessageToHomeKit
  /* Queries the current status of the garage door by calling
  * the QueryNextMessage() virtual method. The determined
  * states are sent to HomeKit via ISystem.
  * @see StartUpdateTicker, QueryNextMessage
  * @note The StartUpdateTicker() method uses a continuous ticker
  *       to automatically perform the update periodically.
  */
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

    if(Msg.TargetState)
    {
      if(!mLastMessage.TargetState || *Msg.TargetState != *mLastMessage.TargetState)
      {
        System->SetTargetState(*Msg.TargetState);
        mLastMessage.TargetState = *Msg.TargetState;
      }
    }
    if(Msg.CurrentState)
    {
      if(!mLastMessage.CurrentState || *Msg.CurrentState != *mLastMessage.CurrentState)
      {
        System->SetCurrentState(*Msg.CurrentState);
        mLastMessage.CurrentState = *Msg.CurrentState;
      }
    }


    {
      auto ObstructionDetected = Msg.ObstructionDetected.value_or(false);
      if(!mLastMessage.ObstructionDetected || ObstructionDetected != *mLastMessage.ObstructionDetected)
      {
        mLastMessage.ObstructionDetected = ObstructionDetected;
        System->SetObstructionDetected(ObstructionDetected);
      }
    }
  }

  #pragma endregion

  #pragma region UpdateTravelInfo
  void UpdateTravelInfo()
  {
    CPositionMarkersArgs MarkersArgs;
    Super->ReadPositionMarkers(MarkersArgs);
    if(MarkersArgs.Handled)
    {
      if(!IsDisabledOrInvalid(MarkersArgs.Value.Travel))
      {
        /* Increasing the duration to compensate for variations. */
        MarkersArgs.Value.Travel.TotalSecs += 5;
        MarkersArgs.Value.Travel.OpeningSecs += 2;
        MarkersArgs.Value.Travel.ClosingSecs += 2;

        if(mTravelInfo != MarkersArgs.Value.Travel)
        {
          mTravelInfo = MarkersArgs.Value.Travel;

          VERBOSE("CControlUnit: TravelInfo: TotalSecs: %u  OpeningSecs: %u  ClosingSecs: %u"
            , mTravelInfo.TotalSecs
            , mTravelInfo.OpeningSecs
            , mTravelInfo.ClosingSecs
          );
        }
      }
    }

  }

  #pragma endregion


  //END Private Methods
  #pragma endregion

};

IUnit_Ptr MakeControlUnit()
{
  return std::make_shared<CControlUnit>();
}

//END CControlUnit
#pragma endregion

#pragma region CDoorPositionSimulateUnit - Implementation
struct CDoorPositionSimulateUnit : CUnitBase
{
  #pragma region Fields
  enum { SecDivider = 8 };

  int CurrentPosition{ 360 };
  int SensorNoise = 8; // cm
  int DoorSpeed = 30; // cm/s
  int StartDelay = (SecDivider * 1) / 2; // 0.5 Sec.
  int DelayCounter{};

  CPositionMarkers  mMarkers{ { 30, 80 }, { 340, 400 } };
  CMotorInfo        mMotor{};

  CMedianStack<int, 5>  mMedianStack;
  //CKalman1DFilter<float>  mKalmanFilter;

  #pragma endregion

  #pragma region Start
  virtual void Start(CVoidArgs&) override
  {
    Ctrl->Ticker.Start(1000 / SecDivider, [this]() { MoveToNextDoorPosition(); });
  }
  #pragma endregion

  #pragma region QuerySensorData
  virtual void QuerySensorData(CSensorDataArgs& args)
  {
    const pos_t MinValue = 1;
    const pos_t MaxValue = std::numeric_limits<pos_t>::max();
    int Position = CurrentPosition;

    Position += random(-SensorNoise, SensorNoise);

    {
      uint32_t R = rand() & 0xffff;
      if(R < 0x1400)
        Position = 1000;
    }

    mMedianStack.push(Position); Position = mMedianStack.pop();
    //Position = mKalmanFilter.Update(Position);

    auto Pos = static_cast<pos_t>(std::min<int>(MaxValue, std::max<int>(MinValue, Position)));

    if(Pos > 5 && Pos < 1000)
      args.Value.DoorPosition = Pos;

    args.Value.RawDoorPosition = Pos;
    args.Handled = true;
  }

  #pragma endregion

  #pragma region OnTriggered
  virtual void OnTriggered(CVoidArgs&) override
  {
    auto Last = mMotor.LastDirection.value_or(EMotorState::Stopped);
    auto Curr = mMotor.Current.value_or(EMotorState::Stopped);
    EMotorState NewState{};

    if(Curr != EMotorState::Stopped)
    {
      INFO("Simulate: Button pressed while moving: Stop virtual motor");
      NewState = EMotorState::Stopped;
    }
    else
    {
      switch(Last)
      {
        case EMotorState::Opening:
          NewState = EMotorState::Closing;
          INFO("Simulate: Button pressed: Start closing");
          break;

        case EMotorState::Closing:
          NewState = EMotorState::Opening;
          INFO("Simulate: Button pressed: Start opening");
          break;

        default:
          {
            auto State = CurrentDoorState();
            switch(State)
            {
              case EDoorState::Open:
                NewState = EMotorState::Closing;
                INFO("Simulate: First Button pressed: Start closing");
                break;

              default:
              case EDoorState::Closed:
                NewState = EMotorState::Opening;
                INFO("Simulate: First Button pressed: Start opening");
                break;
            }
          }
          break;
      }
    }

    if(NewState != EMotorState::Unknown)
    {
      DelayCounter = StartDelay;
      mMotor.Current = NewState;
      if(NewState == EMotorState::Opening || NewState == EMotorState::Closing)
        mMotor.LastDirection = NewState;
    }
  }
  #pragma endregion

  #pragma region CurrentDoorState
  EDoorState CurrentDoorState() const
  {
    /*
    * > (*2) to ensure that the range is also entered.
    */

    if(mMarkers.OpenByEnlarging()) // |closed -> open|
    {
      if(mMarkers.Open.Contains(CurrentPosition - SensorNoise * 2))
        return EDoorState::Open;
      if(mMarkers.Closed.Contains(CurrentPosition + SensorNoise * 2))
        return EDoorState::Closed;
    }
    else // |open -> closed|
    {
      if(mMarkers.Closed.Contains(CurrentPosition - SensorNoise * 2))
        return EDoorState::Closed;
      if(mMarkers.Open.Contains(CurrentPosition + SensorNoise * 2))
        return EDoorState::Open;
    }

    return EDoorState::MovingOrStopped;
  }
  #pragma endregion

  #pragma region MoveToNextDoorPosition
  void MoveToNextDoorPosition()
  {
    if(DelayCounter > 0)
    {
      --DelayCounter;
      return;
    }

    const auto DoorState = CurrentDoorState();
    const auto MotorState = mMotor.Current.value_or(EMotorState::Stopped);

    switch(MotorState)
    {
      case EMotorState::Opening:
        if(DoorState == EDoorState::Open)
        {
          INFO("Simulate: Door reaches open position: Stop virtual motor");
          mMotor.Current = EMotorState::Stopped;
        }
        else
          CurrentPosition -= DoorSpeed / SecDivider;
        break;

      case EMotorState::Closing:
        if(DoorState == EDoorState::Closed)
        {
          INFO("Simulate: Door reaches closed position: Stop virtual motor");
          mMotor.Current = EMotorState::Stopped;
        }
        else
          CurrentPosition += DoorSpeed / SecDivider;
        break;
    }
  }

  #pragma endregion

};

/*
* @brief The unit simulates the door position and a virtual motor.
* The door position is simulated by a linear movement with a
* defined speed and noise. The motor can be started by a trigger
* and stops when the door reaches the end position.
*/
IUnit_Ptr MakeDoorPositionSimulateUnit()
{
  return std::make_shared<CDoorPositionSimulateUnit>();
}

//END CPositionController
#pragma endregion

#pragma region CContinuousEventRecorderUnit - Implementation
struct CContinuousEventRecorderUnit : CUnitBase
{
  #pragma region Fields
  CEventRecorder Record;
  size_t MaxRecordSize{ 60 };
  CMessage mLastMessage;
  CSuperInvoke mSuperInvoke{};

  #pragma endregion

  #pragma region Start
  virtual void Start(CVoidArgs&) override
  {
    Record.reserve(MaxRecordSize);
  }

  #pragma endregion

  #pragma region QuerySensorData
  virtual void QuerySensorData(CSensorDataArgs& args) override
  {
    if(mSuperInvoke(Super, &IUnit::QuerySensorData, args))
    {
      if(args.Value.DoorPosition)
        Record.push_back_pos(*args.Value.DoorPosition);
      else if(args.Value.RawDoorPosition)
        Record.push_back_pos(*args.Value.RawDoorPosition, true);
    }
  }

  #pragma endregion

  #pragma region WriteMotorInfo
  virtual void WriteMotorInfo(CMotorInfoArgs& args) override
  {
    if(mSuperInvoke(Super, &IUnit::WriteMotorInfo, args))
    {
      if(args.Value.Current)
      {
        auto State = *args.Value.Current;
        uint8_t Evt = 0;
        switch(State)
        {
          case EMotorState::Opening: Evt = CEventRecorder::EVT_MotorOpening; break;
          case EMotorState::Closing: Evt = CEventRecorder::EVT_MotorClosing; break;
          case EMotorState::Stopped: Evt = CEventRecorder::EVT_MotorStopped; break;
        }
        if(Evt)
        {
          Record.push_back_evt(Evt);
        }
      }
    }
  }
  #pragma endregion

  #pragma region QueryNextMessage
  virtual void QueryNextMessage(CMessageArgs& args) override
  {
    if(mSuperInvoke(Super, &IUnit::QueryNextMessage, args))
    {
      const CMessage& Msg = args;
      uint8_t Evt = 0;

      /* If Msg.CurrentState is available and different from the last state,
      * the event is recordable.
      */
      if(Msg.CurrentState
        && mLastMessage.CurrentState.value_or(HOMEKIT_CURRENT_DOOR_STATE_OPEN) != Msg.CurrentState)
      {
        mLastMessage.CurrentState = Msg.CurrentState;
        switch(*Msg.CurrentState)
        {
          case HOMEKIT_CURRENT_DOOR_STATE_OPEN:
            Evt = CEventRecorder::EVT_DoorOpen;
            break;

          case HOMEKIT_CURRENT_DOOR_STATE_CLOSED:
            Evt = CEventRecorder::EVT_DoorClosed;
            break;

          case HOMEKIT_CURRENT_DOOR_STATE_OPENING:
          case HOMEKIT_CURRENT_DOOR_STATE_CLOSING:
            Evt = CEventRecorder::EVT_DoorMovOrSt;
            break;
        }
      }

      /* If Msg.ObstructionDetected is available and different from the last state,
      * the event is recordable.
      */
      if(Msg.ObstructionDetected
        && mLastMessage.ObstructionDetected.value_or(false) != Msg.ObstructionDetected)
      {
        mLastMessage.ObstructionDetected = Msg.ObstructionDetected;
        if(*Msg.ObstructionDetected)
          Evt = CEventRecorder::EVT_Obstruction;
      }

      if(Evt)
      {
        Record.push_back_evt(Evt);
      }

    }
  }
  #pragma endregion

  #pragma region QueryEventRecorder
  void QueryEventRecorder(CEventRecorderArgs& args) override
  {
    args = MakeStaticPtr(Record);
  }
  #pragma endregion


};

/*
* @brief The unit records the door position/state and motor state in
* a continuous event recorder. The recorded events can be queried
* via the IEventRecorder interface.
*/
IUnit_Ptr MakeContinuousEventRecorderUnit()
{
  return std::make_shared<CContinuousEventRecorderUnit>();
}

//END
#pragma endregion

#pragma region CSR04UltrasonicSensorPositionUnit

struct CSR04UltrasonicSensorPositionUnit : CUnitBase
{
  #pragma region Types
  using CDistanceStack = CMedianStack<uint16_t, 3>;

  enum class EMode : uint8_t
  {
    Idle,
    Waiting,
    Restart
  };

  #pragma endregion

  #pragma region Fields
  static constexpr uint32_t TickerInterval{ 1000 / 8 }; // 8Hz
  static constexpr uint16_t MaxValidDistance{ 1200 };   // [cm] 12m
  static constexpr uint16_t MinValidDistance{ 2 };      // [cm] 2cm
  static constexpr uint16_t DistancePerTime = 58;       // [cm/us] as 1cm = 58us

  const uint16_t TriggerPin{};
  const uint16_t EchoPin{};

  CDistanceStack  mDistanceStack;
  uint32_t        mStartUS{}; // Start time of the measurement
  EMode           mMode{};    // Current mode
  EMotorState     mLastMotorState{};
  bool            mRelax{};

  static uint32_t EchoUS;

  #pragma endregion

  #pragma region Construction
  CSR04UltrasonicSensorPositionUnit(CSR04UltrasonicSensorPositionParams p)
    : TriggerPin(p.TriggerPin), EchoPin(p.EchoPin)
  {}

  #pragma endregion

  #pragma region +Override IUnit Methods

  #pragma region Setup
  void Setup(const CSetupArgs& args) override
  {
    CUnitBase::Setup(args);

    pinMode(TriggerPin, OUTPUT);
    digitalWrite(TriggerPin, LOW);

    pinMode(EchoPin, INPUT_PULLUP);
  }

  #pragma endregion

  #pragma region Start
  void Start(CVoidArgs&) override
  {
    attachInterrupt(EchoPin, Echo_ISR, FALLING);

    Ctrl->Ticker.Start(TickerInterval, [this](auto e) { OnMeasurementTick(e); });
    TryRestartMeasurement();
  }
  #pragma endregion

  #pragma region QuerySensorData
  void QuerySensorData(CSensorDataArgs& args) override
  {
    if(!mDistanceStack.empty())
    {
      auto Distance = mDistanceStack.pop();
      args.Value.RawDoorPosition = Distance; // always set raw value
      if(Distance >= MinValidDistance && Distance <= MaxValidDistance)
        args.Value.DoorPosition = Distance; // only set valid value
      args.Handled = true;
    }
  }
  #pragma endregion

  #pragma region WriteMotorInfo
  void WriteMotorInfo(CMotorInfoArgs& args) override
  {
    EMotorState State = args.Value.Current.value_or(EMotorState::Unknown);
    if(State != EMotorState::Unknown)
    {
      if(mLastMotorState != State)
      {
        mLastMotorState = State;

        switch(State)
        {
          case EMotorState::Opening:
          case EMotorState::Closing:
            StartStress();
            break;

          case EMotorState::Stopped:
            StartRelaxed();
            break;
        }

      }
    }
  }
  #pragma endregion



  //END Override Methods
  #pragma endregion

  #pragma region +Measurement Methods

  #pragma region OnMeasurementTick
  void OnMeasurementTick(const CTicker::CEvent& e)
  {
    const uint32_t Calls = e.Calls;

    if(mRelax)
    {
      constexpr uint32_t CallInterval = 1000 / TickerInterval;
      if(Calls % CallInterval != 0)
        return; // break frequency to 1Hz
    }

    Dispatch();
  }
  #pragma endregion

  #pragma region Dispatch
  void Dispatch()
  {
    //Serial.printf("Dispatch %d\n", (int)mMode);
    switch(mMode)
    {
      case EMode::Idle:
        break;

      case EMode::Waiting:
        if(EchoUS != 0)
        {
          auto Distance = CheckMesurement();
          if(Distance != 0)
          {
            AddNewDistance(Distance);
            StartMesurement();
            mMode = EMode::Waiting;
          }
        }
        else if(mStartUS != 0)
        {
          uint32_t WaitingDurationUS = micros() - mStartUS;
          if(WaitingDurationUS > MaxValidDistance * DistancePerTime)
          {
            WARN("No echo signal received - Restart measurement");
            goto L_Restart;
          }
        }
        break;

      case EMode::Restart:
L_Restart:
        StartMesurement();
        mMode = EMode::Waiting;
        break;
    }
  }
  #pragma endregion

  #pragma region TryRestartMeasurement
  void TryRestartMeasurement()
  {
    if(mMode == EMode::Idle)
      mMode = EMode::Restart;
  }
  #pragma endregion

  #pragma region StartMesurement
  void StartMesurement()
  {
    //Serial.println("StartMesurement");
    digitalWrite(TriggerPin, LOW);
    delayMicroseconds(2);
    digitalWrite(TriggerPin, HIGH);
    delayMicroseconds(10);
    EchoUS = 0;

    noInterrupts(); // disable interrupts
    digitalWrite(TriggerPin, LOW);
    mStartUS = micros();
    interrupts(); // enable interrupts
  }
  #pragma endregion

  #pragma region CheckMesurement
  uint16_t CheckMesurement()
  {
    //Serial.println("CheckMesurement");
    uint16_t Distance = 0;
    if(EchoUS != 0 && mStartUS != 0)
    {
      uint32_t Duration = EchoUS - mStartUS;
      EchoUS = 0;
      Distance = Duration / DistancePerTime;
    }
    return Distance;
  }

  #pragma endregion

  #pragma region Echo_ISR
  static void ICACHE_RAM_ATTR Echo_ISR()
  {
    EchoUS = micros();
  }

  #pragma endregion

  //END Measurement Methods
  #pragma endregion

  #pragma region AddNewDistance
  void AddNewDistance(uint16_t distance)
  {
    //Serial.printf("AddNewDistance %d\n", distance);
    mDistanceStack.push(distance);
  }

  #pragma endregion

  #pragma region StartStress
  void StartStress()
  {
    if(mRelax != false)
    {
      mRelax = false;
      INFO("Starts ultrasonic sensor high frequency measurement");
    }
  }
  #pragma endregion

  #pragma region StartRelaxed
  void StartRelaxed()
  {
    if(mRelax != true)
    {
      mRelax = true;
      INFO("Ends ultrasonic sensor high frequency measurement");
    }
  }
  #pragma endregion

};

uint32_t CSR04UltrasonicSensorPositionUnit::EchoUS;


IUnit_Ptr MakeSR04UltrasonicSensorPositionUnit(CSR04UltrasonicSensorPositionParams p)
{
  return std::make_shared<CSR04UltrasonicSensorPositionUnit>(p);
}

//END CSR04UltrasonicSensorPositionUnit
#pragma endregion

#pragma region CTriggerDoorMotorUnit
struct CTriggerDoorMotorUnit : CUnitBase
{
  #pragma region Fields
  const uint16_t Pin;

  #pragma endregion

  #pragma region Construction
  CTriggerDoorMotorUnit(uint16_t pin)
    : Pin(pin)
  {
  }

  #pragma endregion

  #pragma region Setup
  virtual void Setup(const CSetupArgs& args) override
  {
    CUnitBase::Setup(args);
    pinMode(Pin, OUTPUT);
    digitalWrite(Pin, LOW);
  }

  #pragma endregion

  #pragma region OnTargetStateChanged
  virtual void OnTargetStateChanged(CTargetStateArgs&) override
  {
    VERBOSE("Trigger Door Motor Pin");
    digitalWrite(Pin, HIGH);
    delay(100);
    digitalWrite(Pin, LOW);
  }

  #pragma endregion


};

IUnit_Ptr MakeTriggerDoorMotorUnit(uint16_t pin)
{
  return std::make_shared<CTriggerDoorMotorUnit>(pin);
}

//END CTriggerDoorMotorUnit
#pragma endregion



//END Unit - Objects
#pragma endregion

#pragma region +Website - Elements

#pragma region EventChart_JavaScript
CTextEmitter EventChart_JavaScript()
{
  return MakeTextEmitter(F(R"(

var EventChartCanvasHD;
var OpenMarkersHD;
var ClosedMarkersHD;
var TravelsHD;

var EventChart;
var EventChartOpenMinPos=0;
var EventChartOpenMaxPos=0;
var EventChartCloseMinPos=0;
var EventChartCloseMaxPos=0;


function MakeEventChart()
{
  EventChartCanvasHD = document.getElementById('PositionCanvas');
  OpenMarkersHD = document.getElementById('OpenMarkers');
  ClosedMarkersHD = document.getElementById('ClosedMarkers');
  TravelsHD = document.getElementById('Travels');
  EventChart = new CChart(EventChartCanvasHD);
  EventChart.LeftMargin = 30;
}

function ResizeEventChart()
{
  if(EventChartCanvasHD)
    EventChartCanvasHD.width = window.innerWidth;
}


/*
 * @info: "OpenMinPos;OpenMaxPos;CloseMinPos;CloseMaxPos"
*/
function SetPositionMarkers(info)
{
  if(info == "") return;
  var parts = info.split(';');
  EventChartOpenMinPos = parseFloat(parts[0]);
  EventChartOpenMaxPos = parseFloat(parts[1]);
  EventChartCloseMinPos = parseFloat(parts[2]);
  EventChartCloseMaxPos = parseFloat(parts[3]);
  var TotalTravelSecs = parseFloat(parts[4]);
  var OpeningTravelSecs = parseFloat(parts[5]);
  var ClosingTravelSecs = parseFloat(parts[6]);

  if(OpenMarkersHD)
    OpenMarkersHD.innerHTML = EventChartOpenMinPos + Unit_cm + " ... " + EventChartOpenMaxPos + Unit_cm;
  if(ClosedMarkersHD)
    ClosedMarkersHD.innerHTML = EventChartCloseMinPos + Unit_cm+ " ... " + EventChartCloseMaxPos + Unit_cm;
  if(TravelsHD)
    TravelsHD.innerHTML = TotalTravelSecs + Unit_s + "&nbsp;&nbsp; [" + OpeningTravelSecs + Unit_s+ "|" + ClosingTravelSecs + Unit_s+ "]";
}


/*
* @entries: "MS;Pos\n"
*/
function UpdateEventChart(entries)
{
  if(!EventChart) return;
  if(entries == "") return;
  var dataPoints = [];
  var Events = [];
  var lines = entries.split('\n');
  var MinPos = 10000;
  var MaxPos = 0;

  if(lines.length < 2)
    return;

  for(var i = 0; i < lines.length; ++i)
  {
    var line = lines[i];
    if(line.length == 0)
      continue;

    var parts = line.split(';');
    var MS = parseInt(parts[0]);
    var Pos = parseFloat(parts[1]);
    var Evt = parseFloat(parts[2]);

    if(Pos > 0)
    {
      dataPoints.push([MS, Pos]);
      MinPos = Math.min(MinPos, Pos);
      MaxPos = Math.max(MaxPos, Pos);
    }
    if(Evt > 0)
    {
      Events.push([MS, Evt]);
    }
  }

  if(dataPoints.length < 2) return;



  if(EventChartOpenMinPos > 0)
  {
    MinPos = Math.min(MinPos, EventChartOpenMinPos);
    MaxPos = Math.max(MaxPos, EventChartOpenMaxPos);
    MinPos = Math.min(MinPos, EventChartCloseMinPos);
    MaxPos = Math.max(MaxPos, EventChartCloseMaxPos);
  }

  var MinMS = dataPoints[0][0];
  var MaxMS = dataPoints[dataPoints.length - 1][0];

  MinPos = Math.floor(MinPos / 100) * 100;
  MaxPos = Math.ceil(MaxPos  /  50) *  50;

  EventChart.SetVrtAxis(MinPos, MaxPos, 100);
  EventChart.SetHrzAxis(MinMS,  MaxMS,   10);

  EventChart.SetDataPoints(dataPoints);
  EventChart.Clear();
  EventChart.DrawHrzGrid();
  EventChart.DrawVrtGrid();
  EventChart.DrawVrtAxisNumbers();
  EventChart.DrawBoundaryBox(EventChartOpenMinPos, EventChartOpenMaxPos,   "#0af", 0.3);
  EventChart.DrawBoundaryBox(EventChartCloseMinPos, EventChartCloseMaxPos, "#fa0", 0.3);
  EventChart.DrawCurve();

  var Depth = 0;
  for(var i = 0; i < Events.length; ++i)
  {
    var Evt = Events[i];
    var Text ="";
    var Level = 0;
    var ColorLn;
    var ColorBg;
    switch(Evt[1])
    {
      case 0x01: Text = "Opening";      break;
      case 0x02: Text = "Closing";      break;
      case 0x03: Text = "Stopped";      break;
      case 0x04: Text = "Failed";      Level=1; break;
      case 0x05: Text = "Open";        Level=1; break;
      case 0x06: Text = "Closed";      Level=1; break;
      case 0x07: Text = "MovOrSt";     Level=1; break;
      case 0x08: Text = "Obstruction"; Level=2; break;
    }

    switch(Level)
    {
      case 0: ColorBg = "#066"; ColorLn = "#0ff"; break;
      case 1: ColorBg = "#660"; ColorLn = "#ff0"; break;
      case 2: ColorBg = "#f44"; ColorLn = "#f44"; break;
    }

    EventChart.DrawEvent(Depth % 4, Evt[0], Text, ColorLn, ColorBg);
    ++Depth;
  }
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
          ForVar('DOOR_POSITION_MARKERS', responseText => SetPositionMarkers(responseText) );

        },
        2000
    );

    setInterval
    (
      function()
        {
          ForSetVar('CONTINUOUS_POSITION', 'entries', responseText => UpdateEventChart(responseText) );

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

var DebugHD;
var TargetStateHD;
var CurrentStateHD;
var ObstructionDetectedHD;
var DoorPositionHD;
var DoorStateHD;
var MotorStateHD;
var LastMotorStateHD;
var StdevHD;
var NewTravelsHD;

var TargetStateValue;


function WriteToTargetStateUI(state)
{
  TargetStateValue = state;
  if(TargetStateHD)
    TargetStateHD.innerHTML = state;
}
function SetTargetState(state)
{
  ForSetVar('TARGET_STATE', state, responseText => WriteToTargetStateUI(responseText) );
}


function WriteToCurrentStateUI(state)
{
  if(CurrentStateHD)
    CurrentStateHD.innerHTML = state;
}
function SetCurrentState(state)
{
  ForSetVar('CURRENT_STATE', state, responseText => WriteToCurrentStateUI(responseText) );
}


function WriteToObstructionDetectedHD(state)
{
  if(ObstructionDetected)
    ObstructionDetected.innerHTML = state;
}
function SetObstructionDetected(state)
{
  ForSetVar('OBSTRUCTION_DETECTED', state, responseText => WriteToObstructionDetectedHD(responseText) );
}



function WriteToDoorPositionUI(state)
{
  if(DoorPositionHD)
    DoorPositionHD.innerHTML = state + Unit_cm;
}

function WriteToDoorStateUI(state)
{
  if(DoorStateHD)
    DoorStateHD.innerHTML = state;
}


function WriteToMotorStateUI(state)
{
  var part = state.split(';');
  if(MotorStateHD)
    MotorStateHD.innerHTML = part[0];
  if(LastMotorStateHD)
    LastMotorStateHD.innerHTML = part[1];
}


function WriteToStdevUI(state)
{
  /*
   * @info: "stdev;min;max;secs"
  */
  var part = state.split(';');
  if(StdevHD)
  {
    if(part.length == 1)
      StdevHD.innerHTML = "--- .. --- &nbsp; | &sigma; " + part[0];
    else
      StdevHD.innerHTML = part[1] + Unit_cm +" .. " + part[2] + Unit_cm+ "&nbsp; | &sigma; " + part[0];
  }
  if(NewTravelsHD)
  {
    if(part.length == 1)
      NewTravelsHD.innerHTML = "--- " + Unit_s;
    else
      NewTravelsHD.innerHTML = part[3] + Unit_s + "&nbsp;&nbsp; [" + part[4] + Unit_s + "|" + part[5] + Unit_s+ "]";
  }

  HideElement('SetOpenMarker',  (part.length == 1 || TargetStateValue != "open") );
  HideElement('SetCloseMarker', (part.length == 1 || TargetStateValue != "close") );
}

function InvokePositionMarks(state)
{
  SetVar('DOOR_POSITION_MARKERS', state);
}





function PressButton()
{
  SetVar('PRESS_BUTTON', '1');
}


window.addEventListener('load', (event) =>
{
  DebugHD = document.getElementById('Debug');
  TargetStateHD = document.getElementById('TargetState');
  CurrentStateHD = document.getElementById('CurrentState');
  ObstructionDetected = document.getElementById('ObstructionDetected');
  DoorPositionHD = document.getElementById('DoorPosition');
  DoorStateHD = document.getElementById('DoorState');
  MotorStateHD = document.getElementById('MotorState');
  LastMotorStateHD = document.getElementById('LastMotorState');
  StdevHD = document.getElementById('Stdev');
  NewTravelsHD = document.getElementById('NewTravels');

  setInterval
  (
    function()
      {
        if(TargetStateHD)
          ForVar('TARGET_STATE', responseText => WriteToTargetStateUI(responseText) );
        if(CurrentStateHD)
          ForVar('CURRENT_STATE', responseText => WriteToCurrentStateUI(responseText) );
        if(ObstructionDetected)
          ForVar('OBSTRUCTION_DETECTED', responseText => WriteToObstructionDetectedHD(responseText) );
        if(DoorPositionHD)
          ForVar('DOOR_POSITION', responseText => WriteToDoorPositionUI(responseText) );
        if(DoorStateHD)
          ForVar('DOOR_STATE', responseText => WriteToDoorStateUI(responseText) );
        if(MotorStateHD || LastMotorStateHD)
          ForVar('MOTOR_STATE', responseText => WriteToMotorStateUI(responseText) );
        if(StdevHD)
          ForSetVar('DOOR_POSITION_MARKERS', 'current-stdev', responseText => WriteToStdevUI(responseText) );

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
  <canvas id="PositionCanvas" width="360" height="200"></canvas>
</div>

<table>
<tr>
  <td>{ACTION_BUTTON:PressButton():TRIGGER}</td>
  <td>{ACTION_BUTTON:SetTargetState('open'):OPEN}</td>
  <td>{ACTION_BUTTON:SetTargetState('close'):CLOSE}</td>
</tr>
</table>
<br/>
<table class='entrytab'>
<caption>HomeKit States</caption>
<tr>
  <th>Target</th>
  <th>Current</th>
  <th>Obstruction</th>
</tr>
<tr>
  <td><div id="TargetState">...</div></td>
  <td><div id="CurrentState">...</div></td>
  <td><div id="ObstructionDetected">...</div></td>
</tr>
</table>
<br/>
<table class='entrytab'>
<caption>Internal States</caption>
<tr>
  <th>Pos.</th>
  <th>Door</th>
  <th>Motor</th>
  <th>Last</th>
</tr>
<tr>
  <td><div id="DoorPosition">...</div></td>
  <td><div id="DoorState">...</div></td>
  <td><div id="MotorState">...</div></td>
  <td><div id="LastMotorState">...</div></td>
</tr>
</table>
<br/>
<table class='entrytab'>
<caption>Markers</caption>
<tr>
  <th>Open</th>
  <th>Closed</th>
  <th>Travels</th>
</tr>
<tr>
  <td><div id="OpenMarkers">...</div></td>
  <td><div id="ClosedMarkers">...</div></td>
  <td><div id="Travels">...</div></td>
</tr>
</table>


<div id="Debug"> </div>
)"));
}

#pragma endregion

#pragma region ConfigPage_HtmlBody
CTextEmitter ConfigPage_HtmlBody()
{
  return MakeTextEmitter(F(R"(
<div>
  <canvas id="PositionCanvas" width="360" height="200"></canvas>
</div>

<br/>
<table>
<tr>
  <td>{ACTION_BUTTON:PressButton():TRIGGER}</td>
  <td>{ACTION_BUTTON:SetTargetState('open'):OPEN}</td>
  <td>{ACTION_BUTTON:SetTargetState('close'):CLOSE}</td>
</tr>
</table>
<br/>
<table class='entrytab'>
<tr>
  <th>Door State</th><td><div id="TargetState">...</div></td>
</tr>
<tr>
  <th>Current Pos.</th><td><div id="DoorPosition">...</div></td>
</tr>
<tr>
  <th>Open Range</th><td><div id="OpenMarkers">...</div></td>
</tr>
<tr>
  <th>Closed Range</th><td><div id="ClosedMarkers">...</div></td>
</tr>
<tr>
  <th>Travel Duration</th><td><div id="Travels">...</div></td>
</tr>
<tr>
  <th>New Range</th><td><div id="Stdev">...</div></td>
</tr>
<tr>
  <th>New Travel Dur.</th><td><div id="NewTravels">...</div></td>
</tr>
</table>

<br/>
<table>
<tr>
  <th>RANGES</th>
  <td>{ACTION_BUTTON:InvokePositionMarks('swap'):SWAP}</td>
  <td>{ACTION_BUTTON:SetOpenMarker:InvokePositionMarks('set-open'):SET NEW OPEN}</td>
  <td>{ACTION_BUTTON:SetCloseMarker:InvokePositionMarks('set-close'):SET NEW CLOSED}</td>
</tr>
</table>
<br/>
<table>
<tr>
  <th>EPROM</th>
  <td>{ACTION_BUTTON:SetOpenMarker:InvokePositionMarks('load'):LOAD}</td>
  <td>{ACTION_BUTTON:SetCloseMarker:InvokePositionMarks('save'):SAVE}</td>
</tr>
</table>

<div id="Debug"> </div>
)"));
}

#pragma endregion

#pragma region HomeKit_HtmlBody
CTextEmitter HomeKit_HtmlBody()
{
  return MakeTextEmitter(F(R"(
<div>
  <canvas id="PositionCanvas" width="360" height="150"></canvas>
</div>

<table class='entrytab'>
<tr>
  <th>Target</th>
  <td>
    {ACTION_BUTTON:SetTargetState('open'):OPEN}
    {ACTION_BUTTON:SetTargetState('close'):CLOSE}
  </td>
</tr>
<tr>
  <th>Obstru.</th>
  <td>
    {ACTION_BUTTON:SetObstructionDetected(true):YES}
    {ACTION_BUTTON:SetObstructionDetected(false):NO}
  </td>
</tr>
<tr>
  <th>Current</th>
  <td>
    {ACTION_BUTTON:SetCurrentState('open'):OPEN}
    {ACTION_BUTTON:SetCurrentState('close'):CLOSE}
    {ACTION_BUTTON:SetCurrentState('opening'):OPENING}
    {ACTION_BUTTON:SetCurrentState('closing'):CLOSING}
    {ACTION_BUTTON:SetCurrentState('stopped'):STOPPED}
  </td>
</tr>
</table>
<br/>
<table class='entrytab'>
<caption>HomeKit States</caption>
<tr>
  <th>Target</th>
  <th>Current</th>
  <th>Obstruction</th>
</tr>
<tr>
  <td><div id="TargetState">...</div></td>
  <td><div id="CurrentState">...</div></td>
  <td><div id="ObstructionDetected">...</div></td>
</tr>
</table>
<br/>
<table class='entrytab'>
<caption>Internal States</caption>
<tr>
  <th>Pos.</th>
  <th>Door</th>
  <th>Motor</th>
  <th>Last</th>
</tr>
<tr>
  <td><div id="DoorPosition">...</div></td>
  <td><div id="DoorState">...</div></td>
  <td><div id="MotorState">...</div></td>
  <td><div id="LastMotorState">...</div></td>
</tr>
</table>


<div id="Debug"> </div>
)"));
}

#pragma endregion

//END Website - Elements
#pragma endregion

#pragma region +Install and Setup

#pragma region AddMenuItems
/*
* @brief Adds the Start, Settings, HomeKit menu items to the controller.
*/
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
          out << EventChart_JavaScript();
          out << Command_JavaScript();
        },
        .Body = [](Stream& out)
        {
          out << MainPage_HtmlBody();
        },
      }
    )

    #pragma endregion
    #pragma region Settings
    .AddMenuItem
    (
      {
        .MenuName = "Settings",
        .URI = "/config",
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
          out << ConfigPage_HtmlBody();
        },
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


#pragma region Apply_DOOR_POSITION_MARKERS
static CTextEmitter Apply_DOOR_POSITION_MARKERS(CHost& host, const char* arguments)
{
  enum class ECmdID
  {
    None,
    SetOpenMarkers,
    SetCloseMarkers,
    CurrentStdev,
    SwapOpenClose,
    Load,
    Save

  };


  char Buffer[64];
  CEventRecorder::CStdDev PositionStdDev{};
  CTravelInfo TravelInfo{};
  std::optional<CRangeInt> NewRange;
  IUnit::CPositionMarkersArgs MarkersArgs;
  IUnit::CEventRecorderArgs EventRecorderArgs;
  ECmdID Cmd = ECmdID::None;

  #pragma region WritePositionMarkers()
  auto WritePositionMarkers = [&](const CRangeInt& range, bool asOpen)
    {
      using range_t = decltype(MarkersArgs.Value.Open);
      auto& CurOpenRange = MarkersArgs.Value.Open;
      auto& CurCloseRange = MarkersArgs.Value.Closed;
      auto& TargetRange = asOpen ? CurOpenRange : CurCloseRange;
      const auto& OtherRange = asOpen ? CurCloseRange : CurOpenRange;
      const range_t NewRange
      {
        static_cast<typename range_t::value_type>(std::max(1,range.Minimum)),
        static_cast<typename range_t::value_type>(std::max(1,range.Maximum))
      };

      if(NewRange.IntersectsWith(OtherRange))
      {
        WARN("DOOR_POSITION_MARKERS set new %s range intersects with other"
          , asOpen ? "Open" : "Close"
        );
        return;
      }

      if(TravelInfo.TotalSecs > 0)
        MarkersArgs.Value.Travel.TotalSecs = TravelInfo.TotalSecs;
      if(TravelInfo.OpeningSecs > 0)
        MarkersArgs.Value.Travel.OpeningSecs = TravelInfo.OpeningSecs;
      if(TravelInfo.ClosingSecs > 0)
        MarkersArgs.Value.Travel.ClosingSecs = TravelInfo.ClosingSecs;

      TargetRange = NewRange;
      MarkersArgs.Handled = false;

      INFO("DOOR_POSITION_MARKERS Update:  Open: %u .. %u   Closed: %u .. %u  Travel: %u %d %d"
        , MarkersArgs.Value.Open.Minimum
        , MarkersArgs.Value.Open.Maximum
        , MarkersArgs.Value.Closed.Minimum
        , MarkersArgs.Value.Closed.Maximum
        , MarkersArgs.Value.Travel.TotalSecs
        , MarkersArgs.Value.Travel.OpeningSecs
        , MarkersArgs.Value.Travel.ClosingSecs
      );
      host.WritePositionMarkers(MarkersArgs);
    };

  #pragma endregion

  #pragma region EPROM_Load
  auto EPROM_Load = [&]() -> bool
    {
      return MarkersArgs.Value.EPROM_Load();
    };

  #pragma endregion

  #pragma region EPROM_Save
  auto EPROM_Save = [&]()
    {
      return MarkersArgs.Value.EPROM_Save();
    };

  #pragma endregion


  auto Parts = Split(arguments, ':');
  if(!Parts.empty())
  {
    const auto& Arg0 = Parts[0];
    if(Arg0 == F("set-open"))
      Cmd = ECmdID::SetOpenMarkers;
    else if(Arg0 == F("set-close"))
      Cmd = ECmdID::SetCloseMarkers;
    else if(Arg0 == F("current-stdev"))
      Cmd = ECmdID::CurrentStdev;
    else if(Arg0 == F("swap"))
      Cmd = ECmdID::SwapOpenClose;
    else if(Arg0 == F("load"))
      Cmd = ECmdID::Load;
    else if(Arg0 == F("save"))
      Cmd = ECmdID::Save;

    host.ReadPositionMarkers(MarkersArgs);
    if(!MarkersArgs)
    {
      WARN("DOOR_POSITION_MARKERS: No DoorPositionMarkers available");
    }
    host.QueryEventRecorder(EventRecorderArgs);
    if(!EventRecorderArgs || !EventRecorderArgs.Value)
    {
      WARN("DOOR_POSITION_MARKERS: No EventRecorder available");
    }

    auto& Recorder = *EventRecorderArgs.Value;

    switch(Cmd)
    {
      case ECmdID::SetOpenMarkers:
      case ECmdID::SetCloseMarkers:
      case ECmdID::CurrentStdev:
        PositionStdDev = Recorder.PositionStdDev();
        NewRange = Recorder.TryGetStopPositionRange(PositionStdDev);
        TravelInfo = Recorder.TravelInfo();
        break;
    }

    switch(Cmd)
    {
      case ECmdID::CurrentStdev:
        if(NewRange)
        {
          const auto& MV = *NewRange;
          snprintf_P(Buffer, sizeof(Buffer)
            , PSTR("%.1f;%d;%d;%u;%u;%u")
            , PositionStdDev.Sigma
            , MV.Minimum
            , MV.Maximum
            , TravelInfo.TotalSecs
            , TravelInfo.OpeningSecs
            , TravelInfo.ClosingSecs
          );
        }
        else
        {
          snprintf_P(Buffer, sizeof(Buffer)
            , PSTR("%.2f")
            , PositionStdDev.Sigma
          );
        }
        return MakeTextEmitter(Buffer);

      case ECmdID::SetOpenMarkers:
        if(NewRange && MarkersArgs.Handled)
          WritePositionMarkers(*NewRange, true);
        break;

      case ECmdID::SetCloseMarkers:
        if(NewRange && MarkersArgs.Handled)
          WritePositionMarkers(*NewRange, false);
        break;

      case ECmdID::SwapOpenClose:
        if(MarkersArgs.Handled)
        {
          MarkersArgs.Handled = false;
          std::swap(MarkersArgs.Value.Open, MarkersArgs.Value.Closed);
          INFO("DOOR_POSITION_MARKERS Swap Open/Close");
          host.WritePositionMarkers(MarkersArgs);
        }
        break;

      case ECmdID::Load:
        if(EPROM_Load())
        {
          INFO("DOOR_POSITION_MARKERS Load from EPROM");
          MarkersArgs.Handled = false;
          host.WritePositionMarkers(MarkersArgs);
        }
        else
        {
          WARN("DOOR_POSITION_MARKERS Load from EPROM failed");
        }
        break;

      case ECmdID::Save:
        if(MarkersArgs.Handled)
        {
          EPROM_Save();
          INFO("DOOR_POSITION_MARKERS Save to EPROM");
        }
        else
        {
          WARN("DOOR_POSITION_MARKERS Save to EPROM failed");
        }
        break;

    }
  }
  return MakeTextEmitter();
}
#pragma endregion

#pragma region InstallVarsAndCmds
/*
* @brief Installs the variables and commands for the HomeKit GarageDoorOpener.
* @vars TARGET_STATE, CURRENT_STATE, OBSTRUCTION_DETECTED, DOOR_POSITION, DOOR_STATE, MOTOR_STATE, PRESS_BUTTON
*/
void InstallVarsAndCmds(CController& c, CGarageDoorOpenerService& door, CHost& host)
{
  c
    #pragma region TARGET_STATE
    .SetVar("TARGET_STATE", [&](auto p)
      {
        if(p.Args[0] != nullptr)
        {
          HOMEKIT_TARGET_DOOR_STATE State;
          const char* Arg0Text = static_value_cast<const char*>(*p.Args[0]);
          if(strcmp_P(Arg0Text, PSTR("open")) == 0)
            State = HOMEKIT_TARGET_DOOR_STATE_OPEN;
          else if(strcmp_P(Arg0Text, PSTR("closed")) == 0 || strcmp_P(Arg0Text, PSTR("close")) == 0)
            State = HOMEKIT_TARGET_DOOR_STATE_CLOSE;
          else
            ERROR("Unknown target state: \"%s\"", Arg0Text);

          modify_value_and_notify(&door.TargetState, State);
        }

        return [State = door.GetTargetState()](Stream& out)
          {
            if(State == HOMEKIT_TARGET_DOOR_STATE_OPEN)
              out << F("open");
            else
              out << F("close");
          };
      })

    #pragma endregion

    #pragma region CURRENT_STATE
    .SetVar("CURRENT_STATE", [&](auto p)
      {
        if(p.Args[0] != nullptr)
        {
          HOMEKIT_CURRENT_DOOR_STATE State{ HOMEKIT_CURRENT_DOOR_STATE_OPEN };
          const char* Arg0Text = static_value_cast<const char*>(*p.Args[0]);
          if(strcmp_P(Arg0Text, PSTR("open")) == 0)
            State = HOMEKIT_CURRENT_DOOR_STATE_OPEN;
          else if(strcmp_P(Arg0Text, PSTR("closed")) == 0 || strcmp_P(Arg0Text, PSTR("close")) == 0)
            State = HOMEKIT_CURRENT_DOOR_STATE_CLOSED;
          else if(strcmp_P(Arg0Text, PSTR("opening")) == 0)
            State = HOMEKIT_CURRENT_DOOR_STATE_OPENING;
          else if(strcmp_P(Arg0Text, PSTR("closing")) == 0)
            State = HOMEKIT_CURRENT_DOOR_STATE_CLOSING;
          else if(strcmp_P(Arg0Text, PSTR("stopped")) == 0)
            State = HOMEKIT_CURRENT_DOOR_STATE_STOPPED;
          else
            ERROR("Unknown current state: \"%s\"", Arg0Text);

          modify_value_and_notify(&door.CurrentState, State);
        }

        return [State = static_value_cast<HOMEKIT_CURRENT_DOOR_STATE>(&door.CurrentState)](Stream& out)
          {
            switch(State)
            {
              case HOMEKIT_CURRENT_DOOR_STATE_OPEN:
                out << F("open");
                break;

              case HOMEKIT_CURRENT_DOOR_STATE_CLOSED:
                out << F("closed");
                break;

              case HOMEKIT_CURRENT_DOOR_STATE_OPENING:
                out << F("opening");
                break;

              case HOMEKIT_CURRENT_DOOR_STATE_CLOSING:
                out << F("closing");
                break;

              case HOMEKIT_CURRENT_DOOR_STATE_STOPPED:
                out << F("stopped");
                break;
            }
          };
      })

    #pragma endregion

    #pragma region OBSTRUCTION_DETECTED
    .SetVar("OBSTRUCTION_DETECTED", [&](auto p)
      {
        if(p.Args[0] != nullptr)
        {
          /* NewState: "true", "TRUE", "1", "ON", "on"  */
          bool NewState = convert_value<bool>(*p.Args[0]);
          host.SetObstructionDetected(NewState);
          modify_value_and_notify(&door.ObstructionDetected, NewState);
        }

        /* return "true" or "false"  */
        return MakeTextEmitter(door.ObstructionDetected);
      })

    #pragma endregion

    #pragma region DOOR_POSITION
    .SetVar("DOOR_POSITION", [&](auto p) -> CTextEmitter
      {
        IUnit::CSensorDataArgs SensorDataArgs;
        host.QuerySensorData(SensorDataArgs);

        return [DoorPosition = SensorDataArgs.Value.DoorPosition](Stream& out)
          {
            if(!DoorPosition)
              out << F("undef");
            else
              out << *DoorPosition;
          };
      })

    #pragma endregion

    #pragma region DOOR_STATE
    .SetVar("DOOR_STATE", [&](auto p) -> CTextEmitter
      {
        IUnit::CDoorStateArgs Args;
        host.QueryDoorState(Args);

        return [State = Args.Value](Stream& out)
          {
            out << to_string(State);
          };
      })

    #pragma endregion

    #pragma region MOTOR_STATE
    .SetVar("MOTOR_STATE", [&](auto p) -> CTextEmitter
      {
        IUnit::CMotorInfoArgs Args;
        host.ReadMotorInfo(Args);

        return [Info = Args.Value](Stream& out)
          {
            out << to_string(Info.Current.value_or(EMotorState::Unknown));
            out << ";";
            out << to_string(Info.LastDirection.value_or(EMotorState::Unknown));
          };
      })

    #pragma endregion

    #pragma region PRESS_BUTTON
    .SetVar("PRESS_BUTTON", [&](auto p) -> CTextEmitter
      {
        if(p.Args[0] != nullptr)
        {
          INFO("Door Button pressed");
          CVoidArgs Args;
          host.OnTriggered(Args);
        }
        return MakeTextEmitter();
      })

    #pragma endregion

    #pragma region CONTINUOUS_POSITION
    .SetVar("CONTINUOUS_POSITION", [&](auto p)
      {
        IUnit::CEventRecorderArgs EventRecorderArgs;
        host.QueryEventRecorder(EventRecorderArgs);
        if(!EventRecorderArgs || !EventRecorderArgs.Value)
        {
          WARN("CONTINUOUS_POSITION: No EventRecorder available");
        }

        auto& Recorder = *EventRecorderArgs.Value;


        if(p.Args[0] != nullptr)
        {
          const char* Arg = static_value_cast<const char*>(*p.Args[0]);
          if(strcmp_P(Arg, PSTR("entries")) == 0)
            return Recorder.EventEntriesEmitter();
        }
        return MakeTextEmitter();
      })

    #pragma endregion

    #pragma region DOOR_POSITION_MARKERS
    .SetVar("DOOR_POSITION_MARKERS", [&](auto p) -> CTextEmitter
      {
        if(p.Args[0] != nullptr)
        {
          auto Arguments = static_value_cast<const char*>(*p.Args[0]);
          if(Arguments)
            return Apply_DOOR_POSITION_MARKERS(host, Arguments);
        }

        IUnit::CPositionMarkersArgs MarkersArgs;
        host.ReadPositionMarkers(MarkersArgs);
        if(!MarkersArgs)
          return MakeTextEmitter();
        else
          return MarkersArgs.Value.Emitter();
      })

    #pragma endregion


    ;
}

#pragma endregion

//END Install and Setup
#pragma endregion


#pragma region Epilog
} // namespace GarageDoorOpener
} // namespace HBHomeKit
#pragma endregion
