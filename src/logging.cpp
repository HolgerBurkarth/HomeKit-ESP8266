#pragma region Prolog
/*******************************************************************
$CRT 10 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>logging.cpp<< 20 Sep 2024  08:29:24 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Includes
#include <Arduino.h>
#include "homekit_debug.h"
#include "hb_homekit.h"

namespace HBHomeKit
{

#pragma endregion

#pragma region +Local Namespace
namespace
{

enum class LogLevel
{
  Verbose,
  Info,
  Warn,
  Error
};

#pragma region GetStaticTimeStampText
const char* GetStaticTimeStampText()
{
  static char TimeStamp[32];
  struct tm timeinfo;
  if(!Safe_GetLocalTime(&timeinfo))
    return "\t";
  else
  {
    strftime(TimeStamp, sizeof(TimeStamp), "%Y-%m-%d %H:%M:%S\t", &timeinfo);
    return TimeStamp;
  }
}

#pragma endregion

#pragma region LogVPrintf_P
void LogVPrintf_P(LogLevel level, PGM_P format, va_list args)
{
  char buf[256];
  size_t len = vsnprintf_P(buf, sizeof(buf), format, args);
  buf[sizeof(buf) - 1] = 0;

  bool UseLog = gbLogging && level != LogLevel::Verbose;
  const char* DateText = UseLog ? GetStaticTimeStampText() : "\t";
  const char* PrefixText;

  switch(level)
  {
    default:
    case LogLevel::Verbose:
      PrefixText = "\t";
      break;
    case LogLevel::Info:
      PrefixText = "Info\t";
      break;
    case LogLevel::Warn:
      PrefixText = "Warn\t";
      break;
    case LogLevel::Error:
      PrefixText = "Error\t";
      break;
  }

  Serial.print(PrefixText);
  if(UseLog)
    gbLogging->Add(PrefixText);

  Serial.print(DateText);
  if(UseLog)
    gbLogging->Add(DateText);

  Serial.println(buf);

  if(UseLog)
    gbLogging->AddLine(buf);
}

#pragma endregion


} // namespace
//END Local Namespace
#pragma endregion

#pragma region InstallLogging
ILogging_Ptr gbLogging;

void InstallLogging(ILogging_Ptr logging)
{
  gbLogging = std::move(logging);
  if(gbLogging)
  {
    // get esp restart reason and log it
    WARN("ESP Restart Reason: %s", ESP.getResetReason().c_str());
  }
}

void InstallLogging()
{
  InstallLogging(std::make_unique<CLogging<>>());
}


#pragma endregion

#pragma region GetLoggingEnumerator
ILoggingEnumerator_Ptr GetLoggingEnumerator()
{
  return gbLogging ? gbLogging->GetEnumerator() : nullptr;
}
#pragma endregion


#pragma region End of HBHomeKit
} // namespace HBHomeKit
using namespace HBHomeKit;

#pragma endregion

#pragma region VerbosePrintf_P
void VerbosePrintf_P(PGM_P format, ...)
{
  va_list args;
  va_start(args, format);
  LogVPrintf_P(LogLevel::Verbose, format, args);
  va_end(args);
}
#pragma endregion

#pragma region InfoPrintf_P
void InfoPrintf_P(PGM_P format, ...)
{
  va_list args;
  va_start(args, format);
  LogVPrintf_P(LogLevel::Info, format, args);
  va_end(args);
}
#pragma endregion

#pragma region WarnPrintf_P
void WarnPrintf_P(PGM_P format, ...)
{
  va_list args;
  va_start(args, format);
  LogVPrintf_P(LogLevel::Warn, format, args);
  va_end(args);
}

#pragma endregion

#pragma region ErrorPrintf_P
void ErrorPrintf_P(PGM_P format, ...)
{
  va_list args;
  va_start(args, format);
  LogVPrintf_P(LogLevel::Error, format, args);
  va_end(args);
}

#pragma endregion

