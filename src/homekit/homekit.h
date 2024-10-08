#ifndef __HOMEKIT_H__
#define __HOMEKIT_H__

#include <homekit/types.h>

#ifdef __cplusplus
extern "C" {
  #endif

  typedef void* homekit_client_id_t;


  typedef enum
  {
    HOMEKIT_EVENT_SERVER_INITIALIZED,
    // Just accepted client connection
    HOMEKIT_EVENT_CLIENT_CONNECTED,
    // Pairing verification completed and secure session is established
    HOMEKIT_EVENT_CLIENT_VERIFIED,
    HOMEKIT_EVENT_CLIENT_DISCONNECTED,
    HOMEKIT_EVENT_PAIRING_ADDED,
    HOMEKIT_EVENT_PAIRING_REMOVED,
  } homekit_event_t;


  typedef struct
  {
    // Pointer to an array of homekit_accessory_t pointers.
    // Array should be terminated by a NULL pointer.
    const homekit_accessory_t** accessories;

    homekit_accessory_category_t category;

    // Used for Bonjour
  // Current configuration number. Required.
  // Must update when an accessory, service, or characteristic is added or removed on the accessory server.
  // Accessories must increment the config number after a firmware update.
  // This must have a range of 1-65535 and wrap to 1 when it overflows.
  // This value must persist across reboots, power cycles, etc.
    uint16_t config_number;

    // Password in format "111-23-456".
    // If password is not specified, a random password
    // will be used. In that case, a password_callback
    // field must contain pointer to a function that should
    // somehow communicate password to the user (e.g. display
    // it on a screen if accessory has one).
    const char* password;
    void (*password_callback)(const char* password);

    // Setup ID in format "XXXX" (where X is digit or Latin capital letter)
    // Used for pairing using QR code
    char* setupId;

    // Callback for "POST /resource" to get snapshot image from camera
    void (*on_resource)(const char* body, size_t body_size);

    void (*on_event)(homekit_event_t event);
  } homekit_server_config_t;

  // Get pairing URI
  int homekit_get_setup_uri(const homekit_server_config_t* config,
    char* buffer, size_t buffer_size);

  // Initialize HomeKit accessory server
  void homekit_server_init(const homekit_server_config_t* config);

  // Reset HomeKit accessory server, removing all pairings
  void homekit_server_reset();

  int  homekit_get_accessory_id(char* buffer, size_t size);
  bool homekit_is_paired();

  // Client related stuff
  homekit_client_id_t homekit_get_client_id();

  bool homekit_client_is_admin();
  int  homekit_client_send(unsigned char* data, size_t size);

  #ifdef __cplusplus
}
#endif

#endif // __HOMEKIT_H__
