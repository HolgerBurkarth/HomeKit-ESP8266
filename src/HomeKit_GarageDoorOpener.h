#pragma region Prolog
/*******************************************************************
$CRT 27 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>HomeKit_GarageDoorOpener.h<< 30 Sep 2024  07:03:54 - (c) proDAD

using namespace HBHomeKit::GarageDoorOpener;
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
namespace GarageDoorOpener
{
#pragma endregion

#pragma region EMotorState
enum class EMotorState
{
  Unknown, // The state is unknown
  Stopped, // The motor is stopped
  Opening, // The motor is opening the door
  Closing  // The motor is closing the door
};

#pragma endregion

#pragma region EDoorState
enum class EDoorState
{
  Unknown,          // The state is unknown
  Failed,           // The state could not be determined
  Open,             // The door is open
  Closed,           // The door is closed
  MovingOrStopped,  // The door is moving or stopped (partially open)
  Stalled,          // The door is stalled (stopped and partially open)
};

#pragma endregion

#pragma region Same usings
using pos_t = uint16_t; // Door position in cm; [1 ... 65535]
using CPosRange = CRange<pos_t>;
using COptionalBool = std::optional<bool>;
using COptionalPos = std::optional<pos_t>;
using COptionalCurrentState = std::optional<HOMEKIT_CURRENT_DOOR_STATE>;
using COptionalTargetState = std::optional<HOMEKIT_TARGET_DOOR_STATE>;
using COptionalMotorState = std::optional<EMotorState>;
using COptionalObstructionDetected = COptionalBool;

#pragma endregion

#pragma region CMessage
struct CMessage
{
  COptionalCurrentState         CurrentState;
  COptionalTargetState          TargetState;
  COptionalObstructionDetected  ObstructionDetected;
};

#pragma endregion

#pragma region CSensorData
struct CSensorData
{
  COptionalPos  DoorPosition;
  COptionalPos  RawDoorPosition;
  COptionalBool OpenSwitch;
  COptionalBool ClosedSwitch;
};

#pragma endregion

#pragma region CMotorInfo
struct CMotorInfo
{
  COptionalMotorState Current;
  COptionalMotorState LastDirection;
};

#pragma endregion

#pragma region CTravelInfo
struct CTravelInfo
{
  uint16_t TotalSecs{};   // Total travel time in seconds from open to closed position. (0) = unknown
  uint16_t OpeningSecs{}; // Start time to leave the stop position (closed). (0) = unknown
  uint16_t ClosingSecs{}; // Start time to leave the stop position (open). (0) = unknown

  bool operator == (const CTravelInfo& value) const
  {
    return TotalSecs == value.TotalSecs && OpeningSecs == value.OpeningSecs && ClosingSecs == value.ClosingSecs;
  }
  bool operator != (const CTravelInfo& value) const
  {
    return !(*this == value);
  }

  friend bool IsDisabledOrInvalid(const CTravelInfo& value)
  {
    return value.TotalSecs == 0 || value.OpeningSecs == 0 || value.ClosingSecs == 0;
  }
};

#pragma endregion

#pragma region CPositionMarkers
/* Position markers for open and closed door positions.
*/
struct CPositionMarkers
{
  struct CEPROM_Data;

  #pragma region Fields
  /* Stop positions */
  CPosRange Open{}, Closed{};

  CTravelInfo Travel{};

  #pragma endregion

  #pragma region IsDisabledOrInvalid
  friend constexpr bool IsDisabledOrInvalid(const CPositionMarkers& value)
  {
    return IsDisabledOrInvalid(value.Open) || IsDisabledOrInvalid(value.Closed);
  }

  #pragma endregion

  #pragma region Properties

  #pragma region OpenByEnlarging
  /* Checks if the door opens by enlarging the position values.
  * @return
  *   True: The door opens when the position values increase
  *   False: The door closes when the position values increase
  */
  constexpr bool OpenByEnlarging() const
  {
    // True:  Position |closed -> open|
    // False: Position |open -> closed|
    return Open.Maximum > Closed.Minimum;
  }

  #pragma endregion

  //END Properties
  #pragma endregion

  #pragma region Methods

  #pragma region IsBetweenStopPoints
  /* Checks if a position is between the fixed stop points.
  * @param pos The position to check.
  * @return True if the position is between the stop points.
  */
  bool IsBetweenStopPoints(pos_t pos) const
  {
    if(OpenByEnlarging())
      return pos < Open.Minimum && pos > Closed.Maximum; // |closed -- pos -- open|
    else
      return pos > Open.Maximum && pos < Closed.Minimum; // |open -- pos -- closed|
  }

  #pragma endregion

  #pragma region Emitter
  /*
* @return The emitter for the position markers:
  *  "OpenMinimum;OpenMaximum;ClosedMinimum;ClosedMaximum;TotalSecs:OpeningSecs;ClosingSecs"
  */
  CTextEmitter Emitter() const;

  #pragma endregion

  #pragma region Load/Save
  bool EPROM_Load();
  bool EPROM_Save() const;

  #pragma endregion

  //END Methods
  #pragma endregion




};

//END CPositionMarkers
#pragma endregion

#pragma region CEventRecorder
struct CEventRecorder
{
  #pragma region Types
  using tick_t = uint16_t; // Centiseconds; (Diver=8) [0 ...  524 sec. ]

  #pragma region Flags-Enum
  enum : uint8_t
  {
    EVT_MotorOpening  = 0x01,
    EVT_MotorClosing  = 0x02,
    EVT_MotorStopped  = 0x03,
    EVT_DoorFailed    = 0x04,
    EVT_DoorOpen      = 0x05,
    EVT_DoorClosed    = 0x06,
    EVT_DoorMovOrSt   = 0x07, // Moving or Stopped
    EVT_Obstruction   = 0x08,
    EVT_Mask          = 0x0F,

    FLG_RawPosition   = 0x80 // The position is a raw and invalid value.
  };

  #pragma endregion

  #pragma region CStdDev
  /* Standard deviation */
  struct CStdDev
  {
    float Mean;
    float Sigma;
  };

  #pragma endregion

  #pragma region CEntry
  struct CEntry
  {
    tick_t  CS;       // [cs] Centiseconds
    pos_t   Position; // [cm] Centimeters
    uint8_t Flags{};  // see EVT_...
  };

  #pragma endregion

  #pragma region CConfig
  struct CConfig
  {
    /* For TryGetStopPositionRange.
    * @param Minimum Lowest sigma value to use.
    * @param Maximum Highest sigma value to use.
    */
    CRangeFloat PositionSigmaRange{ 4.0f, 8.0f };

    /* For TryGetStopPositionRange.
    * Increase the range in both directions.
    */
    float PositionRangeMargin{ 10 };
  };

  #pragma endregion


  //END Types
  #pragma endregion

  #pragma region Fields

  CConfig Config;

  /* The list of all entries.
  */
  std::vector<CEntry> EventEntries;

  /* The list of all entries that are related to the door.
  * @note The list is a subset of EventEntries.
  * @see EVT_DoorOpen EVT_DoorClosed EVT_DoorMovOrSt
  */
  std::vector<CEntry> DoorEntries;

  /* The maximum number of entries.
  */
  size_t MaxEntries{ 60 };

  #pragma endregion

  #pragma region Methods

  #pragma region clear
  void clear()
  {
    EventEntries.clear();
    DoorEntries.clear();
  }

  #pragma endregion

  #pragma region AddEntry
  /* Add an entry to the list.
  * @param vec The list to add the entry to.
  * @param pos (optional) The position in cm.
  * @param evt (optional) The event.
  */
  void AddEntry(std::vector<CEntry>& vec, pos_t pos, uint8_t evt);

  #pragma endregion

  #pragma region push_back_pos / push_back_evt
  /* Add an position to the list.
  * @param pos The position in cm.
  * @param isRaw True if the position is a raw and invalid value.
  */
  void push_back_pos(pos_t pos, bool isRaw = false);

  /* Add an event to the list.
  */
  void push_back_evt(uint8_t evt);

  #pragma endregion

  #pragma region reserve
  void reserve(size_t count)
  {
    MaxEntries = count;
    EventEntries.reserve(count);
    DoorEntries.reserve(count);
  }
  #pragma endregion

  #pragma region EventEntriesEmitter
  /*
  * @return The emitter for the entries: "Idx;Position;Event\n" ...
  */
  CTextEmitter EventEntriesEmitter() const;

  #pragma endregion

  #pragma region PositionStdDev
  /* Calculates the position standard deviation
  */
  CStdDev PositionStdDev() const;

  #pragma endregion

  #pragma region TryGetStopPositionRange
  /* The standard deviation is used to determine the best position range for a stop position (Open or Closed).
  * @param sd The standard deviation of the position.
  * @param sigmaFactor The factor for the standard deviation.
  * @return The best position range or std::nullopt if the standard deviation is too high.
  * @note The standard deviation must be less than 8.
  */
  std::optional<CRangeInt> TryGetStopPositionRange(const CStdDev& sd, float sigmaFactor = 6) const;

  auto TryGetStopPositionRange() const { return TryGetStopPositionRange(PositionStdDev()); }

  #pragma endregion

  #pragma region TravelInfo
  /* Calculates the travel info for open <-> closed position.
    */
  CTravelInfo TravelInfo() const;

  #pragma endregion


  #pragma region MS2CS / CS2MS
  static pos_t MS2CS(uint32_t v) { return static_cast<pos_t>(v / 8); }
  static uint32_t CS2MS(pos_t v) { return v * 8; }

  #pragma endregion

  //END Methods
  #pragma endregion

};

using EventRecorder_Ptr = std::shared_ptr<CEventRecorder>;

/* Creates a shared pointer from a static object that never calls a delete.
* @example
*  CEventRecorder EventRecorder;
*  auto Ptr = MakeStaticPtr(EventRecorder)
*/
inline EventRecorder_Ptr MakeStaticPtr(CEventRecorder& pV)
{
  return EventRecorder_Ptr(&pV, [](CEventRecorder*) {});
}

//END CEventRecorder
#pragma endregion

#pragma region CArgs
/* A template class for arguments that are passed to a method.
* The class contains a value and a flag (Handled) that indicates whether
* the value has been processed.
*/
template<typename T>
struct CArgs
{
  #pragma region Fields
  T       Value{};
  uint8_t CallCount{};
  bool    Handled{}; // True: The value has been processed and should not be processed again.

  #pragma endregion

  #pragma region Construction
  CArgs() = default;
  CArgs(const T& value) : Value(value) {}
  CArgs(T&& value) noexcept : Value(std::move(value)) {}

  #pragma endregion

  #pragma region Methods
  /* Reset the value and the Handled-flag.
  */
  void reset()
  {
    Value = T{};
    CallCount = 0;
    Handled = false;
  }

  #pragma endregion

  #pragma region Operators
  operator const T& () const { return Value; }
  operator T& () { return Value; }

  /* @return True if the value has been processed.
  * @example
  *   if(!args) ... // The value has not been processed
  */
  bool operator ! () const noexcept { return !Handled; }

  CArgs& operator=(const T& value)
  {
    Value = value;
    Handled = true;
    return *this;
  }

  CArgs& operator=(T&& value) noexcept
  {
    Value = std::move(value);
    Handled = true;
    return *this;
  }

  #pragma endregion

};

/*
* @note Workaround: bool is used as a placeholder for void.
*/
using CVoidArgs = CArgs<bool>;

#pragma endregion

#pragma region ISystem
/* Get and set the HomeKit states.
* @note The value is sent to HomeKit when it is set.
*/
struct ISystem
{
  virtual ~ISystem() = default;

  virtual HOMEKIT_TARGET_DOOR_STATE GetTargetState() const = 0;

  virtual void SetTargetState(HOMEKIT_TARGET_DOOR_STATE value) = 0;

  virtual void SetCurrentState(HOMEKIT_CURRENT_DOOR_STATE value) = 0;

  virtual void SetObstructionDetected(bool value) = 0;

};

#pragma endregion

#pragma region IUnit
/* The base class for all implementations of the garage door opener.
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
  using CMessageArgs = CArgs<CMessage>;
  using CTargetStateArgs = CArgs<HOMEKIT_TARGET_DOOR_STATE>;
  using CSensorDataArgs = CArgs<CSensorData>;
  using CMotorInfoArgs = CArgs<CMotorInfo>;
  using CDoorStateArgs = CArgs<EDoorState>;
  using CPositionMarkersArgs = CArgs<CPositionMarkers>;
  using CEventRecorderArgs = CArgs<EventRecorder_Ptr>;

  struct CSetupArgs
  {
    ISystem* System{};
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
  virtual void QueryNextMessage(CMessageArgs&) {}
  virtual void OnTargetStateChanged(CTargetStateArgs&) {}
  virtual void OnTriggered(CVoidArgs&) {}
  virtual void QuerySensorData(CSensorDataArgs&) {}
  virtual void QueryDoorState(CDoorStateArgs&) {}
  virtual void ReadMotorInfo(CMotorInfoArgs&) {}
  virtual void WriteMotorInfo(CMotorInfoArgs&) {}
  virtual void ReadPositionMarkers(CPositionMarkersArgs&) {}
  virtual void WritePositionMarkers(CPositionMarkersArgs&) {}
  virtual void QueryEventRecorder(CEventRecorderArgs&) {}

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
  ISystem* System{};
  CController* Ctrl{};
  IUnit* Super{};

  void Setup(const CSetupArgs& args) override
  {
    System = args.System;
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
  ISystem* System{};

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

  #pragma region Install
  uint8_t Install(IUnit_Ptr unit)
  {
    for(uint8_t i = 0; i < Units.size(); ++i)
    {
      if(!Units[i])
      {
        Units[i] = std::move(unit);
        return i;
      }
    }
    return static_cast<uint8_t>(~0);
  }

  #pragma endregion

  #pragma region Uninstall
  void Uninstall(uint8_t slot)
  {
    if(slot < Units.size())
    {
      if(Units[slot])
      {
        Units[slot].reset();

        for(++slot; slot < Units.size(); ++slot)
          Units[slot - 1] = std::move(Units[slot]);
      }
    }
  }

  void Uninstall(IUnit_Ptr& unit)
  {
    for(uint8_t i = 0; i < Units.size(); ++i)
    {
      if(Units[i] == unit)
      {
        Uninstall(i);
        return;
      }
    }
  }

  #pragma endregion

  #pragma region SetObstructionDetected
  void SetObstructionDetected(bool value)
  {
    if(System)
    {
      System->SetObstructionDetected(value);

      CMotorInfoArgs MotorInfo;
      ReadMotorInfo(MotorInfo);
      if(!MotorInfo)
        return;


      if(MotorInfo.Value.Current.value_or(EMotorState::Stopped) != EMotorState::Stopped)
      {
        INFO("Simulate Obstruction detected. Stopping motor.");
        MotorInfo.reset();
        MotorInfo.Value.Current = EMotorState::Stopped;
        WriteMotorInfo(MotorInfo);
      }

    }
  }

  #pragma endregion


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
    System = args.System;

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
  void QueryNextMessage(CMessageArgs& args) override
  {
    InvokeMethod(&IUnit::QueryNextMessage, args);
  }
  void OnTargetStateChanged(CTargetStateArgs& args) override
  {
    InvokeMethod(&IUnit::OnTargetStateChanged, args);
  }
  void QuerySensorData(CSensorDataArgs& args) override
  {
    InvokeMethod(&IUnit::QuerySensorData, args);
  }
  void QueryDoorState(CDoorStateArgs& args) override
  {
    InvokeMethod(&IUnit::QueryDoorState, args);
  }
  void ReadMotorInfo(CMotorInfoArgs& args) override
  {
    InvokeMethod(&IUnit::ReadMotorInfo, args);
  }
  void WriteMotorInfo(CMotorInfoArgs& args) override
  {
    InvokeMethod(&IUnit::WriteMotorInfo, args);
  }
  void OnTriggered(CVoidArgs& args) override
  {
    InvokeMethod(&IUnit::OnTriggered, args);
  }
  void ReadPositionMarkers(CPositionMarkersArgs& args) override
  {
    InvokeMethod(&IUnit::ReadPositionMarkers, args);
  }
  void WritePositionMarkers(CPositionMarkersArgs& args) override
  {
    InvokeMethod(&IUnit::WritePositionMarkers, args);
  }
  void QueryEventRecorder(CEventRecorderArgs& args) override
  {
    InvokeMethod(&IUnit::QueryEventRecorder, args);
  }

  #pragma endregion

};

#pragma endregion


#pragma region CGarageDoorOpenerService
/* A HomeKit garage door open/close service
* @note CGarageDoorOpenerService::Setup() must be called in the setup() function.
* @example

CHost OpenerHost
(
    MakeControlUnit(),
    MakePositionMarkerUnit(),
    MakeMyDoorUnit(),
    MakeDoorPositionSimulateUnit(),
    MakeContinuousEventRecorderUnit()
);


CGarageDoorOpenerService Door(&OpenerHost);

--or--

CGarageDoorOpenerService Door
({
    .Name = "Door",
    .TargetStateSetter = [](homekit_characteristic_t* pC, homekit_value_t value)
    {
        if(modify_value(pC, value))
        {
            HOMEKIT_TARGET_DOOR_STATE Stat = static_value_cast<HOMEKIT_TARGET_DOOR_STATE>(value);
            ...
        }
    },
});

*/
struct CGarageDoorOpenerService : ISystem
{
  #pragma region Fields
  homekit_characteristic_t CurrentState;
  homekit_characteristic_t TargetState;
  homekit_characteristic_t ObstructionDetected;
  homekit_characteristic_t Name;

  CService<4> Service;



  #pragma endregion

  #pragma region Params_t
  struct Params_t
  {
    /* The name of the door
    */
    const char* Name{ "Door" };

    /* Free to use by user
    */
    void* UserPtr{};

    /* Optional setter function:
    *   void setter(homekit_characteristic_t*, homekit_value_t value);
    */
    void(*TargetStateSetter)(homekit_characteristic_t*, homekit_value_t) {};

    /* Optional getter function:
    *   homekit_value_t getter(const homekit_characteristic_t*);
    */
    homekit_value_t(*CurrentStateGetter)(const homekit_characteristic_t*) {};

    /* Optional getter function:
    *   homekit_value_t getter(const homekit_characteristic_t*);
    */
    homekit_value_t(*ObstructionDetectedGetter)(const homekit_characteristic_t*) {};
  };

  #pragma endregion

  #pragma region Construction
  CGarageDoorOpenerService(const CGarageDoorOpenerService&) = delete;
  CGarageDoorOpenerService(CGarageDoorOpenerService&&) = delete;

  constexpr CGarageDoorOpenerService(Params_t params) noexcept
    :
    CurrentState{ HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_DOOR_STATE("CurrentState", 0, .getter_ex = params.CurrentStateGetter, .UserPtr = params.UserPtr) },
    TargetState{ HOMEKIT_DECLARE_CHARACTERISTIC_TARGET_DOOR_STATE("TargetState", 0, .setter_ex = params.TargetStateSetter, .UserPtr = params.UserPtr) },
    ObstructionDetected{ HOMEKIT_DECLARE_CHARACTERISTIC_OBSTRUCTION_DETECTED("ObstructionDetected", false, .getter_ex = params.ObstructionDetectedGetter, .UserPtr = params.UserPtr) },
    Name{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(params.Name) },
    Service
    {
      {
        .type = HOMEKIT_SERVICE_GARAGE_DOOR_OPENER,
        .primary = true
      },

      &CurrentState,
      &TargetState,
      &ObstructionDetected,
      &Name
    }
  {
  }


  CGarageDoorOpenerService(IUnit* pCtrl, const char* name = nullptr) noexcept
    : CGarageDoorOpenerService
    ({
      .Name = name ? name : "Door",
      .UserPtr = reinterpret_cast<void*>(pCtrl),

      .TargetStateSetter = [](homekit_characteristic_t* pC, homekit_value_t value)
      {
        auto Unit = reinterpret_cast<IUnit*>(pC->UserPtr);
        if(modify_value(pC, value))
        {
          if(Unit)
          {
            HOMEKIT_TARGET_DOOR_STATE Stat = static_value_cast<HOMEKIT_TARGET_DOOR_STATE>(value);
            IUnit::CTargetStateArgs Args = Stat;
            Unit->OnTargetStateChanged(Args);
          }
        }
      },

    })
  {
  }

  #pragma endregion

  #pragma region Properties

  #pragma region GetUnit
  IUnit* GetUnit() const noexcept
  {
    return static_cast<IUnit*>(TargetState.UserPtr);
  }
  #pragma endregion

  #pragma region GetName
  const char* GetName() const noexcept
  {
    return static_value_cast<const char*>(&Name);
  }

  #pragma endregion


  //END Properties
  #pragma endregion

  #pragma region ISystem-Methods (overrides)

  #pragma region GetTargetState
  virtual HOMEKIT_TARGET_DOOR_STATE GetTargetState() const override
  {
    return static_value_cast<HOMEKIT_TARGET_DOOR_STATE>(&TargetState);
  }

  #pragma endregion

  #pragma region SetCurrentState
  virtual void SetCurrentState(HOMEKIT_CURRENT_DOOR_STATE value) override
  {
    modify_value_and_notify(&CurrentState, value);
  }
  #pragma endregion

  #pragma region SetTargetState
  virtual void SetTargetState(HOMEKIT_TARGET_DOOR_STATE value) override
  {
    /* Prevent the implicit call of Unit->OnTargetStateChanged() by setting UserPtr to null.
     * This is important because a call to OnTriggered() by OnTargetStateChanged()
     * will cause OnTargetStateChanged() to be called again.
    */
    void* UserPtr = std::exchange(TargetState.UserPtr, nullptr);
    modify_value_and_notify(&TargetState, value);
    TargetState.UserPtr = UserPtr;
  }

  #pragma endregion

  #pragma region SetObstructionDetected
  virtual void SetObstructionDetected(bool value) override
  {
    modify_value_and_notify(&ObstructionDetected, value);
  }
  #pragma endregion


  //END ISystem-Methods
  #pragma endregion

  #pragma region Methods

  #pragma region Setup
  /* Setup the garage door opener.
  * @note This method must be called in the setup() function.
  */
  void Setup()
  {
    auto Unit = GetUnit();
    if(Unit)
    {
      if(!CHomeKit::Singleton)
      {
        ERROR("CGarageDoorOpenerService::Setup: CHomeKit not yet created");
        return;
      }

      Unit->Setup
      (
        {
          .System = this,
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

  //END Methods
  #pragma endregion

  #pragma region Operators
  constexpr const homekit_service_t* operator&() const noexcept
  {
    return &Service;
  }

  #pragma endregion

};

#pragma endregion

#pragma region +Functions

#pragma region to_string
const char* to_string(EDoorState state);
const char* to_string(EMotorState state);

#pragma endregion

#pragma region Unit - Functions
/*
* @brief The control unit is responsible for controlling the motor and
* the door state. It is also responsible for sending the current
* state to HomeKit.
* @implements
*  - ReadPositionMarkers, WritePositionMarkers
*    Store and provide CPositionMarkers
*
*  - ReadMotorInfo, WriteMotorInfo
*    Store and provide CMotorInfo
*
*  - QueryDoorState
*    Determine the door state using the available sensor data.
*
*  - OnTargetStateChanged
*    Determine the door status based on the current HomeKit status.
*    Update the Motor-LastDirection and invoke OnTriggered
*    if necessary to start the physical motor.
*
*  - QueryNextMessage
*    Determine the resulting HomeKit state based on the
*    current state of the motor and door.
*
*/
IUnit_Ptr MakeControlUnit();

/*
* @brief The unit simulates the door position and a virtual motor.
* The door position is simulated by a linear movement with a
* defined speed and noise. The motor can be started by a trigger
* and stops when the door reaches the end position.
*/
IUnit_Ptr MakeDoorPositionSimulateUnit();

/*
* @brief The unit records the door position/state and motor state in
* a continuous event recorder.
* @note The unit must be installed at the end of the (CHost) chain.
*/
IUnit_Ptr MakeContinuousEventRecorderUnit();

/*
* @brief Starts the door motor by pulling the trigger pin HIGH for 100ms.
* @note Make sure that this pin is set to LOW during booting so that
*       no trigger pulse is sent when restarting.
*       For example: D2 would be possible, but not D3.
*/
IUnit_Ptr MakeTriggerDoorMotorUnit(uint16_t pin);



#pragma region MakeSR04UltrasonicSensorPositionUnit

struct CSR04UltrasonicSensorPositionParams
{
  uint16_t TriggerPin{};
  uint16_t EchoPin{};
};

/* @brief The unit reads the door position from an ultrasonic sensor.
* The sensor is mounted at the top of the door and measures the distance to the door.
* @note Sensor: HC-SR04 Ultrasonic Sensor
* @node The EchoPin must be connected to an interrupt pin. (e.g.: D8)
*/
IUnit_Ptr MakeSR04UltrasonicSensorPositionUnit(CSR04UltrasonicSensorPositionParams p);

#pragma endregion


//END Unit - Functions
#pragma endregion

#pragma region Install - Functions

/*
* @brief Adds the Start, Settings, HomeKit menu items to the controller.
*/
void AddMenuItems(CController& ctrl);

/*
* @brief Installs the variables and commands for the HomeKit GarageDoorOpener.
* @vars TARGET_STATE, CURRENT_STATE, OBSTRUCTION_DETECTED, DOOR_POSITION, DOOR_STATE, MOTOR_STATE, PRESS_BUTTON
*/
void InstallVarsAndCmds(CController& ctrl, CGarageDoorOpenerService& door, CHost& host);

/*
* @brief Installs and configures everything for CGarageDoorOpenerService.
* @note: This function must be invoked in setup().
*/
inline void InstallAndSetupGarageDoorOpener(CController& ctrl, CGarageDoorOpenerService& door, CHost& host)
{
  door.Setup();
  AddMenuItems(ctrl);
  InstallVarsAndCmds(ctrl, door, host);
}

#pragma endregion

#pragma region JavaScript - Functions
CTextEmitter EventChart_JavaScript();
CTextEmitter Command_JavaScript();

#pragma endregion

#pragma region HtmlBody - Functions
CTextEmitter MainPage_HtmlBody();
CTextEmitter ConfigPage_HtmlBody();
CTextEmitter HomeKit_HtmlBody();

#pragma endregion


//END Functions
#pragma endregion



#pragma region Epilog
} // namespace GarageDoorOpener
} // namespace HBHomeKit
#pragma endregion
