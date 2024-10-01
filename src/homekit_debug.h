#pragma region Prolog
#ifndef __HOMEKIT_DEBUG_H__
#define __HOMEKIT_DEBUG_H__
/*******************************************************************
$CRT 09 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>homekit_debug.h<< 30 Sep 2024  07:07:34 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Includes
#include <stdlib.h>
#include <stdio.h>
#include "Arduino.h"
#include <string.h>
#include <esp_xpgm.h>

#pragma endregion

#pragma region extern C
#ifdef __cplusplus
extern "C" {
  #endif

  #pragma region Types / Defs
  typedef unsigned char byte;

  //#define HOMEKIT_DEBUG

  #define HOMEKIT_PRINTF XPGM_PRINTF

  #pragma endregion

  #pragma region Log/Serial Printf Functions
  void VerbosePrintf_P(PGM_P format, ...);
  void InfoPrintf_P(PGM_P format, ...);
  void WarnPrintf_P(PGM_P format, ...);
  void ErrorPrintf_P(PGM_P format, ...);

  #pragma endregion

  #pragma region HOMEKIT_DEBUG dependent defines
  #ifdef HOMEKIT_DEBUG

  #define DEBUG(message, ...)  HOMEKIT_PRINTF(">>> %s: " message "\n", __func__, ##__VA_ARGS__)
  static uint32_t start_time = 0;
  #define DEBUG_TIME_BEGIN()  start_time=millis();
  #define DEBUG_TIME_END(func_name)  HOMEKIT_PRINTF("### [%7d] %s took %6dms\n", millis(), func_name, (millis() - start_time));

  #else

  #define DEBUG(message, ...)
  #define DEBUG_TIME_BEGIN()
  #define DEBUG_TIME_END(func_name)

  #endif

  #pragma endregion

  #pragma region Protocolling Functions/Defines
  /* Logging functions via Serial and ILogging
  * @note:
  *   Each printout is always to the serial.
  *   If a logging instance is also installed (@see InstallLogging()),
  *   INFO, WARN, and ERROR printouts are also included in the log.
  *
  * @note: Note that tabs act as separators for the text parts.
  * @example output format:
  *    |Info:\t\tWiFi connected, IP: 192.168.3.76|
  *    |Info:\t[2024-09-11 04:36:51]\tUsing existing accessory ID: 11:D7:19:95:D3:FC|
  *
  *      |
  *      v
  *
  * <tr> <th>Level</th> <th>   UTC Date Time     </th> <th>       Message                                </th></tr>
  * ---------------------------------------------------------------------------------------------------------------
  * <tr> <td>Info:</td> <td>                     </td> <td>WiFi connected, IP: 192.168.3.76              </td></tr>
  * <tr> <td>Info:</td> <td>[2024-09-11 04:36:51]</td> <td>Using existing accessory ID: 11:D7:19:95:D3:FC</td></tr>
  *
  */
  #define VERBOSE(fmt, ...) VerbosePrintf_P(PSTR(fmt), ##__VA_ARGS__)
  #define INFO(fmt, ...)    InfoPrintf_P(PSTR(fmt), ##__VA_ARGS__)
  #define WARN(fmt, ...)    WarnPrintf_P(PSTR(fmt), ##__VA_ARGS__)
  #define ERROR(fmt, ...)   ErrorPrintf_P(PSTR(fmt), ##__VA_ARGS__)

  #define INFO_HEAP()       VERBOSE("Free heap: %d", system_get_free_heap_size());
  #define DEBUG_HEAP()  DEBUG("Free heap: %d", system_get_free_heap_size());

  #pragma endregion

  #pragma region Misc Functions
  char* binary_to_string(const byte* data, size_t size);
  void print_binary(const char* prompt, const byte* data, size_t size);

  #pragma endregion

  #ifdef __cplusplus
}
#endif
//END extern C
#pragma endregion



#pragma region Epilog
#endif // __HOMEKIT_DEBUG_H__

#pragma endregion
