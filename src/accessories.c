#pragma region Prolog
/*******************************************************************
$CRT 12 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>accessories.c<< 12 Sep 2024  10:19:26 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Includes
#include <stdlib.h>
#include <string.h>
#include <homekit/types.h>
#include <homekit/characteristics.h>

#pragma endregion

#pragma region misc - functions

static size_t align_size(size_t size)
{
  if(size % sizeof(void*))
  {
    size += sizeof(void*) - size % sizeof(void*);
  }
  return size;
}

static void* align_pointer(void* ptr)
{
  uintptr_t p = (uintptr_t)ptr;
  if(p % sizeof(void*))
  {
    p += sizeof(void*) - p % sizeof(void*);
  }
  return (void*)p;
}

#pragma endregion

#pragma region value - functions
bool homekit_value_equal(const homekit_value_t* a, const homekit_value_t* b)
{
  if(a->is_null != b->is_null)
    return false;

  if(a->format != b->format)
    return false;

  switch(a->format)
  {
    case homekit_format_bool:
      return a->bool_value == b->bool_value;
    case homekit_format_uint8:
    case homekit_format_uint16:
    case homekit_format_uint32:
    case homekit_format_uint64:
    case homekit_format_int:
      return a->int_value == b->int_value;
    case homekit_format_float:
      return a->float_value == b->float_value;
    case homekit_format_string:
      return !strcmp(a->string_value, b->string_value);
    case homekit_format_tlv:
      {
        if(!a->tlv_values && !b->tlv_values)
          return true;
        if(!a->tlv_values || !b->tlv_values)
          return false;

        // TODO: implement order independent comparison?
        tlv_t* ta = a->tlv_values->head, * tb = b->tlv_values->head;
        for(; ta && tb; ta = ta->next, tb = tb->next)
        {
          if(ta->type != tb->type || ta->size != tb->size)
            return false;
          if(strncmp((char*)ta->value, (char*)tb->value, ta->size))
            return false;
        }

        return (!ta && !tb);
      }
    case homekit_format_data:
      if(!a->data_value && !b->data_value)
        return true;

      if(!a->data_value || !b->data_value)
        return false;

      if(a->data_size != b->data_size)
        return false;

      return !memcmp(a->data_value, b->data_value, a->data_size);
  }

  return false;
}

void homekit_value_copy(homekit_value_t* dst, const homekit_value_t* src)
{
  memset(dst, 0, sizeof(*dst));

  dst->format = src->format;
  dst->is_null = src->is_null;

  if(!src->is_null)
  {
    switch(src->format)
    {
      case homekit_format_bool:
        dst->bool_value = src->bool_value;
        break;
      case homekit_format_uint8:
      case homekit_format_uint16:
      case homekit_format_uint32:
      case homekit_format_uint64:
      case homekit_format_int:
        dst->int_value = src->int_value;
        break;
      case homekit_format_float:
        dst->float_value = src->float_value;
        break;
      case homekit_format_string:
        if(src->is_static)
        {
          dst->string_value = src->string_value;
          dst->is_static = true;
        }
        else
        {
          dst->string_value = strdup(src->string_value);
        }
        break;
      case homekit_format_tlv:
        {
          if(src->is_static)
          {
            dst->tlv_values = src->tlv_values;
            dst->is_static = true;
          }
          else
          {
            dst->tlv_values = tlv_new();
            for(tlv_t* v = src->tlv_values->head; v; v = v->next)
            {
              tlv_add_value(dst->tlv_values, v->type, v->value, v->size);
            }
          }
          break;
        }
      case homekit_format_data:
        if(src->is_static)
        {
          dst->data_value = src->data_value;
          dst->data_size = src->data_size;
          dst->is_static = true;
        }
        else
        {
          dst->data_size = src->data_size;
          dst->data_value = malloc(src->data_size);
          memcpy(dst->data_value, src->data_value, src->data_size);
        }
        break;
      default:
        // unknown format
        break;
    }
  }
}


homekit_value_t* homekit_value_clone(const homekit_value_t* value)
{
  homekit_value_t* copy = malloc(sizeof(homekit_value_t));
  homekit_value_copy(copy, value);
  return copy;
}

void homekit_value_destruct(homekit_value_t* value)
{
  if(!value->is_null)
  {
    switch(value->format)
    {
      case homekit_format_string:
        if(!value->is_static && value->string_value)
          free(value->string_value);
        break;
      case homekit_format_tlv:
        if(!value->is_static && value->tlv_values)
          tlv_free(value->tlv_values);
        break;
      case homekit_format_data:
        if(!value->is_static && value->data_value)
          free(value->data_value);
        break;
      default:
        // unknown format
        break;
    }
  }
}

void homekit_value_free(homekit_value_t* value)
{
  homekit_value_destruct(value);
  free(value);
}

#pragma endregion

#pragma region characteristic - functions
homekit_characteristic_t* homekit_characteristic_clone(homekit_characteristic_t* ch)
{
  size_t type_len = strlen(ch->type) + 1;
  size_t description_len = ch->description ? strlen(ch->description) + 1 : 0;

  size_t size = align_size(sizeof(homekit_characteristic_t) + type_len + description_len);

  if(ch->min_value)
    size += sizeof(float);
  if(ch->max_value)
    size += sizeof(float);
  if(ch->min_step)
    size += sizeof(float);
  if(ch->max_len)
    size += sizeof(int);
  if(ch->max_data_len)
    size += sizeof(int);
  if(ch->valid_values.count)
    size += align_size(sizeof(uint8_t) * ch->valid_values.count);
  if(ch->valid_values_ranges.count)
    size += align_size(sizeof(homekit_valid_values_range_t) * ch->valid_values_ranges.count);
  if(ch->callback)
  {
    homekit_characteristic_change_callback_t* c = ch->callback;
    while(c)
    {
      size += align_size(sizeof(homekit_characteristic_change_callback_t));
      c = c->next;
    }
  }

  uint8_t* p = calloc(1, size);

  homekit_characteristic_t* clone = (homekit_characteristic_t*)p;
  p += sizeof(homekit_characteristic_t);

  clone->service = ch->service;
  clone->id = ch->id;
  clone->type = (char*)p;
  strncpy((char*)p, ch->type, type_len);
  p[type_len - 1] = 0;
  p += type_len;

  clone->description = (char*)p;
  strncpy((char*)p, ch->description, description_len);
  p[description_len - 1] = 0;
  p += description_len;

  p = align_pointer(p);

  clone->format = ch->format;
  clone->unit = ch->unit;
  clone->permissions = ch->permissions;
  homekit_value_copy(&clone->value, &ch->value);

  if(ch->min_value)
  {
    clone->min_value = (float*)p;
    *(float*)p = *ch->min_value;
    p += sizeof(float);
  }

  if(ch->max_value)
  {
    clone->max_value = (float*)p;
    *(float*)p = *ch->max_value;
    p += sizeof(float);
  }

  if(ch->min_step)
  {
    clone->min_step = (float*)p;
    *(float*)p = *ch->min_step;
    p += sizeof(float);
  }

  if(ch->max_len)
  {
    clone->max_len = (int*)p;
    *(int*)p = *ch->max_len;
    p += sizeof(int);
  }

  if(ch->max_data_len)
  {
    clone->max_data_len = (int*)p;
    *(int*)p = *ch->max_data_len;
    p += sizeof(int);
  }

  if(ch->valid_values.count)
  {
    clone->valid_values.count = ch->valid_values.count;
    clone->valid_values.values = (uint8_t*)p;
    memcpy
    (
      p,//clone->valid_values.values,
      ch->valid_values.values,
      sizeof(uint8_t) * ch->valid_values.count
    );

    p += align_size(sizeof(uint8_t) * ch->valid_values.count);
  }

  if(ch->valid_values_ranges.count)
  {
    int c = ch->valid_values_ranges.count;
    clone->valid_values_ranges.count = c;
    clone->valid_values_ranges.ranges = (homekit_valid_values_range_t*)p;
    memcpy
    (
      p, //clone->valid_values_ranges.ranges,
      ch->valid_values_ranges.ranges,
      sizeof(homekit_valid_values_range_t*) * c
    );

    p += align_size(sizeof(homekit_valid_values_range_t*) * c);
  }

  if(ch->callback)
  {
    int c = 1;
    homekit_characteristic_change_callback_t* callback_in = ch->callback;
    clone->callback = (homekit_characteristic_change_callback_t*)p;

    homekit_characteristic_change_callback_t* callback_out = clone->callback;
    memcpy(callback_out, callback_in, sizeof(*callback_out));

    while(callback_in->next)
    {
      callback_in = callback_in->next;
      callback_out->next = callback_out + 1;
      callback_out = callback_out->next;
      memcpy(callback_out, callback_in, sizeof(*callback_out));
      c++;
    }
    callback_out->next = NULL;

    p += align_size(sizeof(homekit_characteristic_change_callback_t)) * c;
  }

  clone->getter = ch->getter;
  clone->setter = ch->setter;
  clone->getter_ex = ch->getter_ex;
  clone->setter_ex = ch->setter_ex;
  clone->context = ch->context;

  return clone;
}

homekit_service_t* homekit_service_clone(homekit_service_t* service)
{
  size_t type_len = strlen(service->type) + 1;
  size_t size = align_size(sizeof(homekit_service_t) + type_len);

  if(service->linked)
  {
    int i = 0;
    while(service->linked[i])
      i++;

    size += sizeof(homekit_service_t*) * (i + 1);
  }
  if(service->characteristics)
  {
    int i = 0;
    while(service->characteristics[i])
      i++;

    size += sizeof(homekit_characteristic_t*) * (i + 1);
  }

  uint8_t* p = calloc(1, size);
  homekit_service_t* clone = (homekit_service_t*)p;
  p += sizeof(homekit_service_t);
  clone->accessory = service->accessory;
  clone->id = service->id;
  clone->type = strncpy((char*)p, service->type, type_len);
  p[type_len - 1] = 0;
  p += align_size(type_len);

  clone->hidden = service->hidden;
  clone->primary = service->primary;

  if(service->linked)
  {
    clone->linked = (homekit_service_t**)p;
    int i = 0;
    while(service->linked[i])
    {
      clone->linked[i] = service->linked[i];
      i++;
    }
    clone->linked[i] = NULL;
    p += (i + 1) * sizeof(homekit_service_t*);
  }

  if(service->characteristics)
  {
    clone->characteristics = (homekit_characteristic_t**)p;
    int i = 0;
    while(service->characteristics[i])
    {
      clone->characteristics[i] = service->characteristics[i];
      i++;
    }
    clone->characteristics[i] = NULL;
    p += (i + 1) * sizeof(homekit_characteristic_t*);
  }

  return clone;
}

homekit_accessory_t* homekit_accessory_clone(homekit_accessory_t* ac)
{
  size_t size = sizeof(homekit_accessory_t);
  if(ac->services)
  {
    for(int i = 0; ac->services[i]; i++)
    {
      size += sizeof(homekit_service_t*);
    }
    size += sizeof(homekit_service_t*);
  }

  uint8_t* p = calloc(1, size);
  homekit_accessory_t* clone = (homekit_accessory_t*)p;
  p += align_size(sizeof(homekit_accessory_t));

  clone->id = ac->id;
  clone->category = ac->category;
  clone->config_number = ac->config_number;

  if(ac->services)
  {
    clone->services = (homekit_service_t**)p;
    int i = 0;
    while(ac->services[i])
    {
      clone->services[i] = ac->services[i];
      i++;
    }
    clone->services[i] = NULL;

    p += sizeof(homekit_service_t*) * (i + 1);
  }

  return clone;
}


homekit_value_t homekit_characteristic_ex_old_getter(const homekit_characteristic_t* ch)
{
  return ch->getter();
}


void homekit_characteristic_ex_old_setter(homekit_characteristic_t* ch, homekit_value_t value)
{
  ch->setter(value);
}

void homekit_accessories_init(const homekit_accessory_t** accessories)
{
  uint32_t aid = 1;
  for(homekit_accessory_t** accessory_it = (homekit_accessory_t**)accessories; *accessory_it; accessory_it++)
  {
    homekit_accessory_t* accessory = *accessory_it;
    if(accessory->id)
    {
      if(accessory->id >= aid)
        aid = accessory->id + 1;
    }
    else
    {
      accessory->id = aid++;
    }
    uint32_t iid = 1;
    for(homekit_service_t** service_it = accessory->services; *service_it; service_it++)
    {
      homekit_service_t* service = *service_it;
      service->accessory = accessory;
      if(service->id)
      {
        if(service->id >= iid)
          iid = service->id + 1;
      }
      else
      {
        service->id = iid++;
      }
      for(homekit_characteristic_t** ch_it = service->characteristics; *ch_it; ch_it++)
      {
        homekit_characteristic_t* ch = *ch_it;
        ch->service = service;
        if(ch->id)
        {
          if(ch->id >= iid)
            iid = ch->id + 1;
        }
        else
        {
          ch->id = iid++;
        }

        if(!ch->getter_ex && ch->getter)
        {
          ch->getter_ex = homekit_characteristic_ex_old_getter;
        }

        if(!ch->setter_ex && ch->setter)
        {
          ch->setter_ex = homekit_characteristic_ex_old_setter;
        }

        ch->value.format = ch->format;
      }
    }
  }
}

homekit_accessory_t* homekit_accessory_by_id(const homekit_accessory_t** accessories, uint32_t aid)
{
  for(homekit_accessory_t** accessory_it = (homekit_accessory_t**)accessories; *accessory_it; accessory_it++)
  {
    homekit_accessory_t* accessory = *accessory_it;

    if(accessory->id == aid)
      return accessory;
  }

  return NULL;
}

homekit_service_t* homekit_service_by_type(homekit_accessory_t* accessory, const char* type)
{
  for(homekit_service_t** service_it = accessory->services; *service_it; service_it++)
  {
    homekit_service_t* service = *service_it;

    if(!strcmp(service->type, type))
      return service;
  }

  return NULL;
}

homekit_characteristic_t* homekit_service_characteristic_by_type(homekit_service_t* service, const char* type)
{
  for(homekit_characteristic_t** ch_it = service->characteristics; *ch_it; ch_it++)
  {
    homekit_characteristic_t* ch = *ch_it;

    if(!strcmp(ch->type, type))
      return ch;
  }

  return NULL;
}

homekit_characteristic_t* homekit_characteristic_by_aid_and_iid(const homekit_accessory_t** accessories, uint32_t aid, uint32_t iid)
{
  for(homekit_accessory_t** accessory_it = (homekit_accessory_t**)accessories; *accessory_it; accessory_it++)
  {
    homekit_accessory_t* accessory = *accessory_it;

    if(accessory->id != aid)
      continue;

    for(homekit_service_t** service_it = accessory->services; *service_it; service_it++)
    {
      homekit_service_t* service = *service_it;

      for(homekit_characteristic_t** ch_it = service->characteristics; *ch_it; ch_it++)
      {
        homekit_characteristic_t* ch = *ch_it;

        if(ch->id == iid)
          return ch;
      }
    }
  }

  return NULL;
}


homekit_characteristic_t* homekit_characteristic_find_by_type(homekit_accessory_t** accessories, uint32_t aid, const char* type)
{
  for(homekit_accessory_t** accessory_it = accessories; *accessory_it; accessory_it++)
  {
    homekit_accessory_t* accessory = *accessory_it;

    if(accessory->id != aid)
      continue;

    for(homekit_service_t** service_it = accessory->services; *service_it; service_it++)
    {
      homekit_service_t* service = *service_it;

      for(homekit_characteristic_t** ch_it = service->characteristics; *ch_it; ch_it++)
      {
        homekit_characteristic_t* ch = *ch_it;

        if(!strcmp(ch->type, type))
          return ch;
      }
    }
  }

  return NULL;
}


void homekit_characteristic_notify(homekit_characteristic_t* ch, homekit_value_t value)
{
  homekit_characteristic_change_callback_t* callback = ch->callback;
  //printf("homekit_characteristic_notify: %s  %p\n", ch->type, callback);
  while(callback)
  {
    callback->function(ch, value, callback->context);
    callback = callback->next;
  }
}


void homekit_characteristic_add_notify_callback(
  homekit_characteristic_t* ch,
  homekit_characteristic_change_callback_fn function,
  void* context)
{
  homekit_characteristic_change_callback_t* new_callback = malloc(sizeof(homekit_characteristic_change_callback_t));
  new_callback->function = function;
  new_callback->context = context;
  new_callback->next = NULL;

  if(!ch->callback)
  {
    ch->callback = new_callback;
  }
  else
  {
    homekit_characteristic_change_callback_t* callback = ch->callback;
    if(callback->function == function && callback->context == context)
    {
      free(new_callback);
      return;
    }

    while(callback->next)
    {
      if(callback->next->function == function && callback->next->context == context)
      {
        free(new_callback);
        return;
      }
      callback = callback->next;
    }

    callback->next = new_callback;
  }
}


void homekit_characteristic_remove_notify_callback(
  homekit_characteristic_t* ch,
  homekit_characteristic_change_callback_fn function,
  void* context)
{
  while(ch->callback)
  {
    if(ch->callback->function != function || ch->callback->context != context)
    {
      break;
    }

    homekit_characteristic_change_callback_t* c = ch->callback;
    ch->callback = ch->callback->next;
    free(c);
  }

  if(!ch->callback)
    return;

  homekit_characteristic_change_callback_t* callback = ch->callback;
  while(callback->next)
  {
    if(callback->next->function == function && callback->next->context == context)
    {
      homekit_characteristic_change_callback_t* c = callback->next;
      callback->next = callback->next->next;
      free(c);
    }
    else
    {
      callback = callback->next;
    }
  }
}


// Removes particular callback from all characteristics
void homekit_accessories_clear_notify_callbacks(
  const homekit_accessory_t** accessories,
  homekit_characteristic_change_callback_fn function,
  void* context)
{
  for(homekit_accessory_t** accessory_it = (homekit_accessory_t**)accessories; *accessory_it; accessory_it++)
  {
    homekit_accessory_t* accessory = *accessory_it;

    for(homekit_service_t** service_it = accessory->services; *service_it; service_it++)
    {
      homekit_service_t* service = *service_it;

      for(homekit_characteristic_t** ch_it = service->characteristics; *ch_it; ch_it++)
      {
        homekit_characteristic_t* ch = *ch_it;

        homekit_characteristic_remove_notify_callback(ch, function, context);
      }
    }
  }
}


bool homekit_characteristic_has_notify_callback(
  const homekit_characteristic_t* ch,
  homekit_characteristic_change_callback_fn function,
  void* context)
{
  homekit_characteristic_change_callback_t* callback = ch->callback;
  while(callback)
  {
    if(callback->function == function && callback->context == context)
      return true;

    callback = callback->next;
  }

  return false;
}


#pragma endregion

#pragma region to string - functions

#pragma region homekit_accessory_category_to_string
#define ALL_ACCESSORY_CATEGORY(_t) \
_t(other) \
_t(bridge) \
_t(fan) \
_t(garage) \
_t(lightbulb) \
_t(door_lock) \
_t(outlet) \
_t(switch) \
_t(thermostat) \
_t(sensor) \
_t(security_system) \
_t(door) \
_t(window) \
_t(window_covering) \
_t(programmable_switch) \
_t(range_extender) \
_t(ip_camera) \
_t(video_door_bell) \
_t(air_purifier) \
_t(heater) \
_t(air_conditioner) \
_t(humidifier) \
_t(dehumidifier) \
_t(apple_tv) \
_t(speaker) \
_t(airport) \
_t(sprinkler) \
_t(faucet) \
_t(shower_head) \
_t(television) \
_t(target_controller) \

static const char* accessory_category_table[] =
{
  "NULL", // first : homekit_accessory_category_other = 1
  #define _t(name) #name,
  ALL_ACCESSORY_CATEGORY(_t)
  #undef _t
};

const char* homekit_accessory_category_to_string(homekit_accessory_category_t cat)
{
  if(cat >= 0 && cat < sizeof(accessory_category_table) / sizeof(accessory_category_table[0]))
    return accessory_category_table[cat];

  return "unknown";
}


#pragma endregion

#pragma region homekit_characteristic_identify_to_string

#define ALL_CHARACTERISTIC_IDENTIFY(_t) \
_t(ADMINISTRATOR_ONLY_ACCESS) \
_t(AUDIO_FEEDBACK) \
_t(BRIGHTNESS) \
_t(COOLING_THRESHOLD_TEMPERATURE) \
_t(CURRENT_DOOR_STATE) \
_t(CURRENT_HEATING_COOLING_STATE) \
_t(CURRENT_RELATIVE_HUMIDITY) \
_t(CURRENT_TEMPERATURE) \
_t(FIRMWARE_REVISION) \
_t(HARDWARE_REVISION) \
_t(HEATING_THRESHOLD_TEMPERATURE) \
_t(HUE) \
_t(IDENTIFY) \
_t(LOCK_CONTROL_POINT) \
_t(LOCK_CURRENT_STATE) \
_t(LOCK_LAST_KNOWN_ACTION) \
_t(LOCK_MANAGEMENT_AUTO_SECURITY_TIMEOUT) \
_t(LOCK_TARGET_STATE) \
_t(LOGS) \
_t(MANUFACTURER) \
_t(MODEL) \
_t(MOTION_DETECTED) \
_t(NAME) \
_t(OBSTRUCTION_DETECTED) \
_t(ON) \
_t(OUTLET_IN_USE) \
_t(ROTATION_DIRECTION) \
_t(ROTATION_SPEED) \
_t(SATURATION) \
_t(SERIAL_NUMBER) \
_t(TARGET_DOOR_STATE) \
_t(TARGET_HEATING_COOLING_STATE) \
_t(TARGET_RELATIVE_HUMIDITY) \
_t(TARGET_TEMPERATURE) \
_t(TEMPERATURE_DISPLAY_UNITS) \
_t(VERSION) \
_t(AIR_PARTICULATE_DENSITY) \
_t(AIR_PARTICULATE_SIZE) \
_t(SECURITY_SYSTEM_CURRENT_STATE) \
_t(SECURITY_SYSTEM_TARGET_STATE) \
_t(BATTERY_LEVEL) \
_t(CONTACT_SENSOR_STATE) \
_t(CURRENT_AMBIENT_LIGHT_LEVEL) \
_t(CURRENT_HORIZONTAL_TILT_ANGLE) \
_t(CURRENT_POSITION) \
_t(CURRENT_VERTICAL_TILT_ANGLE) \
_t(HOLD_POSITION) \
_t(LEAK_DETECTED) \
_t(OCCUPANCY_DETECTED) \
_t(POSITION_STATE) \
_t(PROGRAMMABLE_SWITCH_EVENT) \
_t(STATUS_ACTIVE) \
_t(SMOKE_DETECTED) \
_t(STATUS_FAULT) \
_t(STATUS_JAMMED) \
_t(STATUS_LOW_BATTERY) \
_t(STATUS_TAMPERED) \
_t(TARGET_HORIZONTAL_TILT_ANGLE) \
_t(TARGET_POSITION) \
_t(TARGET_VERTICAL_TILT_ANGLE) \
_t(SECURITY_SYSTEM_ALARM_TYPE) \
_t(CHARGING_STATE) \
_t(CARBON_MONOXIDE_DETECTED) \
_t(CARBON_MONOXIDE_LEVEL) \
_t(CARBON_MONOXIDE_PEAK_LEVEL) \
_t(CARBON_DIOXIDE_DETECTED) \
_t(CARBON_DIOXIDE_LEVEL) \
_t(CARBON_DIOXIDE_PEAK_LEVEL) \
_t(AIR_QUALITY) \
_t(STREAMING_STATUS) \
_t(SUPPORTED_VIDEO_STREAM_CONFIGURATION) \
_t(SUPPORTED_AUDIO_STREAM_CONFIGURATION) \
_t(SUPPORTED_RTP_CONFIGURATION) \
_t(SETUP_ENDPOINTS) \
_t(SELECTED_RTP_STREAM_CONFIGURATION) \
_t(VOLUME) \
_t(MUTE) \
_t(NIGHT_VISION) \
_t(OPTICAL_ZOOM) \
_t(DIGITAL_ZOOM) \
_t(IMAGE_ROTATION) \
_t(IMAGE_MIRRORING) \
_t(ACCESSORY_FLAGS) \
_t(LOCK_PHYSICAL_CONTROLS) \
_t(CURRENT_AIR_PURIFIER_STATE) \
_t(CURRENT_SLAT_STATE) \
_t(SLAT_TYPE) \
_t(FILTER_LIFE_LEVEL) \
_t(FILTER_CHANGE_INDICATION) \
_t(RESET_FILTER_INDICATION) \
_t(TARGET_AIR_PURIFIER_STATE) \
_t(TARGET_FAN_STATE) \
_t(CURRENT_FAN_STATE) \
_t(ACTIVE) \
_t(SWING_MODE) \
_t(CURRENT_TILT_ANGLE) \
_t(TARGET_TILT_ANGLE) \
_t(OZONE_DENSITY) \
_t(NITROGEN_DIOXIDE_DENSITY) \
_t(SULPHUR_DIOXIDE_DENSITY) \
_t(PM25_DENSITY) \
_t(PM10_DENSITY) \
_t(VOC_DENSITY) \
_t(SERVICE_LABEL_INDEX) \
_t(SERVICE_LABEL_NAMESPACE) \
_t(COLOR_TEMPERATURE) \
_t(IN_USE) \
_t(IS_CONFIGURED) \
_t(PROGRAM_MODE) \
_t(REMAINING_DURATION) \
_t(SET_DURATION) \
_t(VALVE_TYPE) \
_t(CURRENT_HEATER_COOLER_STATE) \
_t(TARGET_HEATER_COOLER_STATE) \
_t(CURRENT_HUMIDIFIER_DEHUMIDIFIER_STATE) \
_t(TARGET_HUMIDIFIER_DEHUMIDIFIER_STATE) \
_t(WATER_LEVEL) \
_t(RELATIVE_HUMIDITY_DEHUMIDIFIER_THRESHOLD) \
_t(RELATIVE_HUMIDITY_HUMIDIFIER_THRESHOLD) \
_t(ACTIVE_IDENTIFIER) \
_t(CONFIGURED_NAME) \
_t(SLEEP_DISCOVERY_MODE) \
_t(CLOSED_CAPTIONS) \
_t(DISPLAY_ORDER) \
_t(CURRENT_MEDIA_STATE) \
_t(TARGET_MEDIA_STATE) \
_t(PICTURE_MODE) \
_t(POWER_MODE_SELECTION) \
_t(REMOTE_KEY) \
_t(INPUT_SOURCE_TYPE) \
_t(INPUT_DEVICE_TYPE) \
_t(IDENTIFIER) \
_t(CURRENT_VISIBILITY_STATE) \
_t(TARGET_VISIBILITY_STATE) \
_t(VOLUME_CONTROL_TYPE) \
_t(VOLUME_SELECTOR) \




static struct { const char* Type; const char* Text; } characteristic_identify_table[] =
{
  #define _t(name) { HOMEKIT_CHARACTERISTIC_ ## name, #name },
  ALL_CHARACTERISTIC_IDENTIFY(_t)
  #undef _t
};

const char* homekit_characteristic_identify_to_string(const char* type)
{
  if(type && type[0])
  {
    for(int i = 0; i < sizeof(characteristic_identify_table) / sizeof(characteristic_identify_table[0]); ++i)
    {
      if(strcmp(characteristic_identify_table[i].Type, type) == 0)
        return characteristic_identify_table[i].Text;
    }
  }
  return "unknown";
}

#pragma endregion

#pragma region homekit_service_identify_to_string

#define ALL_SERVICE_IDENTIFY(_t) \
_t(ACCESSORY_INFORMATION) \
_t(FAN) \
_t(GARAGE_DOOR_OPENER) \
_t(LIGHTBULB) \
_t(LOCK_MANAGEMENT) \
_t(LOCK_MECHANISM) \
_t(OUTLET) \
_t(SWITCH) \
_t(THERMOSTAT) \
_t(AIR_QUALITY_SENSOR) \
_t(SECURITY_SYSTEM) \
_t(CARBON_MONOXIDE_SENSOR) \
_t(CONTACT_SENSOR) \
_t(DOOR) \
_t(HUMIDITY_SENSOR) \
_t(LEAK_SENSOR) \
_t(LIGHT_SENSOR) \
_t(MOTION_SENSOR) \
_t(OCCUPANCY_SENSOR) \
_t(SMOKE_SENSOR) \
_t(STATELESS_PROGRAMMABLE_SWITCH) \
_t(TEMPERATURE_SENSOR) \
_t(WINDOW) \
_t(WINDOW_COVERING) \
_t(BATTERY_SERVICE) \
_t(CARBON_DIOXIDE_SENSOR) \
_t(CAMERA_RTP_STREAM_MANAGEMENT) \
_t(MICROPHONE) \
_t(SPEAKER) \
_t(DOORBELL) \
_t(FAN2) \
_t(SLAT) \
_t(FILTER_MAINTENANCE) \
_t(AIR_PURIFIER) \
_t(SERVICE_LABEL) \
_t(FAUCET) \
_t(IRRIGATION_SYSTEM) \
_t(VALVE) \
_t(HEATER_COOLER) \
_t(HUMIDIFIER_DEHUMIDIFIER) \
_t(TELEVISION) \
_t(INPUT_SOURCE) \
_t(TELEVISION_SPEAKER) \


static struct { const char* Type; const char* Text; } service_identify_table[] =
{
  #define _t(name) { HOMEKIT_SERVICE_ ## name, #name },
  ALL_SERVICE_IDENTIFY(_t)
  #undef _t
};

const char* homekit_service_identify_to_string(const char* type)
{
  if(type && type[0])
  {
    for(int i = 0; i < sizeof(service_identify_table) / sizeof(service_identify_table[0]); ++i)
    {
      if(strcmp(service_identify_table[i].Type, type) == 0)
        return service_identify_table[i].Text;
    }
  }
  return "unknown";
}
#pragma endregion


//END to string - functions
#pragma endregion
