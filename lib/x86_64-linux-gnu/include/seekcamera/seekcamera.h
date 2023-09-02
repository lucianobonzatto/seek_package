//
// SEEK THERMAL CONFIDENTIAL
// _________________________
//
// Copyright (c) 2021 Seek Thermal Incorporated
// All Rights Reserved.
//
// NOTICE:  All information contained herein is, and remains
// the property of Seek Thermal Incorporated and its suppliers,
// if any.  The intellectual and technical concepts contained
// herein are proprietary to Seek Thermal Incorporated
// and its suppliers and may be covered by U.S. and Foreign Patents,
// patents in process, and are protected by trade secret or copyright law.
// Dissemination of this information or reproduction of this material
// is strictly forbidden unless prior written permission is obtained
// from Seek Thermal Incorporated.
//

#ifndef __SEEKCAMERA_H__
#define __SEEKCAMERA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "seekcamera/seekcamera_error.h"
#include "seekcamera/seekcamera_frame.h"

//-----------------------------------------------------------------------------
// Export
//-----------------------------------------------------------------------------
#ifndef SEEKCAMERA_API
#	ifdef __cplusplus
#		if defined(WIN32)
#			define SEEKCAMERA_API extern "C" __declspec(dllexport)
#		else
#			define SEEKCAMERA_API extern "C" __attribute__((__visibility__("default")))
#		endif
#	else
#		if defined(WIN32)
#			define SEEKCAMERA_API extern __declspec(dllexport)
#		else
#			define SEEKCAMERA_API extern __attribute__((__visibility__("default")))
#		endif
#	endif
#endif

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------
// Structure that represents a single Seek camera.
typedef struct seekcamera_t seekcamera_t;

// A camera chip identifier (CID) represented as a C-string.
// The type is also referred to as CID.
typedef char seekcamera_chipid_t[16];

// A camera serial number (SN) represented as a C-string.
typedef char seekcamera_serial_number_t[16];

// A camera core part number (CPN) represented as a C-string.
typedef char seekcamera_core_part_number_t[32];

// Enumerated type representing types of IO protocols supported.
typedef enum seekcamera_io_type_t
{
	SEEKCAMERA_IO_TYPE_USB = 0x01,
	SEEKCAMERA_IO_TYPE_SPI = 0x02,
} seekcamera_io_type_t;

// Structure that represents the firmware version of a Seek camera.
typedef struct seekcamera_firmware_version_t
{
	uint8_t product;
	uint8_t variant;
	uint8_t major;
	uint8_t minor;
} seekcamera_firmware_version_t;

// Types of application resource regions.
typedef enum seekcamera_app_resources_region_t
{
	SEEKCAMERA_APP_RESOURCES_REGION_0 = 11,
	SEEKCAMERA_APP_RESOURCES_REGION_1,
	SEEKCAMERA_APP_RESOURCES_REGION_2,
} seekcamera_app_resources_region_t;

// Callback function fired every time a memory access is performed.
// It is used for providing progress feedback for long memory operations.
typedef void (*seekcamera_memory_access_callback_t)(
	size_t progress,
	void* user_data);

// Callback function fired every time a frame is available.
typedef void (*seekcamera_frame_available_callback_t)(
	seekcamera_t* camera,
	seekcamera_frame_t* frame,
	void* user_data);

// Enumerated type representing types of display color palettes.
typedef enum seekcamera_color_palette_t
{
	SEEKCAMERA_COLOR_PALETTE_WHITE_HOT = 0,
	SEEKCAMERA_COLOR_PALETTE_BLACK_HOT,
	SEEKCAMERA_COLOR_PALETTE_SPECTRA,
	SEEKCAMERA_COLOR_PALETTE_PRISM,
	SEEKCAMERA_COLOR_PALETTE_TYRIAN,
	SEEKCAMERA_COLOR_PALETTE_IRON,
	SEEKCAMERA_COLOR_PALETTE_AMBER,
	SEEKCAMERA_COLOR_PALETTE_HI,
	SEEKCAMERA_COLOR_PALETTE_GREEN,
	SEEKCAMERA_COLOR_PALETTE_USER_0,
	SEEKCAMERA_COLOR_PALETTE_USER_1,
	SEEKCAMERA_COLOR_PALETTE_USER_2,
	SEEKCAMERA_COLOR_PALETTE_USER_3,
	SEEKCAMERA_COLOR_PALETTE_USER_4,
} seekcamera_color_palette_t;

// Structure that represents an entry in color palette data structure.
typedef struct seekcamera_color_palette_data_entry_t
{
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;
} seekcamera_color_palette_data_entry_t;

// An array of color values used to colorize a thermal image.
typedef seekcamera_color_palette_data_entry_t seekcamera_color_palette_data_t[256];

// Enumerated type representing types of automated gain correction (AGC) modes.
typedef enum seekcamera_agc_mode_t
{
	SEEKCAMERA_AGC_MODE_LINEAR = 0,
	SEEKCAMERA_AGC_MODE_HISTEQ
} seekcamera_agc_mode_t;

// Enumerated type representing types of shutter modes.
typedef enum seekcamera_shutter_mode_t
{
	SEEKCAMERA_SHUTTER_MODE_AUTO = 0,
	SEEKCAMERA_SHUTTER_MODE_MANUAL
} seekcamera_shutter_mode_t;

// Enumerated type representing types of temperature units.
typedef enum seekcamera_temperature_unit_t
{
	SEEKCAMERA_TEMPERATURE_UNIT_CELSIUS = 0,
	SEEKCAMERA_TEMPERATURE_UNIT_FAHRENHEIT,
	SEEKCAMERA_TEMPERATURE_UNIT_KELVIN
} seekcamera_temperature_unit_t;

// Structure that contains properties of USB cameras.
typedef struct seekcamera_usb_io_properties_t
{
	uint8_t bus_number;
	uint8_t port_numbers[8];
} seekcamera_usb_io_properties_t;

// Structure that contains properties of SPI cameras.
typedef struct seekcamera_spi_io_properties_t
{
	uint8_t bus_number;
	uint8_t cs_number;
} seekcamera_spi_io_properties_t;

// Generic structure that contains the IO properties of the camera.
typedef struct seekcamera_io_properties_t
{
	seekcamera_io_type_t type;
	union
	{
		seekcamera_usb_io_properties_t usb;
		seekcamera_spi_io_properties_t spi;
	} properties;
} seekcamera_io_properties_t;

// Enumerated type representing the controllable image processing filters.
typedef enum seekcamera_filter_t
{
	SEEKCAMERA_FILTER_GRADIENT_CORRECTION = 0,
	SEEKCAMERA_FILTER_FLAT_SCENE_CORRECTION,
} seekcamera_filter_t;

// Enumerated type representing the possible states of an image processing filter.
typedef enum seekcamera_filter_state_t
{
	SEEKCAMERA_FILTER_STATE_DISABLED = 0,
	SEEKCAMERA_FILTER_STATE_ENABLED,
} seekcamera_filter_state_t;

// Enumerated type representing a unique flat scene correction (FSC) identifier.
typedef enum seekcamera_flat_scene_correction_id_t
{
	SEEKCAMERA_FLAT_SCENE_CORRECTION_ID_0 = 0,
} seekcamera_flat_scene_correction_id_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------
// Gets the IO type of the camera.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_io_type(
	seekcamera_t* camera,
	seekcamera_io_type_t* type);

// Gets the IO properties of the camera.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_io_properties(
	seekcamera_t* camera,
	seekcamera_io_properties_t* io_properties);

// Gets the chip identifier (CID) of the camera.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_chipid(
	seekcamera_t* camera,
	seekcamera_chipid_t* chipid);

// Gets the serial number (SN) of the camera.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_serial_number(
	seekcamera_t* camera,
	seekcamera_serial_number_t* serial_number);

// Gets the core part number (CPN) of the camera.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_core_part_number(
	seekcamera_t* camera,
	seekcamera_core_part_number_t* core_part_number);

// Gets the firmware version of the camera.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_firmware_version(
	seekcamera_t* camera,
	seekcamera_firmware_version_t* version);

// Gets the thermography window of the camera.
// The thermography window is expressed in image coordinates.
// The global origin is the upper-left corner of the frame.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_thermography_window(
	seekcamera_t* camera,
	size_t* x0,
	size_t* y0,
	size_t* w,
	size_t* h);

// Sets the thermography window of the camera.
// The thermography window is expressed in image coordinates.
// The global origin is the upper-left corner of the frame.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_thermography_window(
	seekcamera_t* camera,
	size_t x0,
	size_t y0,
	size_t w,
	size_t h);

// Updates the camera firmware using an input firmware file on the host OS.
// An optional callback can be used to provide progress updates.
SEEKCAMERA_API seekcamera_error_t seekcamera_update_firmware(
	seekcamera_t* camera,
	const char* upgrade_file,
	seekcamera_memory_access_callback_t callback,
	void* user_data);

// Stores calibration data and pairs the camera.
// An optional callback can be used to provide progress updates.
SEEKCAMERA_API seekcamera_error_t seekcamera_store_calibration_data(
	seekcamera_t* camera,
	const char* source_dir,
	seekcamera_memory_access_callback_t callback,
	void* user_data);

// Stores a flat scene correction (FSC).
// The FSC is always stored to the host filesystem; it is also stored to the device if supported.
// An optional callback can be used to provide progress updates.
SEEKCAMERA_API seekcamera_error_t seekcamera_store_flat_scene_correction(
	seekcamera_t* camera,
	seekcamera_flat_scene_correction_id_t id,
	seekcamera_memory_access_callback_t callback,
	void* user_data);

// Deletes a flat scene correction (FSC).
// The FSC will be deleted from any location it was stored.
// An optional callback can be used to provide progress updates.
SEEKCAMERA_API seekcamera_error_t seekcamera_delete_flat_scene_correction(
	seekcamera_t* camera,
	seekcamera_flat_scene_correction_id_t id,
	seekcamera_memory_access_callback_t callback,
	void* user_data);

// Loads application resources into host memory.
// The source region may either be the camera internal memory or the SDK internal cache.
// Resources in each region must be <= 64KB.
// An optional callback can be used to provide progress updates.
SEEKCAMERA_API seekcamera_error_t seekcamera_load_app_resources(
	seekcamera_t* camera,
	seekcamera_app_resources_region_t region,
	void* data,
	size_t data_size,
	seekcamera_memory_access_callback_t callback,
	void* user_data);

// Stores application resources to either the host or the device.
// The source region is host memory.
// The destination region may either be the camera internal memory or the SDK internal cache.
// Resources in each region must be <= 64KB.
// An optional callback can be used to provide progress updates.
SEEKCAMERA_API seekcamera_error_t seekcamera_store_app_resources(
	seekcamera_t* camera,
	seekcamera_app_resources_region_t region,
	const void* data,
	size_t data_size,
	seekcamera_memory_access_callback_t callback,
	void* user_data);

// Begins streaming frames of the specified output formats from the camera.
SEEKCAMERA_API seekcamera_error_t seekcamera_capture_session_start(
	seekcamera_t* camera,
	uint32_t frame_format);

// Stops streaming frames from the camera.
SEEKCAMERA_API seekcamera_error_t seekcamera_capture_session_stop(
	seekcamera_t* camera);

// Registers a user-defined frame available callback function with the camera.
// The callback is fired every time a new frame is available.
SEEKCAMERA_API seekcamera_error_t seekcamera_register_frame_available_callback(
	seekcamera_t* camera,
	seekcamera_frame_available_callback_t callback,
	void* user_data);

// Gets the active color palette.
// Color palettes are used to colorize the image.
// Settings are refreshed between frames.
// This method can only be performed after a capture session has started.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_color_palette(
	seekcamera_t* camera,
	seekcamera_color_palette_t* palette);

// Sets the palette used to colorize the output images.
// Color palettes are used to colorize the image.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_color_palette(
	seekcamera_t* camera,
	seekcamera_color_palette_t palette);

// Sets the color palette data for a particular color palette.
// Setting color palette data is only valid for user palettes.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_color_palette_data(
	seekcamera_t* camera,
	seekcamera_color_palette_t palette,
	seekcamera_color_palette_data_t* palette_data);

// Gets the active AGC mode.
// Settings are refreshed between frames.
// This method can only be performed after a capture session has started.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_agc_mode(
	seekcamera_t* camera,
	seekcamera_agc_mode_t* mode);

// Sets the AGC mode.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_agc_mode(
	seekcamera_t* camera,
	seekcamera_agc_mode_t mode);

// Gets the active shutter mode.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_shutter_mode(
	seekcamera_t* camera,
	seekcamera_shutter_mode_t* mode);

// Sets the shutter mode.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_shutter_mode(
	seekcamera_t* camera,
	seekcamera_shutter_mode_t mode);

// Triggers the camera to shutter as soon as possible.
SEEKCAMERA_API seekcamera_error_t seekcamera_shutter_trigger(
	seekcamera_t* camera);

// Gets the active temperature unit.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_temperature_unit(
	seekcamera_t* camera,
	seekcamera_temperature_unit_t* unit);

// Sets the temperature unit.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_temperature_unit(
	seekcamera_t* camera,
	seekcamera_temperature_unit_t unit);

// Gets the global scene emissivity.
// Emissivity is the measure of an objects ability to emit thermal radiation.
// It may take on values in the closed interval [0,1] with floating point precision.
// Settings are refreshed between frames.
// This method can only be performed after a capture session has started.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_scene_emissivity(
	seekcamera_t* camera,
	float* emissivity);

// Sets the global scene emissivity.
// Emissivity is the measure of an objects ability to emit thermal radiation.
// It may take on values in the closed interval [0,1] with floating point precision.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_scene_emissivity(
	seekcamera_t* camera,
	float emissivity);

// Gets the thermography offset.
// The thermography offset is a constant that is applied to every pixel in the thermography frame.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_thermography_offset(
	seekcamera_t* camera,
	float* offset);

// Sets the thermography offset.
// The thermography offset is a constant that is applied to every pixel in the thermography frame.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_thermography_offset(
	seekcamera_t* camera,
	float offset);

// Sets the state of an image processing filter.
// Settings are refreshed between frames.
SEEKCAMERA_API seekcamera_error_t seekcamera_set_filter_state(
	seekcamera_t* camera,
	seekcamera_filter_t filter,
	seekcamera_filter_state_t state);

// Gets the state of an image processing filter.
// Settings are refreshed between frames.
// This method can only be performed after a capture session has started.
SEEKCAMERA_API seekcamera_error_t seekcamera_get_filter_state(
	seekcamera_t* camera,
	seekcamera_filter_t filter,
	seekcamera_filter_state_t* state);

#ifdef __cplusplus
}
#endif
#endif /* __SEEKCAMERA_H__ */
