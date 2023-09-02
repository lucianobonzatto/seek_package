/*Copyright (c) [2021] [Seek Thermal, Inc.]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The Software may only be used in combination with Seek cores/products.

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Project:	 Seek Thermal SDK Demo
 * Purpose:	 Demonstrates how to communicate with Seek Thermal Cameras
 * Author:	 Seek Thermal, Inc.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#	include <windows.h>
#	define inline __inline
#endif

#if defined(__linux__) || defined(__APPLE__)
#	include <unistd.h>
#	include <sys/time.h>
#endif

#include "seekcamera/seekcamera.h"
#include "seekcamera/seekcamera_manager.h"

// Define the global variables.
volatile bool g_keep_running = true;
volatile bool g_force_pairing = false;
static char* g_cal_data_path = NULL;

// Signal handler function.
static void signal_callback(int signum)
{
	(void)signum;

	fprintf(stdout, "\nCaught Ctrl+C\n\n");
	g_keep_running = false;
}

// Prints the usage instructions.
void print_usage()
{
	fprintf(stdout, "Allowed options:\n");
	fprintf(stdout, "\t-m : Discovery mode. Valid options: usb, spi, all (default: usb)\n");
	fprintf(stdout, "\t   : Required - No\n");
	fprintf(stdout, "\t-p : Path to top-level directory containing the calibration data\n");
	fprintf(stdout, "\t   : Required - No\n");
	fprintf(stdout, "\t-f : Force pairing, even if the device is already paired (default: not specified)\n");
	fprintf(stdout, "\t   : Required - No\n");
	fprintf(stdout, "\t-h : Displays this message\n");
	fprintf(stdout, "\t   : Required - No\n");
}

// Callback function for the per-camera memory access function.
// This funtion is called every time significant progress is made on a long running process.
void handle_camera_memory_access(size_t progress, void* user_data)
{
	seekcamera_t* camera = (seekcamera_t*)user_data;

	seekcamera_chipid_t cid;
	seekcamera_get_chipid(camera, &cid);

	if(progress == 100)
	{
		fprintf(stdout, "calibration data pairing %zu percent complete: %s\n", progress, cid);
	}
	else
	{
		fprintf(stdout, "calibration data pairing %zu percent complete: %s\r", progress, cid);
	}

	fflush(stdout);
}

// Handles camera error events.
void handle_camera_error(seekcamera_t* camera, seekcamera_error_t event_status, void* user_data)
{
	(void)user_data;

	seekcamera_chipid_t cid;
	seekcamera_get_chipid(camera, &cid);
	fprintf(stderr, "encountered unexpected error: %s (%s)", cid, seekcamera_error_get_str(event_status));
}

// Handles camera ready to pair events.
void handle_camera_ready_to_pair(seekcamera_t* camera, seekcamera_error_t event_status, void* user_data)
{
	(void)event_status;
	(void)user_data;

	seekcamera_chipid_t cid;
	seekcamera_get_chipid(camera, &cid);

	void* memory_access_user_data = (void*)camera;
	const seekcamera_error_t status = seekcamera_store_calibration_data(
		camera,
		g_cal_data_path,
		handle_camera_memory_access,
		memory_access_user_data);

	if(status == SEEKCAMERA_SUCCESS)
	{
		fprintf(stdout, "calibration data pairing finished successfully: %s\n", cid);
	}
	else if(status == SEEKCAMERA_ERROR_DIRECTORY_DOES_NOT_EXIST)
	{
		fprintf(stderr, "calibration data pairing failed because the calibration data directory was not found for the connected device: %s\n", cid);
	}
	else if(status == SEEKCAMERA_ERROR_FILE_DOES_NOT_EXIST)
	{
		fprintf(stderr, "calibration data pairing failed because a input required file does not exist: %s\n", cid);
	}
	else if(status == SEEKCAMERA_ERROR_FILE_READ_FAILED)
	{
		fprintf(stderr, "calibration data pairing failed because a file read failed: %s\n", cid);
	}
	else if(status == SEEKCAMERA_ERROR_FILE_WRITE_FAILED)
	{
		fprintf(stderr, "calibration data pairing failed because a file write failed: %s\n", cid);
	}
	else if(status == SEEKCAMERA_ERROR_PERMISSIONS)
	{
		fprintf(stderr, "calibration data pairing failed because of invalid permissions: %s\n", cid);
	}
	else
	{
		fprintf(stderr, "calibration data pairing failed due to an unexpected cause: %s (%s)\n", cid, seekcamera_error_get_str(status));
	}

	fflush(stdout);
	fflush(stderr);
}

// Handles camera connect events.
void handle_camera_connect(seekcamera_t* camera, seekcamera_error_t event_status, void* user_data)
{
	if(g_force_pairing)
	{
		handle_camera_ready_to_pair(camera, event_status, user_data);
	}
}

// Callback function for the camera manager; it fires whenever a camera event occurs.
void camera_event_callback(seekcamera_t* camera, seekcamera_manager_event_t event, seekcamera_error_t event_status, void* user_data)
{
	seekcamera_chipid_t cid;
	seekcamera_get_chipid(camera, &cid);

	fprintf(stdout, "%s: %s\n", seekcamera_manager_get_event_str(event), cid);

	switch(event)
	{
		case SEEKCAMERA_MANAGER_EVENT_CONNECT:
			handle_camera_connect(camera, event_status, user_data);
			break;
		case SEEKCAMERA_MANAGER_EVENT_ERROR:
			handle_camera_error(camera, event_status, user_data);
			break;
		case SEEKCAMERA_MANAGER_EVENT_READY_TO_PAIR:
			handle_camera_ready_to_pair(camera, event_status, user_data);
			break;
		default:
			break;
	}
}

// Application entry point
int main(int argc, char** argv)
{
	// Install signal handlers.
	signal(SIGINT, signal_callback);
	signal(SIGTERM, signal_callback);

	// Default values for the command line arguments.
	g_cal_data_path = NULL;
	char* discovery_mode_str = "usb";
	uint32_t discovery_mode = (uint32_t)SEEKCAMERA_IO_TYPE_USB;

	// Parse command line arguments.
	for(int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		char ch = arg[0];
		if(ch == '-' && strlen(arg) >= 2)
		{
			ch = arg[1];
			switch(ch)
			{
				case 'm':
					if(i < argc - 1)
					{
						discovery_mode_str = argv[i + 1];
						if(strcmp(discovery_mode_str, "usb") == 0)
						{
							discovery_mode = (uint32_t)SEEKCAMERA_IO_TYPE_USB;
						}
						else if(strcmp(discovery_mode_str, "spi") == 0)
						{
							discovery_mode = (uint32_t)SEEKCAMERA_IO_TYPE_SPI;
						}
						else if(strcmp(discovery_mode_str, "all") == 0)
						{
							discovery_mode = (uint32_t)SEEKCAMERA_IO_TYPE_USB | (uint32_t)SEEKCAMERA_IO_TYPE_SPI;
						}
						else
						{
							print_usage();
							return 1;
						}
					}
					else
					{
						print_usage();
						return 1;
					}
					break;
				case 'p':
					if(i < argc - 1)
					{
						g_cal_data_path = argv[i + 1];
					}
					else
					{
						print_usage();
						return 1;
					}
					break;
				case 'f':
					g_force_pairing = true;
					break;
				case 'h':
					print_usage();
					return 0;
				default:
					break;
			}
		}
	}

	fprintf(stdout, "seekcamera-cal starting\n");
	fprintf(stdout, "settings\n");
	fprintf(stdout, "\t1) mode (-m): %s\n", discovery_mode_str);
	fprintf(stdout, "\t2) path (-p): %s\n", g_cal_data_path);
	fflush(stdout);

	// Create the camera manager.
	// This is the structure that owns all Seek camera devices.
	seekcamera_manager_t* manager = NULL;
	seekcamera_error_t status = seekcamera_manager_create(&manager, discovery_mode);
	if(status != SEEKCAMERA_SUCCESS)
	{
		fprintf(stderr, "failed to create camera manager: %s\n", seekcamera_error_get_str(status));
		return 1;
	}

	// Register an event handler for the camera manager.
	// The event handler will be called for every event on a per-camera basis.
	void* user_data = NULL;
	status = seekcamera_manager_register_event_callback(manager, camera_event_callback, user_data);
	if(status != SEEKCAMERA_SUCCESS)
	{
		fprintf(stderr, "failed to register camera event callback: %s\n", seekcamera_error_get_str(status));
		seekcamera_manager_destroy(&manager);
		return 1;
	}

	// Camera events are asynchronous and interrupt the current thread.
	// There are much better ways of keeping the main thread alive. This is
	// just a simple example.
	while(g_keep_running)
	{
		const int sleep_ms = 1000;
#ifdef _WIN32
		Sleep(sleep_ms);
#else
		usleep(sleep_ms * 1000);
#endif
	}

	// Cleanup the camera manager. Cameras will be disconnected and invalidated.
	status = seekcamera_manager_destroy(&manager);
	if(status != SEEKCAMERA_SUCCESS)
	{
		fprintf(stderr, "failed to free camera manager: %s\n", seekcamera_error_get_str(status));
		return 1;
	}

	fprintf(stdout, "done\n");

	return 0;
}
