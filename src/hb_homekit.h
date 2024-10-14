#pragma region Prolog
/*******************************************************************
$CRT 09 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_homekit.h<< 11 Okt 2024  10:35:00 - (c) proDAD

using namespace HBHomeKit;
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling: logtab wlogin
#pragma endregion
#pragma region Includes
#pragma once

#include <Arduino.h>
#include <functional>
#include <limits>
#include <array>
#include <memory>
#include <list>
#include <vector>
#include <map>
#include <cstddef>
#include <homekit_debug.h>
#include <arduino_homekit_server.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <StreamDev.h>
#include <Ticker.h>
#include <DNSServer.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

namespace HBHomeKit
{
#pragma endregion

#pragma region Globals
#if defined(ESP8266)
extern ESP8266WebServer WebServer;
#elif defined(ESP32)
#endif


#pragma endregion

#pragma region CRange
/* Describes a range defined by a minimum and a maximum.
* The range defines all values from the minimum to
* the maximum, where the range includes the maximum value.
*/
template<typename T>
struct CRange
{
  using value_type = T;

  T Minimum{};
  T Maximum{};

  constexpr auto Length() const { return Maximum - Minimum + 1; }

  constexpr bool IntersectsWith(const CRange& other) const
  {
    return Minimum <= other.Maximum && Maximum >= other.Minimum;
  }

  constexpr bool Contains(const T& value) const
  {
    return value >= Minimum && value <= Maximum;
  }

  friend constexpr bool IsDisabledOrInvalid(const CRange& range)
  {
    return range.Minimum > range.Maximum;
  }

};

using CRangeInt = CRange<int>;
using CRangeFloat = CRange<float>;

#pragma endregion

#pragma region CMedianStack
/* A stack of values that returns the median
 * of the values contained in the stack.
 * @note The stack has a fixed size and is implemented as a ring buffer.
 * @tparam T The type of the values
 * @tparam MaxElements The maximum number of elements in the stack. Odd numbers are preferred.
*/
template<typename T, size_t MaxElements>
class CMedianStack
{
  #pragma region Types
public:
  using value_type = T;
  using container_type = std::array<T, MaxElements>;

  #pragma endregion

  #pragma region Fields
private:
  container_type Container{};
  size_t Count{};

  #pragma endregion

  #pragma region Public Methods
public:

  #pragma region empty
  /* Check if the stack is empty.
  * @return true if the stack is empty, false otherwise
  */
  bool empty() const
  {
    return Count == 0;
  }

  #pragma endregion

  #pragma region clear
  /* Clear the stack.
  */
  void clear()
  {
    Count = 0;
  }

  #pragma endregion

  #pragma region push
  /* Push a value onto the stack.
  * If the stack is full, the oldest value is removed.
  * @param value The value to push
  */
  void push(T value)
  {
    if(Count < MaxElements)
      Container[Count++] = value;
    else
    {
      for(size_t i = 1; i < MaxElements; ++i)
        Container[i - 1] = Container[i];
      Container[MaxElements - 1] = value;
    }
  }

  #pragma endregion

  #pragma region pop
  /* Get the median of the values in the stack.
  * @return The median of the values in the stack
  * @note If the stack is empty, the return value is the default value of T.
  */
  T pop() const
  {
    if(Count > 0)
    {
      if constexpr(MaxElements <= 2)
      {
        return Container[0];
      }
      else if constexpr(MaxElements == 3)
      {
        // sort 3 elements
        T Tmp[3] = { Container[0], Container[1], Container[2] };
        if(Tmp[0] > Tmp[1])
          std::swap(Tmp[0], Tmp[1]);
        if(Tmp[1] > Tmp[2])
          std::swap(Tmp[1], Tmp[2]);
        if(Tmp[0] > Tmp[1])
          std::swap(Tmp[0], Tmp[1]);
        return Tmp[1];
      }
      else
      {
        container_type Sorted = Container;
        std::sort(Sorted.begin(), Sorted.begin() + Count);
        return Sorted[Count / 2];
      }
    }
    return T{};
  }

  #pragma endregion

  //END Public Methods
  #pragma endregion

};

//END CMedianStack
#pragma endregion

#pragma region CKalman1DFilter
// kalman filter linear movement 1d
template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
struct CKalman1DFilter
{
  T R{ 5e-5 }; // Measurement variance
  T Q{ 1e-5 }; // Process variance
  T Error{ 1 };
  T Value{ std::numeric_limits<T>::min() };
  
  T Update(T measurement)
  {
    if(Value == std::numeric_limits<T>::min())
    {
      Value = measurement;
    }
    else
    {
      Error += Q;
      T Gain = Error / (Error + R);
      Gain = std::min<T>(1, std::max<T>(0, Gain));
      Value += Gain * (measurement - Value);
      Error = (1 - Gain) * Error;
    }
    return Value;

  }
};

using CKalman1DFilterF = CKalman1DFilter<float>;

#pragma endregion

#pragma region format_text

/* Format a value as text
* @param buffer The buffer to write the text to
* @param size The size of the buffer
* @param fmt The format string
* @param value The value to format
* @return The number of characters written
* @note Overload for bool, int, uint8_t, uint16_t, uint32_t, uint64_t, float, const char*
* @example format_text(buffer, sizeof(buffer), 123);
* @example
* - bool: "true" or "false"
* - int: "123"
* - uint8_t: "23 u8"
*/
template<typename T>
size_t format_text(char* buffer, size_t size, const char* fmt, const T& value);

template<typename T>
size_t format_text_P(char* buffer, size_t size, PGM_P fmt, const T& value);

template<typename T>
size_t format_text(char* buffer, size_t size, const T& value);

template<typename T, size_t N>
size_t format_text(char(&buffer)[N], const T& value);


#pragma region Implementation

template<typename T, size_t N>
size_t format_text(char(&buffer)[N], const T& value)
{
  return format_text<T>(buffer, N, value);
}

template<typename T>
size_t format_text_P(char* buffer, size_t size, PGM_P fmt, const T& value)
{
  return snprintf_P(buffer, size, fmt, value);
}

template<typename T>
size_t format_text(char* buffer, size_t size, const char* fmt, const T& value)
{
  return snprintf(buffer, size, fmt, value);
}

//END Implementation
#pragma endregion

//END format_text
#pragma endregion

#pragma region +homekit_value_t / homekit_characteristic_t - Support

#pragma region is_value_typeof
/* Check if a homekit_value_t is of a specific type
* @param value The homekit_value_t to check
* @return true if the value is of the specific type, false otherwise
*/
template<typename T>
constexpr bool is_value_typeof(const homekit_value_t& value) noexcept;

#pragma region Implementation
/* bool */
template<> constexpr bool is_value_typeof<bool>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_bool;
}

/* int */
template<> constexpr bool is_value_typeof<int>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_int;
}

/* uint8_t */
template<> constexpr bool is_value_typeof<uint8_t>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_uint8;
}

/* uint16_t */
template<> constexpr bool is_value_typeof<uint16_t>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_uint16;
}

/* uint32_t */
template<> constexpr bool is_value_typeof<uint32_t>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_uint32;
}

/* uint64_t */
template<> constexpr bool is_value_typeof<uint64_t>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_uint64;
}

/* float */
template<> constexpr bool is_value_typeof<float>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_float;
}

/* char* */
template<> constexpr bool is_value_typeof<char*>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_string;
}

/* const char* */
template<> constexpr bool is_value_typeof<const char*>(const homekit_value_t& value) noexcept
{
  return value.format == homekit_format_string;
}


//END Implementation
#pragma endregion


//END is_value_typeof
#pragma endregion

#pragma region static_value_cast / static_value_ref_cast

/* Cast a homekit_value_t to a specific type
* @param value The homekit_value_t to cast
* @return The value of the homekit_value_t as the specific type
* @example bool b = static_value_cast<bool>(value);
* @note The value is not checked for the specific type.
* @example bool z = static_value_cast<bool>(homekit_value_t{});
*/
template<typename T>
constexpr T static_value_cast(const homekit_value_t& value) noexcept;
template<typename T>
constexpr T static_value_cast(const homekit_characteristic_t* ch) noexcept;


/* Cast a homekit_value_t to a specific type reference
* @param value The homekit_value_t to cast
* @return The value of the homekit_value_t as the specific type
* @example static_value_ref_cast<bool>(value) = true
* @see modify_value<T>
* @note Only available for types that has a location in the homekit_value_t-union
*/
template<typename T>
constexpr T& static_value_ref_cast(homekit_value_t& value) noexcept;

/* Cast a specific type to a homekit_value_t
* @param value The value to cast
* @return The value as a homekit_value_t
* @example homekit_value_t value = static_value_cast(true);
*/
template<typename T>
constexpr homekit_value_t static_value_cast(T value) noexcept;


#pragma region Implementation
/* homekit_characteristic_t */
template<typename T>
constexpr T static_value_cast(const homekit_characteristic_t* ch) noexcept
{
  return static_value_cast<T>(ch->value);
}

/* bool <-> homekit_value_t */
template<> constexpr bool static_value_cast<bool>(const homekit_value_t& value) noexcept
{
  return value.bool_value;
}
template<> constexpr bool& static_value_ref_cast<bool>(homekit_value_t& value) noexcept
{
  return value.bool_value;
}
template<> constexpr homekit_value_t static_value_cast<bool>(bool value) noexcept
{
  return HOMEKIT_BOOL(value);
}

/* int <-> homekit_value_t */
template<> constexpr int static_value_cast<int>(const homekit_value_t& value) noexcept
{
  return value.int_value;
}
template<> constexpr int& static_value_ref_cast<int>(homekit_value_t& value) noexcept
{
  return value.int_value;
}
template<> constexpr homekit_value_t static_value_cast<int>(int value) noexcept
{
  return HOMEKIT_INT(value);
}


/*  uint8 <-> homekit_value_t */
template<> constexpr uint8_t static_value_cast<uint8_t>(const homekit_value_t& value) noexcept
{
  return value.uint8_value;
}
template<> constexpr uint8_t& static_value_ref_cast<uint8_t>(homekit_value_t& value) noexcept
{
  return value.uint8_value;
}
template<> constexpr homekit_value_t static_value_cast<uint8_t>(uint8_t value) noexcept
{
  return HOMEKIT_UINT8(value);
}

/*  uint16_t <-> homekit_value_t */
template<> constexpr uint16_t static_value_cast<uint16_t>(const homekit_value_t& value) noexcept
{
  return value.uint16_value;
}
template<> constexpr uint16_t& static_value_ref_cast<uint16_t>(homekit_value_t& value) noexcept
{
  return value.uint16_value;
}
template<> constexpr homekit_value_t static_value_cast<uint16_t>(uint16_t value) noexcept
{
  return HOMEKIT_UINT16(value);
}

/*  uint32_t <-> homekit_value_t */
template<> constexpr uint32_t static_value_cast<uint32_t>(const homekit_value_t& value) noexcept
{
  return value.uint32_value;
}
template<> constexpr uint32_t& static_value_ref_cast<uint32_t>(homekit_value_t& value) noexcept
{
  return value.uint32_value;
}
template<> constexpr homekit_value_t static_value_cast<uint32_t>(uint32_t value) noexcept
{
  return HOMEKIT_UINT32(value);
}

/*  uint64_t <-> homekit_value_t */
template<> constexpr uint64_t static_value_cast<uint64_t>(const homekit_value_t& value) noexcept
{
  return value.uint64_value;
}
template<> constexpr uint64_t& static_value_ref_cast<uint64_t>(homekit_value_t& value) noexcept
{
  return value.uint64_value;
}
template<> constexpr homekit_value_t static_value_cast<uint64_t>(uint64_t value) noexcept
{
  return HOMEKIT_UINT64(value);
}

/*  float <-> homekit_value_t */
template<> constexpr float static_value_cast<float>(const homekit_value_t& value) noexcept
{
  return value.float_value;
}
template<> constexpr float& static_value_ref_cast<float>(homekit_value_t& value) noexcept
{
  return value.float_value;
}
template<> constexpr homekit_value_t static_value_cast<float>(float value) noexcept
{
  return HOMEKIT_FLOAT(value);
}

/*  char* <-> homekit_value_t */
template<> constexpr char* static_value_cast<char*>(const homekit_value_t& value) noexcept
{
  return value.string_value;
}
template<> constexpr char*& static_value_ref_cast<char*>(homekit_value_t& value) noexcept
{
  return value.string_value;
}
template<> constexpr homekit_value_t static_value_cast<char*>(char* value) noexcept
{
  return HOMEKIT_STRING(value);
}

/*  const char* <-> homekit_value_t */
template<> constexpr const char* static_value_cast<const char*>(const homekit_value_t& value) noexcept
{
  return value.cstring_value;
}
template<> constexpr const char*& static_value_ref_cast<const char*>(homekit_value_t& value) noexcept
{
  return value.cstring_value;
}
template<> constexpr homekit_value_t static_value_cast<const char*>(const char* value) noexcept
{
  return HOMEKIT_CSTRING(value);
}

/* HOMEKIT_TARGET_DOOR_STATE <-> homekit_value_t */
template<> constexpr HOMEKIT_TARGET_DOOR_STATE static_value_cast<HOMEKIT_TARGET_DOOR_STATE>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_TARGET_DOOR_STATE>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_TARGET_DOOR_STATE>(HOMEKIT_TARGET_DOOR_STATE value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_CURRENT_DOOR_STATE <-> homekit_value_t */
template<> constexpr HOMEKIT_CURRENT_DOOR_STATE static_value_cast<HOMEKIT_CURRENT_DOOR_STATE>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_CURRENT_DOOR_STATE>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_CURRENT_DOOR_STATE>(HOMEKIT_CURRENT_DOOR_STATE value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_TARGET_HEATING_COOLING_STATE <-> homekit_value_t */
template<> constexpr HOMEKIT_TARGET_HEATING_COOLING_STATE static_value_cast<HOMEKIT_TARGET_HEATING_COOLING_STATE>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_TARGET_HEATING_COOLING_STATE>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_TARGET_HEATING_COOLING_STATE>(HOMEKIT_TARGET_HEATING_COOLING_STATE value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_CURRENT_HEATING_COOLING_STATE <-> homekit_value_t */
template<> constexpr HOMEKIT_CURRENT_HEATING_COOLING_STATE static_value_cast<HOMEKIT_CURRENT_HEATING_COOLING_STATE>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_CURRENT_HEATING_COOLING_STATE>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_CURRENT_HEATING_COOLING_STATE>(HOMEKIT_CURRENT_HEATING_COOLING_STATE value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_TEMPERATURE_DISPLAY_UNIT <-> homekit_value_t */
template<> constexpr HOMEKIT_TEMPERATURE_DISPLAY_UNIT static_value_cast<HOMEKIT_TEMPERATURE_DISPLAY_UNIT>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_TEMPERATURE_DISPLAY_UNIT>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_TEMPERATURE_DISPLAY_UNIT>(HOMEKIT_TEMPERATURE_DISPLAY_UNIT value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_CHARACTERISTIC_STATUS <-> homekit_value_t */
template<> constexpr HOMEKIT_CHARACTERISTIC_STATUS static_value_cast<HOMEKIT_CHARACTERISTIC_STATUS>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_CHARACTERISTIC_STATUS>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_CHARACTERISTIC_STATUS>(HOMEKIT_CHARACTERISTIC_STATUS value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_CURRENT_HEATER_COOLER_STATE <-> homekit_value_t */
template<> constexpr HOMEKIT_CURRENT_HEATER_COOLER_STATE static_value_cast<HOMEKIT_CURRENT_HEATER_COOLER_STATE>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_CURRENT_HEATER_COOLER_STATE>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_CURRENT_HEATER_COOLER_STATE>(HOMEKIT_CURRENT_HEATER_COOLER_STATE value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_TARGET_HEATER_COOLER_STATE <-> homekit_value_t */
template<> constexpr HOMEKIT_TARGET_HEATER_COOLER_STATE static_value_cast<HOMEKIT_TARGET_HEATER_COOLER_STATE>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_TARGET_HEATER_COOLER_STATE>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_TARGET_HEATER_COOLER_STATE>(HOMEKIT_TARGET_HEATER_COOLER_STATE value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_SMOKE_DETECTED <-> homekit_value_t */
template<> constexpr HOMEKIT_SMOKE_DETECTED static_value_cast<HOMEKIT_SMOKE_DETECTED>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_SMOKE_DETECTED>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_SMOKE_DETECTED>(HOMEKIT_SMOKE_DETECTED value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_CARBON_MONOXIDE_DETECTED <-> homekit_value_t */
template<> constexpr HOMEKIT_CARBON_MONOXIDE_DETECTED static_value_cast<HOMEKIT_CARBON_MONOXIDE_DETECTED>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_CARBON_MONOXIDE_DETECTED>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_CARBON_MONOXIDE_DETECTED>(HOMEKIT_CARBON_MONOXIDE_DETECTED value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}

/* HOMEKIT_CARBON_DIOXIDE_DETECTED <-> homekit_value_t */
template<> constexpr HOMEKIT_CARBON_DIOXIDE_DETECTED static_value_cast<HOMEKIT_CARBON_DIOXIDE_DETECTED>(const homekit_value_t& value) noexcept
{
  return static_cast<HOMEKIT_CARBON_DIOXIDE_DETECTED>(static_value_cast<uint8_t>(value));
}
template<> constexpr homekit_value_t static_value_cast<HOMEKIT_CARBON_DIOXIDE_DETECTED>(HOMEKIT_CARBON_DIOXIDE_DETECTED value) noexcept
{
  return static_value_cast(static_cast<uint8_t>(value));
}



//END Implementation
#pragma endregion

//END static_value_cast
#pragma endregion

#pragma region homekit_value_cast

/* Reads the homekit_value_t contained in homekit_characteristic_t directly,
* or uses a getter that may be present.
* @param ch The characteristic to cast, nullptr is tolerated
*/
homekit_value_t homekit_value_cast(const homekit_characteristic_t* ch);

/* Cast a homekit_value_t to a specific type
* @param value The homekit_value_t to cast
* @return The value of the homekit_value_t as the specific type
* @see convert_value<T>(const homekit_value_t&)
* @note Supported Types:
* - bool: true if "true", "TRUE", "1", "ON", "on"
* - float
* - int
* @example
*   homekit_value_t TextValue = static_value_cast<const char*>("ON");
*   ...
*   homekit_value_t BoolValue = homekit_value_cast<bool>(TextValue);
*/
template<typename T>
homekit_value_t homekit_value_cast(const homekit_value_t& value) noexcept;


//END homekit_value_cast
#pragma endregion

#pragma region safe_value_cast

/* Cast a homekit_value_t to a specific type
* @param value The homekit_value_t to cast
* @return The value of the homekit_value_t as the specific type.
*         If the value is not of the specific type, the default value is returned.
* @example bool b = safe_value_cast<bool>(value);
* @note The value is checked for the specific type.
*/
template<typename T>
constexpr T safe_value_cast(const homekit_value_t& value) noexcept;
template<typename T>
constexpr T safe_value_cast(const homekit_value_t* value) noexcept;

template<typename T>
constexpr T safe_value_cast(const homekit_characteristic_t* ch) noexcept;


#pragma region Implementation

template<typename T>
constexpr T safe_value_cast(const homekit_value_t& value) noexcept
{
  if(is_value_typeof<T>(value))
    return static_value_cast<T>(value);
  else
    return T{};
}

template<typename T>
constexpr T safe_value_cast(const homekit_value_t* value) noexcept
{
  if(!value) return T{};
  return safe_value_cast<T>(*value);
}

template<typename T>
constexpr T safe_value_cast(const homekit_characteristic_t* ch) noexcept
{
  return safe_value_cast<T>(static_value_cast(ch));
}

//END Implementation
#pragma endregion

//END safe_value_cast
#pragma endregion

#pragma region convert_value

/* Convert a homekit_value_t to a specific type
* @param value The value to convert
* @return The value of the homekit_value_t as the specific type
* @note Supported Types see homekit_value_cast<T>(const homekit_value_t&)
* @example
*   homekit_value_t TextValue = static_value_cast<const char*>("TRUE");
*   ...
*   bool b = convert_value<bool>(TextValue);
*/
template<typename T>
T convert_value(const homekit_value_t& value) noexcept
{
  return static_value_cast<T>(homekit_value_cast<T>(value));
}

#pragma endregion

#pragma region to_string

/* Convert a homekit_value_t to a string
* @param value The value to convert
* @param buffer The buffer to write the string to
* @param size The size of the buffer
* @see format_text
* @example
* - bool: "true" or "false"
*/
const char* to_string(const homekit_value_t& value, char* buffer, size_t size);

template<size_t N>
const char* to_string(const homekit_value_t& value, char(&buffer)[N])
{
  return to_string(value, buffer, N);
}

/* Convert a homekit_value_t to a string
* @param value The value to convert
* @return The string representation of the value
* @see format_text
*/
String to_string(const homekit_value_t& value);
inline String to_string(const homekit_characteristic_t& ch) { return to_string(ch.value); }

/* Special: Convert a homekit_characteristic_t to a string
* @param ch The characteristic to convert
* @return The string representation of the characteristic
* @see format_text
* @example "\"Position\" = 10"
*/
String to_string_debug(const homekit_characteristic_t* ch);

//END to_string
#pragma endregion

#pragma region modify_value / modify_value_and_notify

/* Change the value of a homekit_value_t
* @param value The homekit_value_t to change
* @param newValue The new value
* @return true if the value has changed, false otherwise
* @example modify_value(HomeKitValue, true);
* @see static_value_cast<T>
* @note All types that can be converted to a homekit_value_t using static_value_cast<>() can be used as NewValue.
* @example
*  - modify_value(Ch, homekit_value_t{});
*  - modify_value(Ch, true);
*  - modify_value(Ch, HOMEKIT_CURRENT_DOOR_STATE{});
*/
template<typename T>
constexpr bool modify_value(homekit_value_t& value, T newValue) noexcept;
bool modify_value(homekit_value_t& value, const homekit_value_t& newValue);

/* Change the value of a homekit_characteristic_t
* @param ch The characteristic to change
* @param newValue The new value
* @return true if the value has changed, false otherwise
* @see static_value_cast<T>, modify_value_and_notify
* @note All types that can be converted to a homekit_value_t using static_value_cast<>() can be used as NewValue.
* @example
*  - modify_value(Ch, homekit_value_t{});
*  - modify_value(Ch, true);
*  - modify_value(Ch, HOMEKIT_CURRENT_DOOR_STATE{});
* @note ch is const to support: (This writes a changed value to the log)
*    .TemperatureGetter = [](const homekit_characteristic_t* pC) -> homekit_value_t
*    {
*      modify_value(pC, SensorData.Temperature());
*      return pC->value;
*    },
*
*/
template<typename T>
bool modify_value(const homekit_characteristic_t* ch, T newValue) noexcept;
bool modify_value(const homekit_characteristic_t* ch, const homekit_value_t& newValue);

/* Change the value of a homekit_characteristic_t
* If the value changes, HomeKit clients are notified.
* @param ch The characteristic to change
* @param newValue The new value
* @return true if the value has changed, false otherwise
* @see static_value_cast<T>, modify_value, homekit_characteristic_notify
* @note All types that can be converted to a homekit_value_t using static_value_cast<>() can be used as NewValue.
* @example
*  - modify_value_and_notify(Ch, homekit_value_t{});
*  - modify_value_and_notify(Ch, true);
*  - modify_value_and_notify(Ch, HOMEKIT_CURRENT_DOOR_STATE{});
*/
template<typename T>
bool modify_value_and_notify(homekit_characteristic_t* ch, T newValue) noexcept;
bool modify_value_and_notify(homekit_characteristic_t* ch, const homekit_value_t& newValue);


#pragma region Implementation

template<typename T>
constexpr bool modify_value(homekit_value_t& value, T newValue) noexcept
{
  return modify_value
  (
    value,
    static_cast<const homekit_value_t&>(static_value_cast(newValue))
  );
}

template<typename T>
bool modify_value(const homekit_characteristic_t* ch, T newValue) noexcept
{
  return modify_value
  (
    ch,
    static_cast<const homekit_value_t&>(static_value_cast(newValue))
  );
}

template<typename T>
bool modify_value_and_notify(homekit_characteristic_t* ch, T newValue) noexcept
{
  return modify_value_and_notify
  (
    ch,
    static_cast<const homekit_value_t&>(static_value_cast(newValue))
  );
}

#pragma endregion

//END modify_value
#pragma endregion


//END homekit_value - Support
#pragma endregion

#pragma region Invoker
/* A parameter for a function that can be called with a parameter.
*/
struct CInvokerParam
{
  enum { MaxArgCount = 1 };

  /* Prepare for possible future enhancements.*/
  std::array<const homekit_value_t*, MaxArgCount> Args{};

  constexpr CInvokerParam() = default;
  constexpr CInvokerParam(const homekit_value_t* arg) : Args{ arg } {}
};

/*
* @brief CInvoker is a function that can be called with a parameter.
* @see CController::InstallCmd, CController::SetVar
*/
using CInvoker = std::function<void(CInvokerParam)>;

#pragma endregion

#pragma region CDateTime / CShortTime / minute16_t

struct CDateTime
{
  using short_t = uint16_t;

  static time_t BaseTime();
};

template<uint32_t Diver>
struct CShortTime
{
  static constexpr uint32_t Divider = Diver;

  uint16_t mSTime{};

  CShortTime() = default;
  CShortTime(time_t now) : mSTime(Time2Sort(now)) {}

  static CShortTime Now() { return CShortTime{ time(nullptr) }; }

  void SetNow() { mSTime = Time2Sort(time(nullptr)); }

  CShortTime& operator=(const CShortTime& v) { mSTime = v.mSTime; return *this; }

  operator time_t() const { return Short2Time(mSTime); }


  static uint16_t Time2Sort(time_t now)
  {
    return static_cast<uint16_t>((now - CDateTime::BaseTime()) / Divider);
  }

  static time_t Short2Time(uint16_t st)
  {
    return CDateTime::BaseTime() + st * Divider;
  }

};


using minute16_t = CShortTime<60>; // 16bit minutes relative to device start

#pragma endregion

#pragma region CTicker / CTickerSlots

#pragma region CTickerSlot
/* A symbol for a ticker slot.
* This makes it easier to deal with invalid values.
*/
class CTickerSlot
{
  static const uint8_t InvalidSlot = (uint8_t)~0;

  uint8_t mSlot{ InvalidSlot };

  constexpr CTickerSlot(uint8_t slot) : mSlot(slot) {}
public:
  constexpr CTickerSlot() {} // forbid implicit setting any slot

  /* Create a new slot.
  * @param slot The slot number
  */
  constexpr static CTickerSlot New(uint8_t slot) { return CTickerSlot(slot); }


  constexpr uint8_t Value() const { return mSlot; }

  void reset() { mSlot = InvalidSlot; }

  /* Check if the slot is valid.
  */
  constexpr operator bool() const { return mSlot != InvalidSlot; }


  constexpr bool operator==(const CTickerSlot& other) const { return mSlot == other.mSlot; }
  constexpr bool operator!=(const CTickerSlot& other) const { return mSlot != other.mSlot; }

  constexpr friend bool IsInvalidOrDisabled(const CTickerSlot& slot) { return !slot; }

};

using ticker_slot = CTickerSlot;
#pragma endregion

#pragma region CInterruptForbidder
/* Forbid interrupts in the constructor and allow them in the destructor.
* @note This is useful for critical sections.
*/
struct CInterruptForbidder
{
  inline CInterruptForbidder() noexcept { noInterrupts(); }
  inline ~CInterruptForbidder() noexcept { interrupts(); }
};

#pragma endregion

#pragma region CTicker
/* A timer that calls a function at regular intervals.
*/
struct CTicker
{
  #pragma region Types
  struct CEvent
  {
    uint32_t Calls{};
  };

  using OnTimerFunc = std::function<void(CEvent)>;

  #pragma endregion

  #pragma region Fields
  uint32_t mInterval{};
  uint32_t mLastTick{};
  uint32_t mCalls{};
  OnTimerFunc mOnTimer{};

  #pragma endregion

  #pragma region Construction
  CTicker() = default;
  CTicker(CTicker&&) = default;
  CTicker(const CTicker&) = delete;
  CTicker& operator=(const CTicker&) = delete;
  CTicker& operator=(CTicker&&) = default;


  CTicker(uint32_t interval, OnTimerFunc&& func)
    :
    mInterval(interval),
    mLastTick(millis()),
    mOnTimer(std::move(func))
  {
  }
  CTicker(uint32_t interval, std::function<void()>&& func)
    :
    CTicker(interval, [func](CEvent) { func(); })
  {
  }

  #pragma endregion

  #pragma region Operators

  /* Check if the timer is active and callable.
    */
  operator bool() const
  {
    return !!mOnTimer;
  }

  /* Call the timer function.
  * @param ms The current time in milliseconds.
  * @see OnTick
    */
  void operator ()(uint32_t ms)
  {
    OnTick(ms);
  }

  #pragma endregion

  #pragma region Methods

  #pragma region OnTick
  /* Call the timer function.
  * @param ms The current time in milliseconds.
  * @note The function is called only if the interval has elapsed.
  */
  void OnTick(uint32_t ms)
  {
    if(TimeOver(ms))
      Invoke(ms);
  }

  #pragma endregion

  #pragma region TimeOver
  /* Check if the interval has elapsed.
  */
  bool TimeOver(uint32_t ms) const
  {
    return ms - mLastTick >= mInterval;
  }

  #pragma endregion

  #pragma region Invoke
  /* Call the timer function.
  * @note Be sure mOnTimer is not nullptr.
  */
  void Invoke(uint32_t ms)
  {
    if(mOnTimer)
    {
      mLastTick = ms;
      mOnTimer(CEvent
        {
            .Calls = ++mCalls
        });
    }
  }

  #pragma endregion


  //END Methods
  #pragma endregion

};

#pragma endregion

#pragma region CTickerSlots
/* A collection of timers.
* @tparam Slots The number of slots (maximum = 254, see CTickerSlot)
* @tparam ContinuousEvents If true, the timer functions are called continuously.
* @tparam Locker The type of the locker to protect the slots.
*/
template<size_t Slots, bool ContinuousEvents = true, typename Locker = decltype(void()) >
class CTickerSlots
{
  #pragma region Fields
  static constexpr uint32_t InvalideShortInterval{ std::numeric_limits<uint32_t>::max() };

  std::array<CTicker, Slots> mSlots;
  mutable uint32_t mShortInterval{ InvalideShortInterval };

  #pragma endregion

  #pragma region Construction
public:
  CTickerSlots() = default;
  CTickerSlots(CTickerSlots&&) = default;
  CTickerSlots(const CTickerSlots&) = delete;
  CTickerSlots& operator=(const CTickerSlots&) = delete;
  CTickerSlots& operator=(CTickerSlots&&) = default;

  #pragma endregion

  #pragma region Methods

  #pragma region Start
  /* Start a timer in a slot.
  * @param slot
  * @param interval The interval in milliseconds
  * @param func The function to call
  * @return The slot index or (~0) if no slot is available.
  * @note If a slot is already in use, the old timer is stopped and the new one is started.
  */
  ticker_slot Start(ticker_slot slot, uint32_t interval, CTicker::OnTimerFunc&& func)
  {
    if(slot.Value() >= Slots) return {};
    Locker();
    mSlots[slot.Value()] = CTicker(interval, std::move(func));
    Touch();
    return slot;
  }

  uint32_t Start(ticker_slot slot, uint32_t interval, std::function<void()>&& func)
  {
    if(slot.Value() >= Slots) return {};
    Locker();
    mSlots[slot.Value()] = CTicker(interval, std::move(func));
    Touch();
    return slot;
  }

  ticker_slot Start(uint32_t interval, CTicker::OnTimerFunc&& func)
  {
    uint8_t i = 0;
    Locker();
    for(auto& Slot : mSlots)
    {
      if(!Slot)
      {
        Touch();
        Slot = CTicker(interval, std::move(func));
        return ticker_slot::New(i);
      }
      ++i;
    }
    return {};
  }

  uint32_t Start(uint32_t interval, std::function<void()>&& func)
  {
    uint8_t i = 0;
    Locker();
    for(auto& Slot : mSlots)
    {
      if(!Slot)
      {
        Touch();
        Slot = CTicker(interval, std::move(func));
        return ticker_slot::New(i);
      }
      ++i;
    }
    return {};
  }

  #pragma endregion

  #pragma region Stop
  /* Stop a timer in a slot.
  * @param slot
  * @note If the slot is not in use, nothing happens.
    */
  void Stop(ticker_slot slot)
  {
    if(slot.Value() >= Slots) return;
    Locker();
    mSlots[slot.Value()] = CTicker{};
    Touch();
  }

  #pragma endregion

  #pragma region OnTick
  /* Call the timer functions.
  * @param ms The current time in milliseconds.
  * @note The functions are called only if the interval has elapsed.
  */
  void OnTick(uint32_t ms)
  {
    for(auto& Slot : mSlots)
    {
      Locker();
      if(Slot && Slot.TimeOver(ms))
      {
        if(ContinuousEvents)
          Slot.Invoke(ms);
        else
        {
          //VERBOSE("CTickerSlots::OnTick: ContinuousEvents %u %u | %u", ms, Slot.mLastTick, Slot.mInterval);
          CTicker Temp = std::move(Slot);
          Slot = CTicker{};
          Temp.Invoke(ms);
        }
      }
    }
  }

  #pragma endregion

  #pragma region GetShortInterval
  uint32_t GetShortInterval() const
  {
    if(mShortInterval == InvalideShortInterval)
    {
      uint32_t ShortInterval = std::numeric_limits<uint32_t>::max();
      for(const auto& Slot : mSlots)
      {
        if(Slot)
          ShortInterval = std::min(ShortInterval, Slot.mInterval);
      }
      mShortInterval = ShortInterval;
    }
    return mShortInterval;
    
  }

  #pragma endregion

  #pragma region Touch
private:
  void Touch()
  {
    mShortInterval = InvalideShortInterval; // needs recalculation
  }
  #pragma endregion


  //END Methods
  #pragma endregion

};



/* A collection of timers that are interrupt safe and can be used in an interrupt context.
* @note The timer functions are called continuously.
*/
template<size_t Slots, bool ContinuousEvents = true>
using CTickerSlots_InterruptSafe = CTickerSlots<Slots, ContinuousEvents = true, decltype(CInterruptForbidder()) >;

/* A collection of timers that are interrupt safe and can be used in an interrupt context.
* @note The timer functions are called only once.
*/
template<size_t Slots>
using COneShotTickerSlots_InterruptSafe = CTickerSlots_InterruptSafe<Slots, false>;

/* A collection of timers that are not interrupt safe.
* @note The timer functions are called only once.
*/
template<size_t Slots>
using COneShotTickerSlots = CTickerSlots<Slots, false>;

#pragma endregion


//END CTicker
#pragma endregion

#pragma region CTextEmitter / MakeTextEmitter /Stream
/*
* @brief
* Text blocks of more than 10 KB can occur, especially when building the html page,
* which consists of CSS, JavaScript, and html code.
* Data streams are used to avoid memory bottlenecks.
* The @ref CTextEmitter type provides texts or strings that are read via a data stream.
*
* @example
        CTextEmitter MyBody()
        {
            return [](Stream& p) { p << "Hello World"; };
        }

--or--

        CTextEmitter MyCSS()
        {
            return MakeTextEmitter(F("b { color: red; }"));
        }
*/
using CTextEmitter = std::function<void(Stream&)>;

#pragma region operator<<(Stream&, ...)
/* Allows CTextEmitter to be used with the << operator,
* as already possible with (const char*) and other data.
* @func The CTextEmitter to stream. If nullptr, nothing is streamed.
* @example
*  extern CTextEmitter HtmlBody();
*  out << HtmlBody();
*/
inline Stream& operator<<(Stream& p, CTextEmitter func)
{
  if(func)
    func(p);
  return p;
}

/* Current, 2024-09-18, add missing overload for string.
*   @see StreamDev.h
* @example
*  extern const String& HtmlBody;
*  out << HtmlBody;
*/
inline Stream& operator << (Stream& out, const String& string)
{
  out.write(string.c_str(), string.length());
  return out;
}
inline Stream& operator << (Stream& out, float value)
{
  char buffer[32];
  out << dtostrf(value, 0, 2, buffer);
  return out;
}

#pragma endregion

#pragma region MakeTextEmitter
/* Create a CTextEmitter from a static text.
* @param text The text to stream
* @return The CTextEmitter
* @example CTextEmitter HtmlBody = MakeTextEmitter(F("Hello World"));
*/
inline CTextEmitter MakeTextEmitter(const __FlashStringHelper* text)
{
  return [text](Stream& p) { p << text; };
}
inline CTextEmitter MakeTextEmitter(const String& text)
{
  return [text](Stream& p)
    { 
      //WARNING: Stream<<"" causes problems! : 2024-10-02 Core 3.1.2
      if(!text.isEmpty()) 
        p << text; 
    };
}
inline CTextEmitter MakeTextEmitter(String&& text)
{
  return [text = std::move(text)](Stream& p)
    {
      //WARNING: Stream<<"" causes problems! : 2024-10-02 Core 3.1.2
      if(!text.isEmpty())
        p << text;
    };
}
inline CTextEmitter MakeTextEmitter(const char* pText)
{
  return MakeTextEmitter(String(pText));
}

inline CTextEmitter MakeTextEmitter()
{
  return [](Stream&) {};
}

inline CTextEmitter MakeTextEmitter(bool v)
{
  return [v](Stream& out)
    {
      out << (v ? "true" : "false");
    };
}

inline CTextEmitter MakeTextEmitter(int32_t v)
{
  return [v](Stream& out)
    {
      out << v;
    };
}
inline CTextEmitter MakeTextEmitter(uint32_t v)
{
  return [v](Stream& out)
    {
      out << v;
    };
}
inline CTextEmitter MakeTextEmitter(float v)
{
  return [v](Stream& out)
    {
      out << v;
    };
}


/* A TextEmitter is created by converting the argument into a string using to_string(),
* followed by the return as a TextEmitter. It is crucial that a check is made at
* compile time to see whether a suitable to_string() exists.
* @param value The value to convert to a string
* @return The TextEmitter
* @example CTextEmitter Text = MakeTextEmitter(&Switch.State);
*/
template<typename T, typename = decltype(to_string(std::declval<T>()))>
CTextEmitter MakeTextEmitter(const T& value)
{
  return [Text = to_string(value)](Stream& p)
  {
    //WARNING: Stream<<"" causes problems! : 2024-10-02 Core 3.1.2
    if(!Text.isEmpty())
      p << Text;
  };
}

#pragma endregion

#pragma region to_string
/* Convert a CTextEmitter to a string
*/
inline String to_string(const CTextEmitter& text)
{
  StreamString Stream;
  if(text)
    text(Stream);
  return static_cast<String&&>(Stream);
}

#pragma endregion


//END CTextEmitter
#pragma endregion

#pragma region +Configuration Descriptions - CService / CAccessory / CConfig

#pragma region CService
/* A HomeKit service with a fixed number of characteristics
* @tparam Characteristic_Count The number of characteristics in the service
*/
template<int Characteristic_Count>
struct CService
{
  #pragma region Fields
  const homekit_characteristic_t* Characteristics[Characteristic_Count + 1]{};
  homekit_service_t Service;

  #pragma endregion

  #pragma region Construction

  #pragma region Specials
  CService(const CService&) = delete;
  CService(CService&&) = delete;

  /* Create a HomeKit service with a fixed number of characteristics
  * @param service The service definition
  * @param characteristics The characteristics of the service
  * @example CService<2> service{ { .type = HOMEKIT_SERVICE_SWITCH, .primary = true }, { &Characteristic1, &Characteristic2 } };
  */
private:
  constexpr CService
  (
    const homekit_service_t& service,
    std::initializer_list<const homekit_characteristic_t*> characteristics
  ) noexcept
    :
    Service
    {
      .id = service.id,
      .type = service.type,
      .hidden = service.hidden,
      .primary = service.primary,
      .characteristics{ Characteristics }
    }
  {
    // TODO: Check - Currently not supported by GCC
    //static_assert(Characteristic_Count == characteristics.size(), "Characteristic_Count != characteristics.size()");

    int i = 0;
    for(auto& c : characteristics)
    {
      if(c)
        Characteristics[i++] = c;
    }
  }

public:
  #pragma endregion

  #pragma region CService<2>(...)
  /* Create a HomeKit service with a fixed number of characteristics
  * @param service The service definition
  * @param c0, c1, ... The characteristics of the service
  */
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 2, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1
  ) noexcept
    :
    CService{ service, { c0, c1 } }
  {}
  #pragma endregion

  #pragma region CService<3>(...)
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 3, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1,
    const homekit_characteristic_t* c2
  ) noexcept
    :
    CService{ service, { c0, c1, c2 } }
  {}
  #pragma endregion

  #pragma region CService<4>(...)
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 4, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1,
    const homekit_characteristic_t* c2,
    const homekit_characteristic_t* c3
  ) noexcept
    :
    CService{ service, { c0, c1, c2, c3 } }
  {}
  #pragma endregion

  #pragma region CService<5>(...)
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 5, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1,
    const homekit_characteristic_t* c2,
    const homekit_characteristic_t* c3,
    const homekit_characteristic_t* c4
  ) noexcept
    :
    CService{ service, { c0, c1, c2, c3, c4 } }
  {}
  #pragma endregion

  #pragma region CService<6>(...)
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 6, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1,
    const homekit_characteristic_t* c2,
    const homekit_characteristic_t* c3,
    const homekit_characteristic_t* c4,
    const homekit_characteristic_t* c5
  ) noexcept
    :
    CService{ service, { c0, c1, c2, c3, c4, c5 } }
  {}
  #pragma endregion

  #pragma region CService<7>(...)
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 7, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1,
    const homekit_characteristic_t* c2,
    const homekit_characteristic_t* c3,
    const homekit_characteristic_t* c4,
    const homekit_characteristic_t* c5,
    const homekit_characteristic_t* c6
  ) noexcept
    :
    CService{ service, { c0, c1, c2, c3, c4, c5, c6 } }
  {}
  #pragma endregion

  #pragma region CService<8>(...)
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 8, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1,
    const homekit_characteristic_t* c2,
    const homekit_characteristic_t* c3,
    const homekit_characteristic_t* c4,
    const homekit_characteristic_t* c5,
    const homekit_characteristic_t* c6,
    const homekit_characteristic_t* c7
  ) noexcept
    :
    CService{ service, { c0, c1, c2, c3, c4, c5, c6, c7 } }
  {}
  #pragma endregion

  #pragma region CService<9>(...)
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 9, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1,
    const homekit_characteristic_t* c2,
    const homekit_characteristic_t* c3,
    const homekit_characteristic_t* c4,
    const homekit_characteristic_t* c5,
    const homekit_characteristic_t* c6,
    const homekit_characteristic_t* c7,
    const homekit_characteristic_t* c8
  ) noexcept
    :
    CService{ service, { c0, c1, c2, c3, c4, c5, c6, c7, c8 } }
  {}
  #pragma endregion

  #pragma region CService<10>(...)
  template<typename T = void, typename = std::enable_if_t<Characteristic_Count == 10, T>>
  constexpr CService
  (
    const homekit_service_t& service,
    const homekit_characteristic_t* c0,
    const homekit_characteristic_t* c1,
    const homekit_characteristic_t* c2,
    const homekit_characteristic_t* c3,
    const homekit_characteristic_t* c4,
    const homekit_characteristic_t* c5,
    const homekit_characteristic_t* c6,
    const homekit_characteristic_t* c7,
    const homekit_characteristic_t* c8,
    const homekit_characteristic_t* c9
  ) noexcept
    :
    CService{ service, { c0, c1, c2, c3, c4, c5, c6, c7, c8, c9 } }
  {}
  #pragma endregion


  //END Construction
  #pragma endregion

  #pragma region Properties
  constexpr int Count() const noexcept
  { 
    int i;
    for(i = 0; i < Characteristic_Count && Characteristics[i]; ++i)
    {
      if(Characteristics[i] == nullptr)
        break;
    }
    return i;
  }
  #pragma endregion

  #pragma region Operators
  /* Get the address of the service
  * @return The address of the service
  * @example const homekit_service_t* service = &Service;
  */
  constexpr const homekit_service_t* operator&() const noexcept
  {
    return &Service;
  }


  CService& operator=(const CService&) = delete;
  CService& operator=(CService&&) = delete;

  #pragma endregion

};
#pragma endregion

#pragma region CAccessory
/* A HomeKit accessory with a fixed number of services
* @tparam Service_Count The number of services in the accessory
* @example

    const CAccessory<2> Accessory1
    {
      {
        //.id = 1,
        .category = homekit_accessory_category_switch,
        //.config_number = 1,
      },

      &Device,
      &Switch
    };

*/
template<int Service_Count>
struct CAccessory
{
  #pragma region Fields
  const homekit_service_t*  Services[Service_Count + 1]{};
        homekit_accessory_t Accessory{};

  #pragma endregion

  #pragma region Construction

  #pragma region Specials
  CAccessory(const CAccessory&) = delete;
  CAccessory(CAccessory&&) = delete;

  /* Create a HomeKit accessory with a fixed number of services
  * @param accessory The accessory definition
  * @param services The services of the accessory
  * @example CAccessory<2> accessory{ { .id = 1, .category = homekit_accessory_category_switch, .config_number = 1, .services{ &Service1, &Service2 } };
  */
private:
  constexpr CAccessory
  (
    const homekit_accessory_t& accessory,
    std::initializer_list<const homekit_service_t*> services
  ) noexcept
    :
    Accessory
    {
      .id = accessory.id ? accessory.id : 1,
      .category = accessory.category,
      .config_number = accessory.config_number ? accessory.config_number : 1,
      .services{ Services }
    }
  {
    // TODO: Check - Currently not supported by GCC
    //static_assert(Service_Count == services.size(), "Service_Count != services.size()");

    int i = 0;
    for(auto& s : services)
      Services[i++] = s;
  }

public:
  constexpr CAccessory() {}

  #pragma endregion

  #pragma region CAccessory<2>(...)
  /* Create a HomeKit accessory with a fixed number of services
  * @param accessory The accessory definition
  * @param s0, s1, ... The services of the accessory
  */
  template<typename T = void, typename = std::enable_if_t<Service_Count == 2, T>>
  constexpr CAccessory
  (
    const homekit_accessory_t& accessory,
    const homekit_service_t* s0,
    const homekit_service_t* s1
  ) noexcept
    : CAccessory{ accessory, { s0, s1 } }
  {}

  #pragma endregion

  #pragma region CAccessory<3>(...)
  template<typename T = void, typename = std::enable_if_t<Service_Count == 3, T>>
  constexpr CAccessory
  (
    const homekit_accessory_t& accessory,
    const homekit_service_t* s0,
    const homekit_service_t* s1,
    const homekit_service_t* s2
  ) noexcept
    : CAccessory{ accessory, { s0, s1, s2 } }
  {}
  #pragma endregion


  //END Construction
  #pragma endregion

  #pragma region Operators
  /* Get the address of the accessory
  * @return The address of the accessory
  * @example const homekit_accessory_t* accessory = &Accessory;
  */
  constexpr const homekit_accessory_t* operator&() const noexcept
  {
    return &Accessory;
  }

  CAccessory& operator=(const CAccessory&) = delete;
  CAccessory& operator=(CAccessory&&) = delete;

  #pragma endregion

};
#pragma endregion

#pragma region CConfig
/* A HomeKit configuration with a fixed number of accessories
* @tparam Accessory_Count The number of accessories in the configuration
* @example

    const CConfig<1> config
    {
      {
        .password = "111-11-111"
      },

      &Accessory1
    };

*/
template<int Accessory_Count>
struct CConfig
{
  #pragma region Fields
  const homekit_accessory_t*    Accessories[Accessory_Count + 1]{};
        homekit_server_config_t Config{};
        char                    Password[12]{};

  #pragma endregion

  #pragma region Construction

  #pragma region Specials
  CConfig(const CConfig&) = delete;
  CConfig(CConfig&&) = delete;

private:
  /* Create a HomeKit configuration with a fixed number of accessories
  * @param config The configuration definition
  * @param accessories The accessories of the configuration
  * @example CConfig<2> config{ { .accessories{ &Accessory1, &Accessory2 }, .category = homekit_accessory_category_switch, .config_number = 1, .password
  */
  CConfig
  (
    const homekit_server_config_t& config,
    std::initializer_list<const homekit_accessory_t*> accessories
  ) noexcept
  :
    Config
    {
      .accessories{ Accessories },
      .category = config.category ? config.category : (*accessories.begin())->category,
      .config_number = config.config_number ? config.config_number : (*accessories.begin())->config_number,
      .password = Password,
      .password_callback = config.password_callback,
      .setupId = config.setupId,
      .on_resource = config.on_resource,
      .on_event = config.on_event
    }
  {
    // TODO: Check - Currently not supported by GCC
    //static_assert(Accessory_Count == accessories.size(), "Accessory_Count != accessories.size()");

    int k, i = 0;
    for(auto& s : accessories)
      Accessories[i++] = s;

    /*
    * @note Pattern "111-11-111"
    */
    for(k = 0, i = 0; i < sizeof(Password) - 1 - 1 && config.password[k]; ++k)
    {
      if(config.password[k] != '-')
      {
        if(i == 3 || i == 6)
          Password[i++] = '-';
        Password[i++] = config.password[k];
      }
    }
    Password[i] = 0;
  }

public:
  constexpr CConfig() {}

  #pragma endregion

  #pragma region CConfig<1>(...)
  /* Create a HomeKit configuration with a fixed number of accessories
  * @param config The configuration definition
  * @param a0, a1, ... The accessories of the configuration
  */
  template<typename T = void, typename = std::enable_if_t<Accessory_Count == 1, T>>
  constexpr CConfig
  (
    const homekit_server_config_t& config,
    const homekit_accessory_t* a0
  ) noexcept
    : CConfig{ config, { a0 } }
  {}

  #pragma endregion

  #pragma region CConfig<2>(...)
  template<typename T = void, typename = std::enable_if_t<Accessory_Count == 2, T>>
  constexpr CConfig
  (
    const homekit_server_config_t& config,
    const homekit_accessory_t* a0,
    const homekit_accessory_t* a1
  ) noexcept
    : CConfig{ config, { a0, a1 } }
  {}

  #pragma endregion


  //END Construction
  #pragma endregion

  #pragma region Operators
  constexpr const homekit_server_config_t* operator&() const noexcept
  {
    return &Config;
  }

  CConfig& operator=(const CConfig&) = delete;
  CConfig& operator=(CConfig&&) = delete;

  #pragma endregion
};

#pragma endregion


//END Configuration Descriptions
#pragma endregion

#pragma region +Logging - Classes

#pragma region ILoggingEnumerator
/* Interface for an enumerator of log lines.
* @example
*       ILoggingEnumerator_Ptr enumerator = GetLoggingEnumerator();
*       if(enumerator)
*       {
*         while(enumerator->Next())
*         {
*           const char* line = enumerator->Current();
*           Serial.println(line);
*         }
*       }
*/
struct ILoggingEnumerator
{
  virtual ~ILoggingEnumerator() = default;
  virtual bool Next() = 0;
  virtual const char* Current() const = 0;
};

using ILoggingEnumerator_Ptr = std::unique_ptr<ILoggingEnumerator>;
#pragma endregion

#pragma region ILogging
/* Interface for a logging object.
* @see InstallLogging, GetLoggingEnumerator
*/
struct ILogging
{
  virtual ~ILogging() = default;
  virtual void NewLine() = 0;
  virtual void AddLine(const char* text) = 0;
  virtual void AddLine(const String& text) { AddLine(text.c_str()); }
  virtual void Add(char c) = 0;
  virtual void Add(const char* text) = 0;
  virtual void Clear() = 0;
  virtual ILoggingEnumerator_Ptr GetEnumerator() const = 0;
  virtual String GetLine(uint32_t id) const = 0;

};

using ILogging_Ptr = std::unique_ptr<ILogging>;
extern ILogging_Ptr gbLogging;

#pragma endregion

#pragma region CLogging
/* A logging object with a fixed number of lines and characters per line.
* @see InstallLogging, GetLoggingEnumerator
*/
template<size_t MaxLines = 60, size_t CharsPerLine = 128>
class CLogging : public ILogging
{
  #pragma region Fields
  using Cline = std::array<char, CharsPerLine + 1>;
  std::vector<Cline> mBuffer;
  uint16_t mEndIndex{};
  bool mFull{};
  bool mNeedClearLine{};

public:
  #pragma endregion

  #pragma region CEnumerator
  struct CEnumerator : ILoggingEnumerator
  {
    CLogging& mLogging;
    size_t mIndex;
    size_t mCount;

    CEnumerator(const CLogging& logging, size_t index, size_t count)
      :
      mLogging(const_cast<CLogging&>(logging)),
      mIndex(index),
      mCount(count)
    {
    }

    size_t LineIndex() const
    {
      return mIndex;
    }

    virtual bool Next() override
    {
      if(mCount == 0)
        return false;
      --mCount;
      if(++mIndex >= MaxLines)
        mIndex = 0;

      // terminate the string
      auto& Line = mLogging.mBuffer[LineIndex()];
      Line.data()[Line.size() - 1] = 0;
      return true;
    }

    virtual const char* Current() const override
    {
      return mLogging.mBuffer[LineIndex()].data();
    }

  };

  #pragma endregion

  #pragma region Construction
  CLogging(const CLogging&) = delete;
  CLogging(CLogging&&) = delete;
  CLogging& operator=(const CLogging&) = delete;
  CLogging& operator=(CLogging&&) = delete;

  CLogging()
  {
    mBuffer.resize(MaxLines);
    Clear();
  }

  #pragma endregion

  #pragma region Public Methods

  #pragma region NewLine
  virtual void NewLine() override
  {
    ++mEndIndex;
    if(mEndIndex >= MaxLines)
    {
      mEndIndex = 0;
      mFull = true;
    }

    mNeedClearLine = true;
  }
  #pragma endregion

  #pragma region AddLine
  virtual void AddLine(const char* text) override
  {
    StrCatCurPos(text);
    NewLine();
  }

  #pragma endregion

  #pragma region Add
  virtual void Add(char c) override
  {
    char Buf[2] = { c, 0 };
    StrCatCurPos(Buf);
  }

  virtual void Add(const char* text) override
  {
    StrCatCurPos(text);
  }

  #pragma endregion

  #pragma region Clear
  virtual void Clear() override
  {
    mFull = false;
    mEndIndex = 0;
    for(auto& Line : mBuffer)
      Line.fill(0);
  }

  #pragma endregion

  #pragma region GetEnumerator
  virtual ILoggingEnumerator_Ptr GetEnumerator() const override
  {
    size_t Count, Cur;

    if(mFull)
    {
      Cur = mEndIndex;
      Count = MaxLines;
    }
    else
    {
      Cur = 0;
      Count = mEndIndex;
    }

    return std::make_unique<CEnumerator>(*this, Cur, Count);
  }

  #pragma endregion

  #pragma region  GetLine
  virtual String GetLine(uint32_t id) const
  {
    if(id >= MaxLines)
      return String();
    if(mFull)
      id += mEndIndex;

    return String(mBuffer[id % MaxLines].data());
  }
  #pragma endregion



  //END Public Methods
  #pragma endregion

  #pragma region Private Methods
private:

  void ClearLineIfNeeded()
  {
    if(mNeedClearLine)
    {
      mNeedClearLine = false;
      mBuffer[mEndIndex][0] = 0;
    }
  }

  void StrCatCurPos(const char* src)
  {
    auto& Line = mBuffer[mEndIndex];
    ClearLineIfNeeded();
    strncat(Line.data(), src, Line.size() - 1);
  }


  //END Private Methods
  #pragma endregion

};

#pragma endregion

//END Logging
#pragma endregion

#pragma region +Password/Connection - Classes

#pragma region IPasswordEnumerator
/* Interface for an enumerator of SSID/password pairs. */
struct IPasswordEnumerator
{
  virtual ~IPasswordEnumerator() = default;

  /* Reset the enumerator to the first element.   */
  virtual void Reset() = 0;

  /* Move to the next element.*/
  virtual bool Next() = 0;

  /* Get the SSID of the current element. */
  virtual String GetSSID() const = 0;

  /* Get the password of the current element. */
  virtual String GetPassword() const = 0;

  /* The (current) ssid and password was accepted. */
  virtual void Accepted() = 0;

};

using IPasswordEnumerator_Ptr = std::shared_ptr<IPasswordEnumerator>;

#pragma endregion

#pragma region CStaticPasswordEnumerator
/* Password of SSID/password pairs
* @see CreatePasswordEnumerator
*/
template<typename TableType>
struct CStaticPasswordEnumerator : IPasswordEnumerator
{
  #pragma region Fields
  using const_iterator = typename TableType::const_iterator;

  TableType       mTab;
  const_iterator  mItr;
  const_iterator  mNext;

  #pragma endregion

  #pragma region Construction
  constexpr CStaticPasswordEnumerator(TableType loginTab) noexcept
    : mTab(loginTab)
    , mItr(mTab.end())
    , mNext(mTab.end())
  {
  }

  #pragma endregion

  #pragma region Properties
  const_iterator begin() const noexcept { return mTab.begin(); }
  const_iterator end() const noexcept { return mTab.end(); }

  #pragma endregion

  #pragma region Methods
  virtual void Reset() override
  {

    mItr = mTab.end();
    mNext = mTab.begin();
  }

  virtual bool Next() override
  {
    if(mNext == end())
      return false;

    mItr = mNext;
    ++mNext;
    return true;
  }

  virtual String GetSSID() const override
  {
    return std::get<0>(*mItr);
  }

  virtual String GetPassword() const override
  {
    return std::get<1>(*mItr);
  }

  virtual void Accepted() override
  {
  }

  #pragma endregion
};

#pragma endregion

#pragma region CWiFiConnection
/* Class for connecting to a WiFi network. */
class CWiFiConnection
{
  #pragma region Types

  /*
      enum WiFiMode_t
      {
              WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
      };

  */


  //END Types
  #pragma endregion

  #pragma region Fields
  IPasswordEnumerator_Ptr     mPasswordEnumerator;
  uint32_t                    mConnectingTimeout;
  WiFiMode_t                  mRecomendedMode;
  Ticker                      mDnsTicker;
  std::unique_ptr<DNSServer>  mDNSServer;
  bool                        mInUse{};

  static WiFiMode_t mCurrentMode;
public:
  #pragma endregion

  #pragma region CParams
  struct CParams
  {
    IPasswordEnumerator_Ptr Password;
    uint32_t                Timeout{ 30 * 1000 };
    WiFiMode_t              RecomendedMode{ WIFI_STA };
  };

  #pragma endregion

  #pragma region Construction

  CWiFiConnection(CParams params) noexcept
    : mPasswordEnumerator(std::move(params.Password))
    , mConnectingTimeout(params.Timeout)
    , mRecomendedMode(params.RecomendedMode)
  {
  }

  #pragma endregion

  #pragma region Properties
  void SetPasswordEnumerator(IPasswordEnumerator_Ptr passwordEnumerator)
  {
    mPasswordEnumerator = std::move(passwordEnumerator);
  }

  bool HasPasswordEnumerator() const
  {
    return mPasswordEnumerator != nullptr;
  }

  WiFiMode_t GetRecomendedMode() const
  {
    return mRecomendedMode;
  }

  void SetRecomendedMode(WiFiMode_t mode)
  {
    mRecomendedMode = mode;
  }

  static WiFiMode_t GetCurrentMode()
  {
    return mCurrentMode;
  }

  String WifiInfoText() const;

  /* Called each time the connection is used and
  * indicates that the connection is in use.
  * @note Cleared by Connect(), STAConnect(), APConnect()
  */
  void SetInUse()
  {
    mInUse = true;
  }

  bool IsInUse() const
  {
    return mInUse;
  }

  //END Properties
  #pragma endregion

  #pragma region Static Methods
  static bool IsSTA(WiFiMode_t m) { return (m & WIFI_STA) != 0; }
  static bool IsAP(WiFiMode_t m) { return (m & WIFI_AP) != 0; }
  static bool IsSTA() { auto M = GetCurrentMode(); return !IsAP(M) && IsSTA(M); }
  static bool IsAP() { auto M = GetCurrentMode(); return !IsSTA(M) && IsAP(M); }

  #pragma endregion

  #pragma region Public Methods

  /* Connect to a WiFi network.
  * @return true if the connection was successful, false otherwise.
  * @note GetRecomendedMode is used to determine the mode.
  * @note CurrentMode is set to the mode.
    */
  bool Connect(bool tryMode = false);

  /* Connect to a WiFi network.
  * @return true if the connection was successful, false otherwise.
  * @note CurrentMode set to WIFI_STA
  * @note PasswordEnumerator is used to get the SSID and password.
    */
  bool STAConnect();

  /* Make local Wifi available.
  * @return true if the connection was successful, false otherwise.
  * @note If GetHomeKitConfig returns null or DeviceNameFrom, the default SSID = "ESP8266" and password = "12345678" are used.
  * @note   The password is taken from the GetHomeKitConfig. All "-" characters are removed.
  * @note The DeviceName is also used as the SSID. @see DeviceName()
  * @note CurrentMode set to WIFI_AP
  * @note Provide a DNS server for the captive portal.
    * @see https://www.qrcode-generator.de/solutions/wifi-qr-code/
    */
  bool APConnect();

  //END Public Methods
  #pragma endregion

  #pragma region Private Methods
private:

  /* Try to connect to a WiFi network.
  * @param ssid The SSID of the network.
  * @param password The password of the network.
  * @return true if the connection was successful, false otherwise.
    */
  bool TryConnectWiFi(const String& ssid, const String& password);

  //END Private Methods
  #pragma endregion


};

#pragma endregion

//END Password
#pragma endregion

#pragma region +WebSite

#pragma region CHtmlWebSiteMenuItem
struct CHtmlWebSiteMenuItem
{
  using CGetBool = std::function<bool()>;

  const char* Title{};    // Optional: Title of the website: e.g. "HomeKit Device"
  const char* MenuName{}; // Name of the menu item: e.g. "Settings"

  /* ID of the menu item (as URI).
  * "/" means the root menu
  * @note Must start with a slash!
  * @example "/settings"
  */
  const char* URI{};

  /* Pages that are smaller than 10kb must be marked.
  * @note Important for servers that do not support HTTP1.1 and the page
  * length must be known before sending. This means that the page
  * must be completely available in the main memory.  
  */
  bool        LowMemoryUsage{};

  /* Optional menu item ID (for easy access),
  * negative values are forbidden
  */
  int           ID{};
  bool          SpecialMenu{};      // Optional: e.g. right justified for "About"
  int           RefreshInterval{};  // Optional: Refresh (reload page) interval in seconds
  CTextEmitter  CSS;                // Optional: CSS for the website
  CTextEmitter  JavaScript;         // Optional: JavaScript for the website
  CTextEmitter  Body;               // The body of the website
  CGetBool      Visible;            // Optional ability to temporarily hide a menu.
};

#pragma endregion

#pragma region IHtmlWebSiteBuilder
struct IHtmlWebSiteBuilder
{
  #pragma region Types

  struct CHeaderParams
  {
    const char*   Title{};
    const char*   ExText{};
    CTextEmitter  CSS{};
    CTextEmitter  JavaScript{};
    int           RefreshInterval{};
  };

  struct CMenuStripItem
  {
    const char* Title{};
    const char* URI{};      // e.g. "/settings"
    bool        Selected{};
    bool        Special{};  // e.g. right justified for "About"
  };

  struct IMenuStripItems
  {
    virtual ~IMenuStripItems() = default;
    virtual bool Next() = 0;
    virtual CMenuStripItem Current() = 0;
  };

  #pragma endregion


  virtual ~IHtmlWebSiteBuilder() = default;

  virtual void Header(Stream&, CHeaderParams) = 0;
  virtual void Tail(Stream&) = 0;
  virtual void MenuStrip(Stream&, IMenuStripItems&) = 0;
  virtual void Body(Stream&, CTextEmitter) = 0;


};

using IHtmlWebSiteBuilder_Ptr = std::unique_ptr<IHtmlWebSiteBuilder>;

#pragma endregion


/* Create a new HTML web site builder. */
IHtmlWebSiteBuilder_Ptr CreateHtmlWebSiteBuilder();

//END WebSite
#pragma endregion

#pragma region CmdAttribute
/* Attributes for commands (@see CController::InstallCmd)
*/
enum CmdAttribute
{
  CmdAttr_None,
  CmdAttr_FirstSendPage, // Send page before executing the command
};

#pragma endregion

#pragma region CController
/* Deploy Web site, manage and use variables and commands:
*/
class CController
{
  #pragma region Public Types
public:
  static constexpr size_t MaxTickerSlots = 8;
  static constexpr size_t MaxOneShotTickerSlots = 5;

  /* Expander for variables (@see SetVar, ExpandVariables)
  */
  using Expander = std::function<CTextEmitter(CInvokerParam)>;

  #pragma region CCmdItem
  struct CCmdItem
  {
    CInvoker      Invoker;  // Command function
    String        Text;     // Command text (e.g. for html-code)
    CmdAttribute  Attr{};
  };

  #pragma endregion

  #pragma region CWiFiConnectionCtrl
  struct CWiFiConnectionCtrl
  {
    static constexpr uint32_t STAWatchDogTimeoutMS = 1 * 60 * 1000; // 1 minute
    static constexpr uint32_t APWatchDogTimeoutMS  = 5 * 60 * 1000; // 5 minutes

    CController&  mCtrl;

    CWiFiConnectionCtrl(CController& ctrl)
      : mCtrl(ctrl)
    {
    }

    void ConnectOrDie();
  };

  #pragma endregion

  //END Public Types
  #pragma endregion

  #pragma region Private Types
private:
  struct CMenuStripItems; /* Enumerator for menu items */

  //END Private Types
  #pragma endregion

  #pragma region Private Fields
private:
  using CMD_Map = std::map<String, CCmdItem>;
  using VAR_Map = std::map<String, Expander>;
  using MENU_List = std::list<CHtmlWebSiteMenuItem>;

  MENU_List               mMenuItems;
  CMD_Map                 mCMDs;
  VAR_Map                 mVars;
  IHtmlWebSiteBuilder_Ptr mBuilder;
  CWiFiConnection&        mWifiConnection;

  /* Selected menu item index.
  * In the range of [0 .. mMenuItems.size()-1]
  */
  int mSelectedMenuIndex{};

  /* Only menus with low memory usage are visible.
  * @see CHtmlWebSiteMenuItem::LowMemoryUsage
  * @see SendHtmlPage
  */
  bool mOnlyLMUMenusVisible{};

  /* Disable all web requests.
  */
  bool mDisableWebRequests{};

  using const_menu_iterator = MENU_List::const_iterator;
  using const_cmd_iterator = CMD_Map::const_iterator;

  #pragma endregion

  #pragma region Public Fields
public:
  COneShotTickerSlots<MaxOneShotTickerSlots>  OneShotTicker;
  CTickerSlots<MaxTickerSlots>                Ticker;
  CWiFiConnectionCtrl                         WiFiCtrl;

  #pragma endregion

  #pragma region Construction
public:
  CController
  (
    CWiFiConnection& wifiConnection,
    IHtmlWebSiteBuilder_Ptr builder = CreateHtmlWebSiteBuilder()
  )
    : mWifiConnection{ wifiConnection }
    , mBuilder{ std::move(builder) }
    , WiFiCtrl{ *this }
  {
  }

  #pragma endregion

  #pragma region Properties

  /* Get the selected menu item.
  * @return The selected menu item or nullptr if no menu item is selected.
  * @see GetSelectedMenuItemIndex, SetSelectedMenuItemIndex
  */
  const CHtmlWebSiteMenuItem* GetSelectedMenuItem() const;

  /* Get the ID of the selected menu item.
  * @return The ID of the selected menu item or -1 if no menu item is selected.
  * @see GetSelectedMenuItem, SetSelectedMenuItemIndex
  * @note CHtmlWebSiteMenuItem::ID is used as ID
  */
  int GetSelectedMenuItemID() const;

  /* Get the index of the selected menu item. [0 ... mMenuItems.size()-1]
  * @return The index of the selected menu item or -1 if no menu item is selected.
  * @see GetSelectedMenuItem, SetSelectedMenuItemIndex
  */
  int GetSelectedMenuItemIndex() const { return mSelectedMenuIndex; }

  /* Set the selected menu item by index. [0 ... mMenuItems.size()-1]
  * @param index The index of the menu item to select.
  * @return true if the menu item was selected, false otherwise.
  * @see GetSelectedMenuItem, GetSelectedMenuItemIndex
  */
  bool SetSelectedMenuItemIndex(int index);


  //END Properties
  #pragma endregion

  #pragma region Public Methods

  #pragma region AddMenuItem
/* Add a menu item to the web site.
* @param item The menu item to add.
* @see AddDeviceMenu, AddLoggingMenu, AddWiFiLoginMenu
* @example
     AddMenuItem
     (
              {
                  .Title = "HomeKit Settings",
                  .MenuName = "Settings",
                  .URI = "/settings",
                  .Body = []() -> String
                  {
                      return F(R"(Html-Body text)");
                  },
              }
   );
  */
  CController& AddMenuItem(CHtmlWebSiteMenuItem&& item);

  #pragma endregion

  #pragma region InstallCmd
  /* Add a command that creates a button on the HTML page.
    * When the button is clicked, a function is performed.
  * @param cmd Name of the command.
    *            For better readability in HTML code, capital letters should always be used.
  *            Allowed literals: A-Z 0-9 and -_
  * @param func The function that is to be called when you click on the html button.
  * @see InvokeCMD, ExpandVariables
  * @example
            Source-Code:
                InstallCmd
                (
                    "TEST",
                    [](auto)
                    {
                        Serial.println("TEST pressed");
                    }
                );
                InstallCmd
                (
          "OPEN", "Open Door",
                    [](auto)
                    {
                        Serial.println("The door opens");
                    }
                );

            HTML Source-Code: (via CHtmlWebSiteMenuItem::Body)
        {FORM_CMD:TEST} {FORM_CMD:OPEN}

      Webbrowser:
        <button type='submit' name='CMD' value='TEST'>TEST</button>
        <button type='submit' name='CMD' value='OPEN'>Open Door</button>
    */
  CController& InstallCmd(const char* cmd, CInvoker&& func);
  CController& InstallCmd(const char* cmd, CmdAttribute attr, CInvoker&& func);
  CController& InstallCmd(const char* cmd, const char* text, CInvoker&& func);
  CController& InstallCmd(const char* cmd, const char* text, CmdAttribute attr, CInvoker&& func);

  #pragma endregion

  #pragma region SetVar
  /* Add a variable that will be expanded during HTML body generation.
* @param var Name of the variable.
*            For better readability in HTML code, capital letters should always be used.
*            Allowed literals: A-Z 0-9 and -_
* @param expander The function that expands the variable.
* @param content The content of the variable.
* @see ExpandVariables, TryGetVar
* @example
    SetVar
    (
      "VAR",
      [](auto param)
      {
        return MakeTextEmitter(F("VAR-Value"));
      }
    );
  --or--
    SetVar("VAR", "VAR-Value");
  */
  CController& SetVar(const char* var, Expander&& expander);
  CController& SetVar(const String& var, Expander&& expander);
  CController& SetVar(const char* var, String content);
  CController& SetVar(const String& var, String content);

  #pragma endregion

  #pragma region TryGetVar
  /* Try to get the content of a variable.
  * @param var Name of the variable.
  * @param pContent The content of the variable. If the variable was not found, the content is emptyString.
  *              pContent is ignored if nullptr. In this case, only the return value is evaluated.
  * @return true if the variable was found, false otherwise.
  * @see SetVar
  * @note: TryGetVar(name, nullptr) is used to check if a variable exists.
    */
  bool TryGetVar(const char* var, String* pContent, CInvokerParam param = {}) const;
  bool TryGetVar(const String& var, String* pContent, CInvokerParam param = {}) const;
  bool TryGetVar(const String& var, Stream& out, CInvokerParam param = {}) const;

  #pragma endregion

  #pragma region Setup
  /* Setup the controller. Must be called before using the controller.
    * Standard variables and commands are created. The web server is also prepared to serve the pages.
  * @see InstallStdVariables, InstallStdCMDs
  * @note CController::Setup() is also called.
  */
  void Setup();

  #pragma endregion

  #pragma region Loop
  void Loop(bool tickerEnabled = true);

  #pragma endregion

  #pragma region DisableWebRequests / EnableWebRequests

  void DisableWebRequests() { mDisableWebRequests = true; }
  void EnableWebRequests() { mDisableWebRequests = false; }

  #pragma endregion

  #pragma region IndexOfMenuURI
  /* Get the index of a menu item by URI.
  * @param uri The URI of the menu item. @see CHtmlWebSiteMenuItem::URI
  * @return The index of the menu item or -1 if the menu item was not found.
  */
  int IndexOfMenuURI(const char* uri) const;

  #pragma endregion

  #pragma region ForEachMenuItem / ForEachCmdName / ForEachVar
  /*
  * @param F func: Function to be called for each menu item: void func(const CHtmlWebSiteMenuItem& item)
  */
  template<typename F>
  void ForEachMenuItem(F func) const
  {
    for(auto& Item : mMenuItems)
      func(Item);
  }

  /* @param F func: Function to be called for each command name: void func(const String&)
  */
  template<typename F>
  void ForEachCmdName(F func) const
  {
    for(auto& Item : mCMDs)
      func(Item.first);
  }

  /* @param F func: Function to be called for each command: void func(const String& name, const String& content)
  */
  template<typename F>
  void ForEachVar(F func) const
  {
    for(auto& Item : mVars)
      func(Item.first, to_string(Item.second(nullptr)));
  }


  #pragma endregion

  #pragma region GetCmdText
  /* Get the text for a command
  * @param cmd The command
  * @return The text for the command.
  *         If the command has no text or no command named cmd is found,
  *         the return value is cmd.
  */
  const char* GetCmdText(const char* cmd) const;

  #pragma endregion

  #pragma region ExpandVariables
  /* Expand variables in the input text
  * @param _inputText The input text with variables
  * @note: Variables are enclosed in curly brackets, e.g. "{VarName:Args}"
  * @example
  *   "Today is {DATE}" -> "Today is 2024-09-12"
  */
  void ExpandVariables(Stream&, const String& _inputText) const;

  #pragma endregion

  #pragma region Debug-Helpers

  const CController& DebugPrintCMDs() const
  {
    Serial.println("CMDs:");
    ForEachCmdName
    (
      [](const String& cmd)
      {
        Serial.printf("\t\"%s\"\n", cmd.c_str());
      }
    );
    Serial.println();
    return *this;
  }

  const CController& DebugPrintVARs() const
  {
    Serial.println("VARs:");
    ForEachVar
    (
      [](const String& name, const String& content)
      {
        Serial.printf("\t\"%s\" = \"%s\"\n", name.c_str(), content.c_str());
      }
    );
    Serial.println();
    return *this;
  }

  const CController& DebugPrintMenuItems() const
  {
    Serial.println("MenuItems:");
    ForEachMenuItem
    (
      [](const CHtmlWebSiteMenuItem& item)
      {
        Serial.printf("\t\"%s\" = \"%s\"\n", item.MenuName, item.URI);
      }
    );
    Serial.println();
    return *this;
  }

  //END Debug-Helpers
  #pragma endregion


  //END Public Methods
  #pragma endregion

  #pragma region Private Methods
  void PerformWebServerRequest();
  void PerformWebServerVarRequest();

  #pragma region DispatchWebseverUriRequest
  /* Distributes a web server request, such as "/device" or "/log",
  * by determining the associated menu page and
  * then sending the (new) web page. @see AddMenuItem
  *
  * @param uri The URI of the request
  * @note:
  * If the URI contains a suffix such as .ico, then it is safe to say that
  * it is not a device-provided page and a 404 will be returned.
  *
  * @note:
  * If no menu item can be associated with the URI, 302 redirects to the root page.
  */
  void DispatchWebseverUriRequest(const char* uri);

  #pragma endregion

  #pragma region FindCMD
  /* Find a command by name.
  * @param cmd The name of the command.
  * @return The iterator to the command or mCMDs.end() if the command was not found.
  */
  const_cmd_iterator FindCMD(const String& cmd) const;

  #pragma endregion

  #pragma region InvokeCMD
  /* Invoke a command by its name
  * @param cmdItr The command iterator @see FindCMD
  * @param cmd The command name
  */
  void InvokeCMD(const_cmd_iterator cmdItr, const String& cmd, CInvokerParam param = {});

  #pragma endregion

  #pragma region InstallStdCMDs
  /* Install standard commands
  * @example {FORM_BEGIN} Commands: {FORM_CMD:REBOOT} {FORM_CMD:RESET_PAIRING} {FORM_END}
  * @note:
  * - REBOOT
  *   Reboots the device
  *
  * - RESET_PAIRING
  *   Resets the HomeKit pairing and restarts the device
  */
  void InstallStdCMDs();

  #pragma endregion

  #pragma region InstallStdVariables
  /* Install standard variables
  * @see ExpandVariables
  * @note: The following standard variables are installed:
  * - LOCAL_IP
  *   The IP address of the device

  * - REMOTE_IP
  *   The IP address of the remote client
  *
  * - MAC
  *   The MAC address of the device
  *
  * - DEVICE_NAME
  *   The name of the device
  *
  * - IS_PAIRED
  *   "Yes" if the device is paired with HomeKit, otherwise "No"
  *
  * - CLIENT_COUNT
  *   The number of connected clients
  *
  * - FORM_CMD:cmdName
  *   A button to execute a command
  *   @example {FORM_CMD:REBOOT}
  *       @see FORM_BEGIN, FORM_END
  *
  * - FORM_BEGIN
  *   Start of a command form
  *   @example {FORM_BEGIN} Command: {FORM_CMD:REBOOT} {FORM_END}
  *   @see FORM_END
  *
  * - FORM_END
  *   End of a command form
  *   @see FORM_BEGIN
  *
  * - PARAM_TABLE_BEGIN
  *   Start of a parameter table
  *   @see PARAM_TABLE_END
  *   @example
  *               {PARAM_TABLE_BEGIN}
  *                   <tr><th>IP</th><td>{IP}</td></tr>
  *                   <tr><th>MAC</th><td>{MAC}</td></tr>
  *               {PARAM_TABLE_END}
  *
  * - PARAM_TABLE_END
  *   End of a parameter table
  *   @see PARAM_TABLE_BEGIN
  *
  * - HTML_LOG
  *   A table with log entries
  *
  * - DATE
  *  The current date (YYYY-MM-DD)
  *
  * - TIME
  *  The current time (HH:MM:SS)
  *
  * - FORMAT_FILESYSTEM
  *  Formats the file system. @see CSimpleFileSystem
  *
  *
  */
  void InstallStdVariables();

  #pragma endregion

  #pragma region SendHtmlPage
  /* Send the HTML page for the currently selected menu.
    * The Html page for the currently selected menu is created using the HtmlWebSiteBuilder.
    */
  void SendHtmlPage();
  void SendHtmlPage(Stream&) const;

  #pragma endregion


  //END Private Methods
  #pragma endregion

};

//END CController
#pragma endregion

#pragma region CSimpleFileSystem
/* File system for storing files in the EEPROM.
 * @note This instance cannot allocate on the stack because it is too large.
 * The file system is organized as follows:
 * - The file system is stored in the EEPROM.
 * - Files can only be overwritten with the length they had at creation.
 * - Filename must be 4 characters long.
*/
class CSimpleFileSystem
{
  #pragma region Types
  enum
  {
    MaxTableEntries = 16,
    NameLength = 4,
    EpromPageSize = 4096, // @see SPI_FLASH_SEC_SIZE

    /* 2024-09-17 HB: May not be #0, otherwise the data will not be stored in the EPROM for unknown reasons. */
    EpromPage = 1
  };

  struct CEntry
  {
    uint8_t  Name[NameLength];
    uint16_t StartOffset;
    uint16_t Size;
  };

  struct CPage
  {
    uint32_t Crc;
    CEntry   Entries[MaxTableEntries];
    uint8_t  Memory[EpromPageSize - MaxTableEntries * sizeof(CEntry) - sizeof(uint32_t)];
  };

  #pragma endregion

  #pragma region Fields
  CPage     mPage;
  size_t    mUsedMemorySize{};
  bool      mWriteBackNeeded{};
  uint8_t   mModifyCounter{};

  #pragma endregion

  #pragma region Construction
public:
  CSimpleFileSystem(const CSimpleFileSystem&) = delete;
  CSimpleFileSystem& operator=(const CSimpleFileSystem&) = delete;
  CSimpleFileSystem(CSimpleFileSystem&&) = delete;
  CSimpleFileSystem& operator=(CSimpleFileSystem&&) = delete;


  CSimpleFileSystem()
  {
    ReadPageOrFormat();
  }
  ~CSimpleFileSystem()
  {
    Sync();
  }

  #pragma endregion

  #pragma region CFileEnumerator
  class CFileEnumerator
  {
    #pragma region Fields
    const CSimpleFileSystem& mFS;
    char    mName[NameLength + 1];
    uint8_t mBeginModifyCounter;
    int     mIndex = -1;

    #pragma endregion

    #pragma region Construction
  public:
    CFileEnumerator(const CSimpleFileSystem& FS)
      : mFS(FS)
      , mBeginModifyCounter(FS.mModifyCounter)
    {
    }

    #pragma endregion

    #pragma region Next
    bool Next();

    #pragma endregion

    #pragma region Current
    const char* Current() const
    {
      return mName;
    }

    #pragma endregion

  };

  //END CFileEnumerator
  #pragma endregion

  #pragma region Properties
  /* If checks whether the FS is empty, i.e. no files are noted
* @return True if the FS is empty.
  */
  bool empty() const
  {
    return mUsedMemorySize == 0;
  }

  #pragma endregion

  #pragma region Public Methods

  #pragma region Files
  /* Enumerates the files in the file system.
   * @return The file enumerator.
  */
  CFileEnumerator Files() const
  {
    return CFileEnumerator(*this);
  }

  #pragma endregion

  #pragma region Sync
  /* Synchronizes the file system with the EEPROM.
   * This method is called automatically in the destructor.
  */
  void Sync();

  #pragma endregion

  #pragma region Exists
  /* Checks if a file exists.
   * @param pName The name of the file.
   * @return True if the file exists.
  */
  bool Exists(const char* pName) const
  {
    return FindEntry(pName) >= 0;
  }

  #pragma endregion

  #pragma region FileSize
  /* Gets the size of a file.
   * @param pName The name of the file.
   * @return The size of the file or -1 if the file does not exist.
  */
  int FileSize(const char* pName) const;

  #pragma endregion

  #pragma region ReadFile
  /* Reads a file.
   * @param pName The name of the file.
   * @param pBuf The buffer to read the file into.
   * @param Size The size of the buffer. Must be equal to the file size.
   * @return The number of bytes read or -1 if the file does not exist or the buffer is too small.
  */
  int ReadFile(const char* pName, uint8_t* pBuf, int Size);

  #pragma endregion

  #pragma region WriteFile
  /* Writes or creates a file.
   * @param pName The name of the file. Must be 4 characters long.
   * @param pBuf The buffer to write.
   * @param Size The size of the buffer. Must be equal to the file size if the file already exists.
   * @return The number of bytes written or -1 if the file system is full.
  */
  int WriteFile(const char* pName, const uint8_t* pBuf, int Size);

  #pragma endregion

  #pragma region Format
  /* Formats the file system.
  */
  void Format();
  #pragma endregion


  #pragma region List
  /* Lists the files in the file system.
  */
  void List();

  #pragma endregion


  //END Public Methods
  #pragma endregion

  #pragma region Private Methods
private:

  /* Touches the file system.
   * This method is called automatically when a file is modified.
  */
  void Touch();

  /* Reads the file system page from the EEPROM.
   * @return True if the page was read successfully.
  */
  bool ReadPage();

  /* Writes the file system page to the EEPROM.
   * @return True if the page was written successfully.
  */
  bool WritePage();

  /* Reads the file system page from the EEPROM or formats the file system if the page is invalid.
  */
  void ReadPageOrFormat();

  /* Finds an entry in the file system.
   * @param pName The name of the file or nullptr to find an empty slot.
   * @return The index of the entry or -1 if the entry was not found.
  */
  int FindEntry(const char* pName = nullptr) const;

  /* Debug prints the file system table.
  */
  void DebugPrintTable();



  //END Private Methods
  #pragma endregion

};

//END CSimpleFileSystem
#pragma endregion

#pragma region SimpleFileSystem
/* Allows the direct call of a method of the CSimpleFileSystem class.
*/
struct SimpleFileSystem
{
  static void Format()
  {
    auto FS = std::make_unique<CSimpleFileSystem>();
    FS->Format();
  }

  static int ReadFile(const char* pName, uint8_t* pBuf, int Size)
  {
    auto FS = std::make_unique<CSimpleFileSystem>();
    return FS->ReadFile(pName, pBuf, Size);
  }

  template<typename T>
  static bool ReadFile(const char* pName, T& p)
  {
    return ReadFile(pName, reinterpret_cast<uint8_t*>(&p), sizeof(T)) == sizeof(T);
  }

  static int WriteFile(const char* pName, const uint8_t* pBuf, int Size)
  {
    auto FS = std::make_unique<CSimpleFileSystem>();
    return FS->WriteFile(pName, pBuf, Size);
  }

  template<typename T>
  static bool WriteFile(const char* pName, const T& p)
  {
    return WriteFile(pName, reinterpret_cast<const uint8_t*>(&p), sizeof(T)) == sizeof(T);
  }

};
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


#pragma region +HomeKit - Services
/* More information about HomeKit services can be found at:
*           HomeKit_GarageDoorOpener.h
*           HomeKit_HeaterCooler.h
*           HomeKit_Sensor.h
*           HomeKit_Switch.h
*           HomeKit_Thermostat.h
*/


#pragma region CDeviceService
/* A HomeKit device service
*  Contains the basic information of the device
*/
struct CDeviceService
{
  #pragma region Fields
  homekit_characteristic_t HKIdentity;
  homekit_characteristic_t HKManufacturer;
  homekit_characteristic_t HKModel;
  homekit_characteristic_t HKName;
  homekit_characteristic_t HKSerialNumber;
  homekit_characteristic_t HKFirmwareRevision;
  char HKDeviceName[64]{};

  CService<6> Service;

  #pragma endregion

  #pragma region BaseAccessories_t
  struct BaseAccessories_t
  {
    const char* DeviceName = "Device";
    uint16_t DeviceID{};
    const char* Model = "ESP8266/ESP32";
    const char* SerialNumber = "0123456";
    const char* FirmwareRevision = "1.0";
    const char* Manufacturer = "Holger Burkarth";
  };

  #pragma endregion

  #pragma region Construction
  CDeviceService(const CDeviceService&) = delete;
  CDeviceService(CDeviceService&&) = delete;

  /*
* @param base The base information of the device
* @example
      constexpr CDeviceService Device // http://Sample.local
      {
        .DeviceName{"Sample"},
      };
--or--
      constexpr CDeviceService Device // http://Sample5F0C.local
      {
        {
          .DeviceName{"Sample"},
          .DeviceID{0x5F0C}
          ...
        }
      };
--or--
      const CDeviceService Device // http://Sample????.local
      {
        {
          .DeviceName{"Sample"},
          .DeviceID{CDeviceService::MAC_DeviceID()}
          ...
        }
      };

   */
  constexpr CDeviceService(BaseAccessories_t base) noexcept
    :
    HKIdentity
    {
      .type = HOMEKIT_CHARACTERISTIC_IDENTIFY,
      .description = "Identify",
      .format = homekit_format_bool,
      .permissions = homekit_permissions_paired_read,
    },

    HKManufacturer{ HOMEKIT_DECLARE_CHARACTERISTIC_MANUFACTURER(base.Manufacturer) },
    HKModel{ HOMEKIT_DECLARE_CHARACTERISTIC_MODEL(base.Model) },
    HKName{ HOMEKIT_DECLARE_CHARACTERISTIC_NAME(HKDeviceName) },
    HKSerialNumber{ HOMEKIT_DECLARE_CHARACTERISTIC_SERIAL_NUMBER(base.SerialNumber) },
    HKFirmwareRevision{ HOMEKIT_DECLARE_CHARACTERISTIC_FIRMWARE_REVISION(base.FirmwareRevision) },

    Service
    {
      {
        .type = HOMEKIT_SERVICE_ACCESSORY_INFORMATION,
        .primary = true
      },

      &HKIdentity,
      &HKManufacturer,
      &HKModel,
      &HKName,
      &HKSerialNumber,
      &HKFirmwareRevision
    }
  {
    auto Digit = [](uint8_t c) -> char
      {
        return c < 10 ? '0' + c : 'A' + c - 10;
      };


    int i = 0;
    // Copy the device name
    for(; i < sizeof(HKDeviceName) - 1 - 4 && base.DeviceName[i]; ++i)
      HKDeviceName[i] = base.DeviceName[i];
    // add base.DeviceID to the device name as 4 HEX digits
    if(base.DeviceID != 0)
    {
      HKDeviceName[i++] = Digit(base.DeviceID >> 12 & 0xF);
      HKDeviceName[i++] = Digit(base.DeviceID >> 8 & 0xF);
      HKDeviceName[i++] = Digit(base.DeviceID >> 4 & 0xF);
      HKDeviceName[i++] = Digit(base.DeviceID & 0xF);
    }
    HKDeviceName[i] = 0;
  }
  #pragma endregion

  #pragma region Static Methods

  #pragma region MAC_DeviceID
  /* Get the device-id of the device.
* @example
        const CDeviceService Device
        {
            {
                .DeviceName{"Sample"},
                .DeviceID{CDeviceService::MAC_DeviceID()}
            }
        };

*/
  static uint16_t MAC_DeviceID()
  {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    return (mac[4] << 8) | mac[5];
  }

  #pragma endregion

  //END Static Methods
  #pragma endregion

  #pragma region Operators
  constexpr const homekit_service_t* operator&() const noexcept
  {
    return &Service;
  }

  #pragma endregion

};

#pragma endregion


//END HomeKit - Services
#pragma endregion


#pragma region >>> CHomeKit <<<
/* HomeKit class.
* @note This class is a singleton.
*/
class CHomeKit
{
  #pragma region Private Fields
  CAccessory<2>                   mInnerAccessory;
  CConfig<1>                      mInnerConfig;
  const homekit_server_config_t*  mConfig{};

  CTicker     mLEDTimer;
  uint32_t    mLastHtmlRequestCount{};
  uint32_t    mLastHtmlRequestMS{};
  bool        mHomeKitServerStarted{};
  bool        mEnergySavingEnabled{true};

  static bool mIsPaired; // True if HomeKit is paired (with Apple)
  #pragma endregion

  #pragma region Public Fields
public:
  CWiFiConnection WifiConnection;
  CController     Controller;

  #pragma endregion

  #pragma region Construction
public:
  CHomeKit(const CHomeKit&) = delete;
  CHomeKit(CHomeKit&&) = delete;
  CHomeKit& operator=(const CHomeKit&) = delete;
  CHomeKit& operator=(CHomeKit&&) = delete;

  #pragma region CHomeKit(&Config, ... )
  /*
  * @example
    CSwitchService Switch{ ... };

    const CDeviceService Device
    {
      .DeviceName{"Switch"},
    };

    const CAccessory<2> Accessory1
    {
      {
        .category = homekit_accessory_category_switch,
      },

      &Device,
      &Switch
    };


    const CConfig<1> gbConfig
    {
      {
        .password = "111-11-111"
      },

      &Accessory1
    };

    CHomeKit HomeKit(&gbConfig);

  */
  CHomeKit
  (
    const homekit_server_config_t* config,
    CWiFiConnection::CParams wifiPass = {},
    IHtmlWebSiteBuilder_Ptr builder = CreateHtmlWebSiteBuilder()
  )
    : mConfig(config)
    , WifiConnection(std::move(wifiPass))
    , Controller(WifiConnection, std::move(builder))
    , mLEDTimer{ 100, &UpdateBuildInLED }
  {
    Singleton = this;
  }

  #pragma endregion

  #pragma region CHomeKit(&Accessory, ...)
  /*
  * @example
    CSensorService Sensor{ ... };

    const CDeviceService Device
    {
      .DeviceName{"Sensor"},
    };

    const CAccessory<3> gbAccessory
    {
      {
        .category = homekit_accessory_category_sensor,
      },

      &Device,
      Sensor[0],
      Sensor[1]
    };

    CHomeKit HomeKit(&gbAccessory);

  */
  CHomeKit
  (
    const homekit_accessory_t* pAccessory,
    CWiFiConnection::CParams wifiPass = {},
    IHtmlWebSiteBuilder_Ptr builder = CreateHtmlWebSiteBuilder()
  )
    : mInnerConfig{ {.password = "111-11-111"}, pAccessory }
    , mConfig(&mInnerConfig)
    , WifiConnection(std::move(wifiPass))
    , Controller(WifiConnection, std::move(builder))
    , mLEDTimer{ 100, &UpdateBuildInLED }
  {
    Singleton = this;
  }

  #pragma endregion

  #pragma region CHomeKit(Device, category, &Service, ...)
  /*
  * @example
    CSwitchService Switch{ ... };

    const CDeviceService Device
    {
      .DeviceName{"Switch"},
    };

    CHomeKit HomeKit(Device, homekit_accessory_category_switch, &Switch);
  */
  CHomeKit
  (
    const CDeviceService& device,
    homekit_accessory_category_t category,
    const homekit_service_t* pService,
    CWiFiConnection::CParams wifiPass = {},
    IHtmlWebSiteBuilder_Ptr builder = CreateHtmlWebSiteBuilder()
  )
    : mInnerAccessory{ {.category = category}, &device, pService }
    , mInnerConfig{ {.password = "111-11-111"}, &mInnerAccessory }
    , mConfig(&mInnerConfig)
    , WifiConnection(std::move(wifiPass))
    , Controller(WifiConnection, std::move(builder))
    , mLEDTimer{ 100, &UpdateBuildInLED }
  {
    Singleton = this;
  }

  #pragma endregion

  //END Construction
  #pragma endregion

  #pragma region Properties
  static CHomeKit* Singleton;

  #pragma region IsPaired
  /* Gets whether the device is HomeKit paired.
  * @return True if HomeKit is paired (with Apple)
  */
  static bool IsPaired()
  {
    if(!mIsPaired)
      mIsPaired = homekit_is_paired();
    return mIsPaired;
  }

  #pragma endregion

  #pragma region Config
  const homekit_server_config_t* Config() const
  {
    return mConfig;
  }
  #pragma endregion

  #pragma region DisableEnergySaving
  /* Deactivate energy-saving mode.
   * This prevents a PowerBank from switching off at
   * a load of less than 20 mA. 
   * @note More likely to be needed during development.
   * @see Loop
  */
  void DisableEnergySaving()
  {
    mEnergySavingEnabled = false;
  }
  #pragma endregion


  //END Properties
  #pragma endregion

  #pragma region Operators
  operator CController& () { return Controller; }
  operator const homekit_server_config_t* () const { return mConfig; }

  //END Operators
  #pragma endregion

  #pragma region Public Methods

  /* Activate local Wifi spot.
  * @note This method must be called in the Setup() function.
  * @see CWiFiConnection::APConnect
  */
  void Activate_AP()
  {
    WifiConnection.SetRecomendedMode(WIFI_AP);
  }

  /* Setup the HomeKit device.
  * @note This method must be called in the setup() function.
  */
  bool Setup();

  /* Loop the HomeKit device.
  * @note This method must be called in the loop() function.
  */
  void Loop();

  /* Reset HomeKit pairing.
  * @param reboot If true, the device will reboot after resetting the pairing.
  */
  static void ResetPairing(bool reboot = false);


  //END Public Methods
  #pragma endregion

  #pragma region Private Methods
private:

  static void UpdateBuildInLED(CTicker::CEvent);

  /* Synchronize the date and time with an NTP server.
  */
  static void SyncDateTime();

  //END Private Methods
  #pragma endregion

};

//END CHomeKit
#pragma endregion



#pragma region +Functions

#pragma region GetHomeKitConfig
/* Get the HomeKit configuration.
* @return The CHomeKit configuration or nullptr if CHomeKit is not created.
*/
inline const homekit_server_config_t* GetHomeKitConfig()
{
  return CHomeKit::Singleton ? CHomeKit::Singleton->Config() : nullptr;
}
#pragma endregion

#pragma region  InstallLogging / GetLoggingEnumerator

/* Install a logging object for VERBOSE(), INFO(), WARN(), ERROR() and logging to the HomeKit log.
* @param logging The logging object (CLogging<>)
* @note InstallLogging() is called in CHomeKit::Setup()
*/
void InstallLogging(ILogging_Ptr logging);
void InstallLogging();

/* Get an enumerator for the logging object.
* @return The enumerator for the logging object or nullptr if no logging object is installed.
*/
ILoggingEnumerator_Ptr GetLoggingEnumerator();

#pragma endregion

#pragma region CreatePasswordEnumerator - Functions

/* Create a password enumerator from a static array.
* @tparam VarType The type of the SSID/password pairs. E.g. const char*.
* @param loginTab The array of SSID/password pairs.
* @return The created enumerator.
* @see IPasswordEnumerator
* @example

        CreatePasswordEnumerator("MyWifi", "123456")

--or--

        CreatePasswordEnumerator
        (
            {
                { "MyWifi",         "123456" },
                { "FriendWifi", "654321" },
                [...]
            }
        )

*/
template<typename VarType, size_t N>
IPasswordEnumerator_Ptr CreatePasswordEnumerator(const std::pair<VarType, VarType>(&loginTab)[N])
{
  using ItemType = std::pair<VarType, VarType>;
  using TableType = std::array<ItemType, N>;

  auto CopyToTable = [](const std::pair<const char*, const char*>(&loginTab)[N]) -> TableType
    {
      TableType Tab;
      for(size_t i = 0; i < N; ++i)
        Tab[i] = ItemType{ std::get<0>(loginTab[i]), std::get<1>(loginTab[i]) };
      return Tab;
    };

  return std::make_unique<CStaticPasswordEnumerator<TableType>>(CopyToTable(loginTab));
}

/* Required specialization for const char */
template<size_t N>
IPasswordEnumerator_Ptr CreatePasswordEnumerator(const std::pair<const char*, const char*>(&loginTab)[N])
{
  return CreatePasswordEnumerator<const char*, N>(loginTab);
}

inline IPasswordEnumerator_Ptr CreatePasswordEnumerator(const char* ssid, const char* pass)
{
  const std::pair<const char*, const char*> Tab[]{ { ssid, pass } };
  return CreatePasswordEnumerator(Tab);
}


#pragma endregion

#pragma region For Each - Functions

#pragma region ForEachAccessory
/*
* @brief: Enumerate all accessories of a homekit configuration
* @param: pConfig: HomeKit configuration
* @param: F func: Function to be called for each accessory:
*   void func(const homekit_accessory_t* pAccessory)
* -or-
*  R func(const homekit_accessory_t* pAccessory)
*/
template<typename F>
void ForEachAccessory(const homekit_server_config_t* pConfig, F func)
{
  if(pConfig)
  {
    for(int i = 0; pConfig->accessories[i]; ++i)
      func(pConfig->accessories[i]);
  }
}
template<typename R, typename F>
R ForEachAccessory(const homekit_server_config_t* pConfig, F func)
{
  if(pConfig)
  {
    for(int i = 0; pConfig->accessories[i]; ++i)
    {
      R tmp = func(pConfig->accessories[i]);
      if(tmp)
        return tmp;
    }
  }
  return R{};
}

#pragma endregion

#pragma region ForEachService
/*
* @brief: Enumerate all services of a homekit accessory
* @param: pAccessory: HomeKit accessory
* @param: F func: Function to be called for each service:
*   void func(const homekit_service_t* pService)
* -or-
*  R func(const homekit_service_t* pService)
*/
template<typename F>
void ForEachService(const homekit_accessory_t* pAccessory, F func)
{
  if(pAccessory)
  {
    for(int i = 0; pAccessory->services[i]; ++i)
      func(pAccessory->services[i]);
  }
}
template<typename R, typename F>
R ForEachService(const homekit_accessory_t* pAccessory, F func)
{
  if(pAccessory)
  {
    for(int i = 0; pAccessory->services[i]; ++i)
    {
      R tmp = func(pAccessory->services[i]);
      if(tmp)
        return tmp;
    }
  }
  return R{};
}

#pragma endregion

#pragma region ForEachCharacteristic
/*
* @brief: Enumerate all characteristics of a homekit service
* @param: pService: HomeKit service
* @param: F func: Function to be called for each characteristic:
*  void func(const homekit_characteristic_t* pCharacteristic)
* -or-
*  R func(const homekit_characteristic_t* pCharacteristic)
*/
template<typename F>
void ForEachCharacteristic(const homekit_service_t* pService, F func)
{
  if(pService)
  {
    for(int i = 0; pService->characteristics[i]; ++i)
      func(pService->characteristics[i]);
  }
}
template<typename R, typename F>
R ForEachCharacteristic(const homekit_service_t* pService, F func)
{
  if(pService)
  {
    for(int i = 0; pService->characteristics[i]; ++i)
    {
      R tmp = func(pService->characteristics[i]);
      if(tmp)
        return tmp;
    }
  }
  return R{};
}

#pragma endregion


//END For Each - Functions
#pragma endregion

#pragma region Find - Functions

/*
* @brief: Find the first service of a given type
* @param pConfig: HomeKit configuration, nullptr -> nullptr
* @param type: Service type (e.g. "SWITCH" or HOMEKIT_SERVICE_SWITCH)
* @return: Pointer to service or nullptr
*/
const homekit_service_t* FindServiceByType(const homekit_server_config_t* pConfig, const char* type);
inline const homekit_service_t* FindServiceByType(const char* type)
{
  return FindServiceByType(GetHomeKitConfig(), type);
}

/*
* @brief: Find the first characteristic of a given name
* @param pConfig: HomeKit configuration, nullptr -> nullptr
* @param name: Service name (e.g. "ON", HOMEKIT_CHARACTERISTIC_ON, "On" )
* @return: Pointer to service or nullptr
*/
homekit_characteristic_t* FindCharacteristicByTypeOrName(const homekit_service_t* pService, const char* typeOrName);

//END Find - Functions
#pragma endregion

#pragma region DeviceName

/* Get the device name from the CHomeKit configuration (CDeviceService)
* @param pConfig: HomeKit configuration, nullptr -> ""
* @return: Device name or ""
* @see CDeviceService
*/
const char* DeviceName(const homekit_server_config_t* pConfig = GetHomeKitConfig());

#pragma endregion

#pragma region smart_gmtime

/* Get the current time in the form of a tm structure.
* @param pTM: Pointer to the tm structure to be filled.
* @return True if the time could be determined, otherwise tm is 1970 based.
* @node By synchronizing online only once a day, an attempt is made to keep the speed high.
*/
bool smart_gmtime(tm* pTM);

/* Get the time in the form of a tm structure.
* @param pTM: Pointer to the tm structure to be filled.
* @param time: Time in seconds since 1970.
* @node By synchronizing online only once a day, an attempt is made to keep the speed high.
*/
void smart_gmtime(tm* pTM, time_t time);

#pragma endregion

#pragma region Split
std::vector<String> Split(const String& text, char sep);

#pragma endregion

#pragma region +Components like: Menus, Vars, Commands

#pragma region AddWiFiLoginMenu
/* Adds a menu (WiFi) that allows the user to connect to a WiFi network.
* @param alwaysUsePeristentLogin
*       If true, the persistent password enumerator is always used.
*       If False, an already set password enumerator is not changed.
*       @see CreatePasswordEnumerator(), Constructor of CController
*       @note Mainly used for testing, debugging and simulating AP-WiFi situations
* @note Following variables will be supplied:
* - WLOGIN_SSID: The SSID of the first WiFi network to connect to.
* - WLOGIN_PASSWORD: The password of the first WiFi network to connect to.
* - WIFI_LIST: The list of available WiFi networks as an HTML table.
* @note Following commands will be installed:
* - ENTER_WLOGIN: Display the password entry page "/wlogin".
* - LOGIN: Connect to the selected WiFi network.
* @note Following menu items will be added:
* - /wifi: Displays a page with a list of available WIFIs.
* - /wlogin: Displays a page to enter the password for the selected WiFi network.
* @note
*   Should be called before any other menus are added so that the 'WiFi' menu item
    is displayed as the first page in AP WiFi mode.  
* @note
*   A persistent password enumerator is installed. This replaces any previously
*   installed enumerator.
*
* @see CWiFiConnection::SetPasswordEnumerator
*/
CController& AddWiFiLoginMenu(CController&, bool alwaysUsePeristentLogin = true);

#pragma endregion

#pragma region AddDeviceMenu
/* Add a info menu item to the web site "/device".
 * The Device item is created on the right side of the menu bar.
 * This page displays information about the device and allows
 * you to restart and unpaired the device.
 * @see AddMenuItem
*/
CController& AddDeviceMenu(CController&);
#pragma endregion

#pragma region AddLoggingMenu
/* Add a log menu item to the web site. "/log"
* The Log item is created on the right side of the menu bar.
* This page displays the most recently recorded log events.
* @see AddMenuItem
*/
CController& AddLoggingMenu(CController&);
#pragma endregion

#pragma region AddStandardMenus
/* Adds standard menu items to the controller.
* @note The following menu items are added:
* - Logging
* - Device
*/
inline CController& AddStandardMenus(CController& c)
{
  AddLoggingMenu(c);
  AddDeviceMenu(c);
  return c;
}

#pragma endregion

#pragma region +ActionUI

#pragma region InstallActionUI
/* Installs the action UI.
* @see ActionUI_JavaScript, ActionUI_CSS
* @Elements:
* - ACTION_BUTTON
*   @note ActionUI_JavaScript must be use in .JavaScript for AddMenuItem()
*   Creates a button that calls a JavaScript function.
*   Functions for setting and reading variables are provided by the @ref ActionUI_JavaScript().
*   @see ActionUI_JavaScript for more information.
*       @syntax:
*      {ACTION_BUTTON:OnFunction:Text}
*   --or--
*      {ACTION_BUTTON:ID:OnFunction:Text}
*   @example
*     {ACTION_BUTTON:SwtID:SetState(true):Switch ON}
*       ==>>   <button id='SwtID' onclick='SetState(true)'>Switch ON</button>
*
* - ACTION_CHECKBOX
*       @syntax:
*      {ACTION_CHECKBOX:ID:OnFunction}
*   @example
*     {ACTION_CHECKBOX:switch:SetSwitch(this.checked)}
*
*
*/
CController& InstallActionUI(CController&);

#pragma endregion

#pragma region ActionUI_JavaScript
/* JavaScript support for action buttons.
* @return The JavaScript code for action buttons.
* @note Used within .JavaScript for @ref AddMenuItem()

* @functions
* - InvokeAction(varLine, functor)
*   @param varLine The variable line to send to the server.
*   @param functor The function to call when the server response is received.
*   @note The functor is called with the server response as a parameter.
*   @example
*     InvokeAction("DATE", function(responseText) { console.log(responseText); });
*     --or--
*     InvokeAction("MYVAL=1", responseText => console.log(responseText));
*
* - SetVar(varName, varValue)
*   @param varName The name of the variable.
*   @param varValue The value of the variable.
*   @example SetVar("MYVAL", 1);
*
* - ForSetVar(varName, varValue, functor)
*   @param varName The name of the variable.
*   @param varValue The value of the variable.
*   @param functor The function to call when the server response is received.
*   @note The functor is called with the server response as a parameter.
*   @example ForSetVar("MYVAL", 1, function(responseText) { console.log(responseText); });
*
* - ForVar(varName, functor)
*   @param varName The name of the variable.
*   @param functor The function to call when the server response is received.
*   @note The functor is called with the server response as a parameter.
*   @example ForVar("DATE", function(text) { console.log(text); });
*
*/
CTextEmitter ActionUI_JavaScript();

#pragma endregion

#pragma region ActionUICSSSupport
/* CSS support for action buttons.
* @note Used within .CSS for @ref AddMenuItem()
*/
CTextEmitter ActionUI_CSS();

#pragma endregion



//END ActionUI
#pragma endregion

#pragma region UIUtils_JavaScript
/* @info: JavaScript functions for manipulating the UI
*  The following functions are available:
*  param idOrHd : id or HTMLElement: e.g: 'myId' or document.getElementById('myId')
*
* Functions:
*  VisibleElement(idOrHd, hide)
*  HideElement(idOrHd, hide)
*  DisableElement(idOrHd, disable)
*  SetElementInnerHTML(idOrHd, text)
*  SetElementChecked(idOrHd, boolValue)
*  LogDebug(txt)
*
* Strings:
*   Unit_cm  -> "cm"
*   Unit_s   -> "s"
*
*/
CTextEmitter UIUtils_JavaScript();

#pragma endregion

#pragma region Chart_JavaScript
/* JavaScript code for drawing a chart.
* The code is used by the CChart class.
*
* @member field LeftMargin
* @member field RightMargin
* @member field TopMargin
* @member field BottomMargin
* @member field GridColor
* @member field NoniusColor
* @member field DataColor
* @member field DataLineWidth
* @member field AxisNumberColor
* @member field XDataToString = function(x)
*
* @member function Clear()
* 
* @member function SetMargins(left, right, top, bottom)
* @member function SetHrzMargins(left, right)
* @member function SetVrtMargins(top, bottom)
*
*   mode: (combination of)
*    - ""         : no auto margins
*    - "label"    : has labels
*    - "numbers"  : has numbers
*    - "left"     : (only for vertical axis) left side
*    - "right"    : (only for vertical axis) right side
* @member function AutoMargins(hrzAxisMode, vrtAxisMode)
*
*
* @member function SetHrzAxis(start, end, step)
* @member function SetVrtAxis(start, end, step)
* @member function SwapXYAxis()
* 
* @member function ResetAxisColors()
* @member function SetAxisColors(noniusColor, axisNumberColor)
* @member function SetAxisColors(colorName)
* @member function ResetGridColor()
* @member function SetGridColor(gridColor)
* @member function SetGridColor(colorName)
* @member function DrawHrzGrid()
* @member function DrawVrtGrid()
* @member function DrawHrzAxisNumbers()
* @member function ResetDataColor()
* @member function SetDataColor(dataColor)
* 
*   etage: [-1, 0, 1] (default = 1)
*    - (-1) : inner grid, no numbers
*    - (0)  : on axis, no numbers
*    - (1)  : in combination with numbers
* 
*   leftSide: [true, false] (default = true);
* @member function DrawHrzAxisLabel(labelText, etage)
* @member function DrawVrtAxisNumbers(leftSide)
* @member function DrawVrtAxisLabel(labelText, leftSide, etage)
*
*   trackNumber [0 .. 3]
* @member function DrawEvent(trackNumber, x, txt, colorLine, colorBg)
* @member function DrawBoundaryBox(minValue, maxValue, fillColor, opacity)
* 
*  dataPoints Array of data points:
*        Each data point is an array with two values [x, y] or one value y.
* @member function SetDataPoints(dataPoints)
* @member function DrawCurve()
* @member function DrawDots()
*
*
* @example
* var Chart = CChart(canvasElement);
* Chart.SetDataPoints([[0, 0], [1, 3], [2, 2], [3, 1]]);
* Chart.SetHrzAxis(0, 10, 1);
* Chart.SetVrtAxis(0, 10, 1);
* Chart.DrawCurve();
*
*/
CTextEmitter Chart_JavaScript();

#pragma endregion


//END Functions
#pragma endregion



#pragma region Debug/Analysis - Functions

#pragma region DebugPrintConfig
/* List the HomeKit configuration to the serial port.
* @param pConfig The HomeKit configuration to print.
* @example

HomeKit Config switch  config_number:1
    Accessory switch id:1 config_number:1
        Service ACCESSORY_INFORMATION id:1 hidden:0 primary:1
          IDENTIFY "Identify"             = false
          MANUFACTURER "Manufacturer"     = "Holger Burkarth"
          MODEL "Model"                  = "ESP8266/ESP32"
          NAME "Name"                     = "SwitchSample"
          SERIAL_NUMBER "Serial Number"  = "0123456"
          FIRMWARE_REVISION "Firmware Revision"    = "1.0"

        Service SWITCH id:8 hidden:0 primary:1
          ON "On"         = false
          NAME "Name"     = "SwitchParam"
*/
inline void DebugPrintConfig(const homekit_server_config_t* pConfig)
{
  Serial.printf
  (
    "HomeKit Config %s  config_number:%d\n"
    , homekit_accessory_category_to_string(pConfig->category)
    , pConfig->config_number
  );
  ForEachAccessory(pConfig, [&](const homekit_accessory_t* pAccessory)
    {
      Serial.printf
      (
        "\tAccessory %s id:%d config_number:%d\n"
        , homekit_accessory_category_to_string(pAccessory->category)
        , pAccessory->id
        , pAccessory->config_number
      );
      ForEachService(pAccessory, [&](const homekit_service_t* pService)
        {
          Serial.printf
          (
            "\t\tService %s id:%d hidden:%d primary:%d\n"
            , homekit_service_identify_to_string(pService->type)
            , pService->id
            , pService->hidden
            , pService->primary
          );

          ForEachCharacteristic(pService, [&](const homekit_characteristic_t* pCharacteristic)
            {
              Serial.printf
              (
                "\t\t\t%16s \"%s\"\t = %s\n"
                , homekit_characteristic_identify_to_string(pCharacteristic->type)
                , pCharacteristic->description
                , to_string(pCharacteristic->value).c_str()
              );
            });

          Serial.println();
        });

      Serial.println();
    });
}

#pragma endregion


//END Debug/Analysis - Functions
#pragma endregion

//END Functions
#pragma endregion



#pragma region Epilog
} // namespace HBHomeKit
#pragma endregion
