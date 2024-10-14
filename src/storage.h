#pragma region Prolog
#ifndef __STORAGE_H__
#define __STORAGE_H__
/*******************************************************************
$CRT 13 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>storage.h<< 13 Sep 2024  15:21:17 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Includes
//#include "EEPROM.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "pairing.h"

#pragma endregion

/* Read and write functions for the common storage area.
* The common storage area is used out of homekit.
* @note offset+size must be less than 4096
*/
bool homekit_storage_common_read(uint8_t pageIndex, size_t offset, uint8_t* pBuf, size_t size);
bool homekit_storage_common_write(uint8_t pageIndex, size_t offset, const uint8_t* pBuf, size_t size);

int homekit_storage_reset();
int homekit_storage_init();

void homekit_storage_save_accessory_id(const char* accessory_id);
int homekit_storage_load_accessory_id(char* data);

void homekit_storage_save_accessory_key(const ed25519_key* key);
int homekit_storage_load_accessory_key(ed25519_key* key);

bool homekit_storage_can_add_pairing();
int homekit_storage_add_pairing(const char* device_id, const ed25519_key* device_key, byte permissions);
int homekit_storage_update_pairing(const char* device_id, byte permissions);
int homekit_storage_remove_pairing(const char* device_id);
int homekit_storage_find_pairing(const char* device_id, pairing_t* pairing);

typedef struct
{
  int idx;
} pairing_iterator_t;


inline void homekit_storage_pairing_iterator_init(pairing_iterator_t* it)
{
  it->idx = 0;
}
inline void homekit_storage_pairing_iterator_done(pairing_iterator_t* iterator)
{
}

int homekit_storage_next_pairing(pairing_iterator_t* it, pairing_t* pairing);


#pragma region Epilog
#ifdef __cplusplus
}
#endif
#endif // __STORAGE_H__

#pragma endregion
