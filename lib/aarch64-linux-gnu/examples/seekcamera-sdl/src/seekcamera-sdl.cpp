/*Copyright (c) [2020] [Seek Thermal, Inc.]

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

// C includes
#include <csignal>
#include <cstring>

// C++ includes
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <utility>

// SDL includes
#if defined(__linux__) || defined(__APPLE__)
#	include <SDL2/SDL.h>
#elif defined(_WIN32)
#	define SDL_MAIN_HANDLED
#	include <SDL.h>
#endif

// Seek SDK includes
#include "seekcamera/seekcamera.h"
#include "seekcamera/seekcamera_manager.h"
#include "seekframe/seekframe.h"

// Structure representing a renderering interface.
// It uses SDL and all rendering is done on the calling thread.
struct seekrenderer_t
{
	// Frame data
	seekcamera_frame_format_t frame_format;
	seekframe_t* frame;
	size_t frame_width;
	size_t frame_height;
	size_t frame_stride;
	std::mutex frame_mutex;

	// Options
	bool do_print_metadata;

	// Camera and settings
	seekcamera_t* camera;
	std::pair<std::string, seekcamera_color_palette_t> palette;
	std::pair<std::string, seekcamera_agc_mode_t> agc_mode;
	std::pair<std::string, seekcamera_shutter_mode_t> shutter_mode;

	// Rendering data
	SDL_Rect area;
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;

	// Synchronization data
	std::atomic<bool> is_active;
	std::atomic<bool> is_visible;
};

// Structure representing a rendering event.
// These types of events are sent from camera event thread to the main thread.
// This is required because SDL events must be handled on the main thread.
struct seekrenderer_event_t
{
	enum
	{
		WINDOW_CREATE,
		WINDOW_DESTROY,
		WINDOW_DRAW_FRAME,
	} event_type;
	seekrenderer_t* renderer;
};

// Define the global variables
static std::atomic<bool> g_exit_requested;                                 // Controls application shutdown.
static std::queue<seekrenderer_event_t> g_event_queue;                     // Stores camera events that need to be coordinated with the main thread.
static std::condition_variable g_event_queue_cv;                           // Synchronizes camera events that need to be coordinated with main thread.
static std::mutex g_event_queue_mutex;                                     // Synchronizes camera events that need to be coordinated with main thread.
static std::array<seekrenderer_t*, 16> g_renderers;                        // Fixed size renderer pool - the size is arbitary.
static std::map<std::string, seekcamera_color_palette_t> g_color_palettes; // Available color palettes.
static std::map<std::string, seekcamera_agc_mode_t> g_agc_modes;           // Available AGC modes.
static std::map<std::string, seekcamera_shutter_mode_t> g_shutter_modes;   // Available shutter modes.
static bool g_auto_pair;                                                   // Controls whether unpaired cameras are automatically paired when connected.

// Applies the current color palette.
// Settings will be refreshed between frames.
bool seekrenderer_apply_color_palette(seekrenderer_t* renderer)
{
	if(!renderer->is_active.load())
	{
		return false;
	}
	return seekcamera_set_color_palette(renderer->camera, renderer->palette.second) == SEEKCAMERA_SUCCESS;
}

// Switches the current color palette.
// Settings will be refreshed between frames.
bool seekrenderer_switch_color_palette(seekrenderer_t* renderer)
{
	// Cycle through to the next color palette.
	auto palette = g_color_palettes.find(renderer->palette.first);
	std::advance(palette, 1);

	// Hit the end of the map -- loop back around.
	if(palette == g_color_palettes.end())
	{
		palette = g_color_palettes.begin();
	}

	// Store the color palette key and value.
	renderer->palette.first = palette->first;
	renderer->palette.second = palette->second;

	return seekrenderer_apply_color_palette(renderer);
}

// Applies the current AGC mode.
// Settings will be refreshed between frames.
bool seekrenderer_apply_agc_mode(seekrenderer_t* renderer)
{
	if(!renderer->is_active.load())
	{
		return false;
	}
	return seekcamera_set_agc_mode(renderer->camera, renderer->agc_mode.second) == SEEKCAMERA_SUCCESS;
}

// Switches the current AGC mode.
// Settings will be refreshed between frames.
bool seekrenderer_switch_agc_mode(seekrenderer_t* renderer)
{
	// Cycle through to the next AGC mode.
	auto mode = g_agc_modes.find(renderer->agc_mode.first);
	std::advance(mode, 1);

	// Hit the end of the map -- loop back around.
	if(mode == g_agc_modes.end())
	{
		mode = g_agc_modes.begin();
	}

	// Store the AGC mode key and value.
	renderer->agc_mode.first = mode->first;
	renderer->agc_mode.second = mode->second;

	return seekrenderer_apply_agc_mode(renderer);
}

// Applies the current shutter mode.
// Settings will be refreshed between frames.
bool seekrenderer_apply_shutter_mode(seekrenderer_t* renderer)
{
	if(!renderer->is_active.load())
	{
		return false;
	}
	return seekcamera_set_shutter_mode(renderer->camera, renderer->shutter_mode.second) == SEEKCAMERA_SUCCESS;
}

// Switches the current shutter mode.
// Settings will be refreshed between frames.
bool seekrenderer_switch_shutter_mode(seekrenderer_t* renderer)
{
	// Cycle through to the next shutter mode.
	auto mode = g_shutter_modes.find(renderer->shutter_mode.first);
	std::advance(mode, 1);

	// Hit the end of the map -- loop back around.
	if(mode == g_shutter_modes.end())
	{
		mode = g_shutter_modes.begin();
	}

	// Store the shutter mode and key.
	renderer->shutter_mode.first = mode->first;
	renderer->shutter_mode.second = mode->second;

	return seekrenderer_apply_shutter_mode(renderer);
}

// Triggers the shutter.
bool seekrenderer_shutter_trigger(seekrenderer_t* renderer)
{
	if(renderer->is_active.load())
	{
		const seekcamera_error_t status = seekcamera_shutter_trigger(renderer->camera);
		return status == SEEKCAMERA_SUCCESS;
	}
	return true;
}

// Print the frame header metadata to the console.
// The metadata will be printed on the next frame.
bool seekrenderer_print_frame_header(seekrenderer_t* renderer)
{
	if(!renderer->is_active.load())
	{
		return false;
	}
	renderer->do_print_metadata = true;
	return true;
}

// Restarts the capture session.
// Reapplies the current settings.
bool seekrenderer_capture_session_restart(seekrenderer_t* renderer)
{
	if(!renderer->is_active.load())
	{
		return false;
	}

	// Stop the capture session.
	seekcamera_error_t status = seekcamera_capture_session_stop(renderer->camera);
	if(status != SEEKCAMERA_SUCCESS)
	{
		return false;
	}

	// Reapply the current color palette.
	if(!seekrenderer_apply_color_palette(renderer))
	{
		return false;
	}

	// Reapply the current AGC mode.
	if(!seekrenderer_apply_agc_mode(renderer))
	{
		return false;
	}

	// Reapply the current shutter mode.
	if(!seekrenderer_apply_shutter_mode(renderer))
	{
		return false;
	}

	// Start capture session.
	status = seekcamera_capture_session_start(renderer->camera, renderer->frame_format);
	if(status != SEEKCAMERA_SUCCESS)
	{
		return false;
	}

	return true;
}

// Closes the SDL window associated with a renderer.
void seekrenderer_close_window(seekrenderer_t* renderer)
{
	renderer->is_visible.store(false);

	if(renderer->texture != NULL)
	{
		SDL_DestroyTexture(renderer->texture);
		renderer->texture = nullptr;
	}

	if(renderer->renderer != NULL)
	{
		SDL_DestroyRenderer(renderer->renderer);
		renderer->renderer = nullptr;
	}

	if(renderer->window != NULL)
	{
		SDL_DestroyWindow(renderer->window);
		renderer->window = nullptr;
	}
}

// Prints the usage instructions.
void print_usage()
{
	std::cout
		<< "Allowed options:\n"
		<< "\t-m,--mode              : Discovery mode. Valid options: usb, spi, all (default: usb)\n"
		<< "\t                       : Required - No\n"
		<< "\t-d,--disable-auto-pair : Disables auto pairing\n"
		<< "\t                       : Required - No (default: not specified)\n"
		<< "\t-h,--help              : Displays this message\n"
		<< "\t                       : Required - No" << std::endl;
}

// Prints the user controls.
void print_user_controls()
{
	std::cout
		<< "user controls:\n"
		<< "\t1) mouse click: next color palette\n"
		<< "\t2) c:           next color palette\n"
		<< "\t3) a:           next agc mode\n"
		<< "\t4) s:           next shutter mode\n"
		<< "\t5) t:           trigger shutter (product dependent)\n"
		<< "\t6) p:           print frame metadata from the header to the console\n"
		<< "\t7) r:           restart capture session\n"
		<< "\t8) h:           display this message\n"
		<< "\t9) q:           quit"
		<< std::endl;
}

// Signal handler function.
static void signal_callback(int signum)
{
	(void)signum;
	std::cout << "\ncaught ctrl+c\n";
	std::cout << std::endl;
	g_exit_requested.store(true);
}

// Gets the number of active renderers.
size_t get_num_active_renderers()
{
	return std::count_if(std::begin(g_renderers), std::end(g_renderers), [](seekrenderer_t* renderer) {
		return renderer->is_active.load();
	});
}

// Gets the first available renderer.
seekrenderer_t* get_renderer_by_availability()
{
	for(auto& g_renderer : g_renderers)
	{
		if(!g_renderer->is_active.load())
		{
			return g_renderer;
		}
	}
	return nullptr;
}

// Gets the renderer associated with a camera.
seekrenderer_t* get_renderer_by_camera(seekcamera_t* camera)
{
	if(camera == nullptr)
	{
		return nullptr;
	}

	seekrenderer_t* renderer = nullptr;

	// Get the CID of this camera.
	// It will be used to find the renderer associated with this camera.
	seekcamera_chipid_t this_camera_cid = { 0 };
	seekcamera_get_chipid(camera, &this_camera_cid);

	// Find the renderer associated with this camera.
	for(auto& g_renderer : g_renderers)
	{
		// This renderer is inactive.
		// Renderers are not reaped until shutdown.
		if(!g_renderer->is_active.load())
		{
			continue;
		}

		seekcamera_chipid_t renderer_cid = { 0 };
		seekcamera_get_chipid(g_renderer->camera, &renderer_cid);

		if(strncmp(this_camera_cid, renderer_cid, sizeof(seekcamera_chipid_t)) == 0)
		{
			renderer = g_renderer;
			break;
		}
	}

	return renderer;
}

// Gets the renderer associated with a SDL window ID.
seekrenderer_t* get_renderer_by_window_id(uint32_t window_id)
{
	seekrenderer_t* renderer = nullptr;

	// Find the renderer associated with this window.
	for(auto& g_renderer : g_renderers)
	{
		// This renderer is inactive.
		// Renderers are not repeated until shutdown.
		if(!g_renderer->is_active.load())
		{
			continue;
		}

		if(SDL_GetWindowID(g_renderer->window) == window_id)
		{
			renderer = g_renderer;
			break;
		}
	}

	return renderer;
}

// Handles frame available events.
void handle_camera_frame_available(seekcamera_t* camera, seekcamera_frame_t* camera_frame, void* user_data)
{
	(void)camera;
	auto* renderer = (seekrenderer_t*)user_data;

	// Get the frame to draw.
	seekframe_t* frame = nullptr;
	const seekcamera_error_t status = seekcamera_frame_get_frame_by_format(camera_frame, renderer->frame_format, &frame);
	if(status != SEEKCAMERA_SUCCESS)
	{
		std::cerr << "failed to get frame: " << seekcamera_error_get_str(status) << std::endl;
		return;
	}

	// Store the frame.
	std::lock_guard<std::mutex> frame_lock(renderer->frame_mutex);
	renderer->frame = frame;

	// Create a new event to be processed on the main thread.
	// The event is used to notify the main thread to draw a new frame to the SDL window.
	seekrenderer_event_t event{};
	event.event_type = seekrenderer_event_t::WINDOW_DRAW_FRAME;
	event.renderer = renderer;

	// Store the draw event.
	std::unique_lock<std::mutex> lock(g_event_queue_mutex);
	g_event_queue.push(event);
	lock.unlock();

	// Notify the main thread.
	g_event_queue_cv.notify_one();
}

// Handles camera connect events.
void handle_camera_connect(seekcamera_t* camera, seekcamera_error_t event_status, void* user_data)
{
	(void)event_status;
	(void)user_data;

	auto renderer = get_renderer_by_availability();
	if(renderer == nullptr)
	{
		std::cerr << "renderer pool is exhausted" << std::endl;
		return;
	}

	// Enable the renderer.
	renderer->is_active.store(true);

	// Store the camera reference.
	renderer->camera = camera;

	// Register a frame available callback function.
	seekcamera_error_t status = seekcamera_register_frame_available_callback(renderer->camera, handle_camera_frame_available, (void*)renderer);
	if(status != SEEKCAMERA_SUCCESS)
	{
		std::cerr << "failed to register frame callback: " << seekcamera_error_get_str(status) << std::endl;
		renderer->is_active.store(false);
		return;
	}

	// Start the capture session.
	status = seekcamera_capture_session_start(renderer->camera, renderer->frame_format);
	if(status != SEEKCAMERA_SUCCESS)
	{
		std::cerr << "failed to start capture session: " << seekcamera_error_get_str(status) << std::endl;
		renderer->is_active.store(false);
		return;
	}

	// Apply the current color palette.
	if(!seekrenderer_apply_color_palette(renderer))
	{
		std::cerr << "failed to set color palette" << std::endl;
	}

	// Apply the current AGC mode.
	if(!seekrenderer_apply_agc_mode(renderer))
	{
		std::cerr << "failed to set agc mode" << std::endl;
	}

	// Apply the current shutter mode.
	if(!seekrenderer_apply_shutter_mode(renderer))
	{
		std::cerr << "failed to set shutter mode" << std::endl;
	}

	// Create a new event to be processed on the main thread.
	// The event is used to notify the main thread to create a new SDL window for drawing.
	seekrenderer_event_t event{};
	event.event_type = seekrenderer_event_t::WINDOW_CREATE;
	event.renderer = renderer;

	// Store the connect event.
	std::unique_lock<std::mutex> lock(g_event_queue_mutex);
	g_event_queue.push(event);
	lock.unlock();

	// Notify the main thread.
	g_event_queue_cv.notify_one();
}

// Handles camera disconnect events.
void handle_camera_disconnect(seekcamera_t* camera, seekcamera_error_t event_status, void* user_data)
{
	(void)event_status;
	(void)user_data;

	auto renderer = get_renderer_by_camera(camera);
	if(renderer == nullptr)
	{
		return;
	}

	renderer->is_active.store(false);

	// Invalidate references that rely on the camera lifetime.
	renderer->camera = nullptr;
	renderer->frame = nullptr;

	// Create a new event to be processed on the main thread.
	// The event is used to notify the main thread to destroy an existing SDL window.
	seekrenderer_event_t event{};
	event.event_type = seekrenderer_event_t::WINDOW_DESTROY;
	event.renderer = renderer;

	// Store the disconnect event.
	std::unique_lock<std::mutex> lock(g_event_queue_mutex);
	g_event_queue.push(event);
	lock.unlock();

	// Notify the main thread.
	g_event_queue_cv.notify_one();
}

// Handles camera error events.
void handle_camera_error(seekcamera_t* camera, seekcamera_error_t event_status, void* user_data)
{
	auto renderer = get_renderer_by_camera(camera);
	if(renderer == nullptr)
	{
		return;
	}

	// Handle the camera error.
	// For the purpose of the sample, errors are repeatedly handled until they are unhandlable.
	switch(event_status)
	{
		case SEEKCAMERA_SUCCESS:
		{
			break;
		}
		case SEEKCAMERA_ERROR_TIMEOUT:
		case SEEKCAMERA_ERROR_DEVICE_COMMUNICATION:
		{
			std::cerr << "failed to communicate to device" << std::endl;
			if(renderer->is_active)
			{
				std::cerr << "restarting capture session" << std::endl;
				if(!seekrenderer_capture_session_restart(renderer))
				{
					std::cerr << "failed to restart capture session" << std::endl;
					handle_camera_disconnect(camera, event_status, user_data);
				}
			}
			break;
		}
		case SEEKCAMERA_ERROR_NOT_PAIRED:
		{
			std::cerr << "device is unpaired - pair the device before imaging" << std::endl;
			break;
		}
		default:
		{
			std::cout << "unhandled camera error: " << seekcamera_error_get_str(event_status) << std::endl;
			break;
		}
	}
}

// Handles camera ready to pair events
void handle_camera_ready_to_pair(seekcamera_t* camera, seekcamera_error_t event_status, void* user_data)
{
	if(!g_auto_pair)
	{
		std::cout << "device is unpaired - pair the device before imaging" << std::endl;
	}

	// Lambda for printing pairing progress updates to the console.
	auto progress_callback = [](size_t progress, void*) {
		if(progress == 100)
		{
			std::cout << "pairing " << progress << "% complete\n";
		}
		else
		{
			std::cout << "pairing " << progress << "% complete\r";
		}
		std::flush(std::cout);
	};

	// Attempt to pair the camera automatically.
	// Pairing refers to the process by which the sensor is associated with the host and the embedded processor.
	const seekcamera_error_t status = seekcamera_store_calibration_data(camera, nullptr, progress_callback, nullptr);
	if(status != SEEKCAMERA_SUCCESS)
	{
		std::cerr << "failed to pair device: " << seekcamera_error_get_str(status) << std::endl;
	}

	// Start imaging.
	handle_camera_connect(camera, event_status, user_data);
}

// Callback function for the camera manager; it fires whenever a camera event occurs.
void camera_event_callback(seekcamera_t* camera, seekcamera_manager_event_t event, seekcamera_error_t event_status, void* user_data)
{
	seekcamera_chipid_t cid{};
	seekcamera_get_chipid(camera, &cid);

	seekcamera_firmware_version_t firmware_version{};
	seekcamera_get_firmware_version(camera, &firmware_version);

	std::cout
		<< seekcamera_manager_get_event_str(event) << ' '
		<< "(CID: " << cid
		<< ", FW: "
		<< static_cast<int>(firmware_version.product) << '.'
		<< static_cast<int>(firmware_version.variant) << '.'
		<< static_cast<int>(firmware_version.major) << '.'
		<< static_cast<int>(firmware_version.minor)
		<< ")" << std::endl;

	// Handle the event type.
	switch(event)
	{
		case SEEKCAMERA_MANAGER_EVENT_CONNECT:
			handle_camera_connect(camera, event_status, user_data);
			break;
		case SEEKCAMERA_MANAGER_EVENT_DISCONNECT:
			handle_camera_disconnect(camera, event_status, user_data);
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

// Application entry point.
int main(int argc, char** argv)
{
	// Initialize global variables.
	g_exit_requested.store(false);

	g_auto_pair = true;

	g_color_palettes.insert({ "white-hot", SEEKCAMERA_COLOR_PALETTE_WHITE_HOT });
	g_color_palettes.insert({ "black-hot", SEEKCAMERA_COLOR_PALETTE_BLACK_HOT });
	g_color_palettes.insert({ "spectra", SEEKCAMERA_COLOR_PALETTE_SPECTRA });
	g_color_palettes.insert({ "prism", SEEKCAMERA_COLOR_PALETTE_PRISM });
	g_color_palettes.insert({ "tyrian", SEEKCAMERA_COLOR_PALETTE_TYRIAN });
	g_color_palettes.insert({ "iron", SEEKCAMERA_COLOR_PALETTE_IRON });
	g_color_palettes.insert({ "amber", SEEKCAMERA_COLOR_PALETTE_AMBER });
	g_color_palettes.insert({ "hi", SEEKCAMERA_COLOR_PALETTE_HI });
	g_color_palettes.insert({ "green", SEEKCAMERA_COLOR_PALETTE_GREEN });

	g_agc_modes.insert({ "histeq", SEEKCAMERA_AGC_MODE_HISTEQ });
	g_agc_modes.insert({ "linear", SEEKCAMERA_AGC_MODE_LINEAR });

	g_shutter_modes.insert({ "auto", SEEKCAMERA_SHUTTER_MODE_AUTO });
	g_shutter_modes.insert({ "manual", SEEKCAMERA_SHUTTER_MODE_MANUAL });

	for(auto& g_renderer : g_renderers)
	{
		g_renderer = new seekrenderer_t();

		// Initialize the the frame data.
		g_renderer->frame_format = SEEKCAMERA_FRAME_FORMAT_COLOR_ARGB8888;
		g_renderer->frame = nullptr;
		g_renderer->frame_width = 0;
		g_renderer->frame_height = 0;
		g_renderer->frame_stride = 0;

		g_renderer->do_print_metadata = false;

		g_renderer->camera = nullptr;

		// Initialize the default color palette.
		auto palette = g_color_palettes.find("tyrian");
		g_renderer->palette.first = palette->first;
		g_renderer->palette.second = palette->second;

		// Initialize the default AGC mode.
		auto agc_mode = g_agc_modes.find("histeq");
		g_renderer->agc_mode.first = agc_mode->first;
		g_renderer->agc_mode.second = agc_mode->second;

		// Initialize the default shutter mode.
		auto shutter_mode = g_shutter_modes.find("auto");
		g_renderer->shutter_mode.first = shutter_mode->first;
		g_renderer->shutter_mode.second = shutter_mode->second;

		// Initialize the rendering data.
		g_renderer->area.x = 0;
		g_renderer->area.y = 0;
		g_renderer->area.w = 0;
		g_renderer->area.h = 0;
		g_renderer->window = nullptr;
		g_renderer->renderer = nullptr;
		g_renderer->texture = nullptr;

		// Initialize the synchronization data.
		g_renderer->is_active.store(false);
		g_renderer->is_visible.store(false);
	}

	// Install signal handlers.
	signal(SIGINT, signal_callback);
	signal(SIGTERM, signal_callback);

	// Default values for the command line arguments.
	std::string discovery_mode_str("usb");
	uint32_t discovery_mode = static_cast<uint32_t>(SEEKCAMERA_IO_TYPE_USB);

	// Parse command line arguments.
	for(int i = 1; i < argc; ++i)
	{
		const std::string arg(argv[i]);
		if(arg.find('-') != std::string::npos)
		{
			if(arg == "-m" || arg == "--mode")
			{
				if(i == argc - 1)
				{
					print_usage();
					return 1;
				}

				discovery_mode_str = std::string(argv[i + 1]);
				if(discovery_mode_str == "usb")
				{
					discovery_mode = static_cast<uint32_t>(SEEKCAMERA_IO_TYPE_USB);
				}
				else if(discovery_mode_str == "spi")
				{
					discovery_mode = static_cast<uint32_t>(SEEKCAMERA_IO_TYPE_SPI);
				}
				else if(discovery_mode_str == "all")
				{
					discovery_mode = static_cast<uint32_t>(SEEKCAMERA_IO_TYPE_USB) | static_cast<uint32_t>(SEEKCAMERA_IO_TYPE_SPI);
				}
				else
				{
					print_usage();
					return 1;
				}
			}
			else if(arg == "-d" || arg == "--disable-auto-pair")
			{
				g_auto_pair = false;
			}
			else if(arg == "-h" || arg == "--help")
			{
				print_usage();
				return 0;
			}
			else
			{
				print_usage();
				return 1;
			}
		}
	}

	// Print the current settings.
	std::cout
		<< "seekcamera-sdl starting\n"
		<< "settings:\n"
		<< "\t1) mode:      " << discovery_mode_str << '\n'
		<< "\t2) auto pair: " << (g_auto_pair ? "on" : "off") << std::endl;

	// Print the user controls.
	print_user_controls();

	// Initialize SDL and enable bilinear stretching.
	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	std::cout << "display driver: " << SDL_GetVideoDriver(0) << std::endl;

	// Create the camera manager.
	// This is the structure that owns all Seek camera devices.
	seekcamera_manager_t* manager = nullptr;
	seekcamera_error_t status = seekcamera_manager_create(&manager, discovery_mode);
	if(status != SEEKCAMERA_SUCCESS)
	{
		std::cerr << "failed to create camera manager: " << seekcamera_error_get_str(status) << std::endl;
		return 1;
	}

	// Register an event handler for the camera manager to be called whenever a camera event occurs.
	void* user_data = nullptr;
	status = seekcamera_manager_register_event_callback(manager, camera_event_callback, user_data);
	if(status != SEEKCAMERA_SUCCESS)
	{
		std::cerr << "failed to register camera event callback: " << seekcamera_error_get_str(status) << std::endl;
		return 1;
	}

	// Cascade the windows.
	int window_position_index = 0;
	const std::vector<std::pair<int, int>> window_positions = {
		std::make_pair<int, int>(100, 100),
		std::make_pair<int, int>(110, 110),
		std::make_pair<int, int>(120, 120),
		std::make_pair<int, int>(130, 130),
		std::make_pair<int, int>(140, 140),
		std::make_pair<int, int>(150, 150),
	};

	// Poll for events until told to stop.
	// Both renderer events and SDL events are polled.
	// Events are polled on the main thread because SDL events must be handled on the main thread.
	while(!g_exit_requested.load())
	{
		// Poll for the renderer events.
		// These events come from the asynchronous SDK event threads but require SDL calls, so they are handled here.
		std::unique_lock<std::mutex> event_lock(g_event_queue_mutex);
		if(g_event_queue_cv.wait_for(event_lock, std::chrono::milliseconds(150), [=] { return !g_event_queue.empty(); }))
		{
			seekrenderer_event_t renderer_event = g_event_queue.front();
			g_event_queue.pop();
			event_lock.unlock();

			auto renderer = renderer_event.renderer;

			// Handle the renderer event.
			switch(renderer_event.event_type)
			{
				case seekrenderer_event_t::WINDOW_CREATE:
				{
					const auto position = window_positions[window_position_index];
					window_position_index = (window_position_index + 1) % window_positions.size();

					// Setup the window handle.
					const int window_x = position.first;
					const int window_y = position.second;
					const int window_width = 0;
					const int window_height = 0;

					// Set the window title.
					seekcamera_chipid_t cid{};
					seekcamera_get_chipid(renderer->camera, &cid);

					seekcamera_firmware_version_t firmware_version{};
					seekcamera_get_firmware_version(renderer->camera, &firmware_version);

					std::stringstream window_title;
					window_title
						<< "Seek Thermal - SDL Sample "
						<< "(CID: " << cid
						<< ", FW: "
						<< static_cast<int>(firmware_version.product) << '.'
						<< static_cast<int>(firmware_version.variant) << '.'
						<< static_cast<int>(firmware_version.major) << '.'
						<< static_cast<int>(firmware_version.minor)
						<< ")";

					// Setup the window handle.
					SDL_Window* window = SDL_CreateWindow(window_title.str().c_str(), window_x, window_y, window_width, window_height, SDL_WINDOW_HIDDEN);
#if SDL_VERSION_ATLEAST(2, 0, 5)
					SDL_SetWindowResizable(window, SDL_TRUE);
#endif

					// Setup the window renderer.
					SDL_Renderer* window_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

					// Setup the renderer area.
					renderer->area.x = 0;
					renderer->area.y = 0;
					renderer->area.w = 0;
					renderer->area.h = 0;
					renderer->window = window;
					renderer->renderer = window_renderer;

					// Set the window to black.
					SDL_SetRenderDrawColor(renderer->renderer, 0, 0, 0, 0xFF);
					SDL_RenderFillRect(renderer->renderer, &(renderer->area));
					SDL_RenderPresent(renderer->renderer);

					// Allow frames to be drawn.
					renderer->is_visible.store(true);

					break;
				}
				case seekrenderer_event_t::WINDOW_DESTROY:
				{
					renderer->is_active.store(false);

					seekrenderer_close_window(renderer);

					// Keep the application alive as long as one renderer is still active.
					if(get_num_active_renderers() == 0)
					{
						g_exit_requested.store(true);
					}

					break;
				}
				case seekrenderer_event_t::WINDOW_DRAW_FRAME:
				{
					if(!renderer->is_visible.load())
					{
						break;
					}

					std::lock_guard<std::mutex> frame_lock(renderer->frame_mutex);
					if(renderer->frame == NULL)
					{
						break;
					}

					// Get the frame dimensions.
					const size_t frame_width = seekframe_get_width(renderer->frame);
					const size_t frame_height = seekframe_get_height(renderer->frame);
					const size_t frame_stride = seekframe_get_line_stride(renderer->frame);

					// Lazy allocate the texture data.
					if(
						renderer->texture == nullptr ||
						renderer->frame_width != frame_width ||
						renderer->frame_height != frame_height ||
						renderer->frame_stride != frame_stride)
					{
						// Cache the frame sizes.
						renderer->frame_width = frame_width;
						renderer->frame_height = frame_height;
						renderer->frame_stride = frame_stride;

						// Realloc the texture data to match the new frame dimensions.
						if(renderer->texture != nullptr)
						{
							SDL_DestroyTexture(renderer->texture);
							renderer->texture = nullptr;
						}

						// Resize and show the window -- upscaling by two.
						const size_t scale_factor = 2;
						renderer->area.w = static_cast<int>(renderer->frame_width * scale_factor);
						renderer->area.h = static_cast<int>(renderer->frame_height * scale_factor);
						SDL_RenderSetLogicalSize(renderer->renderer, renderer->area.w, renderer->area.h);

						renderer->texture = SDL_CreateTexture(
							renderer->renderer,
							SDL_PIXELFORMAT_ARGB8888,
							SDL_TEXTUREACCESS_TARGET,
							static_cast<int>(renderer->frame_width),
							static_cast<int>(renderer->frame_height));

						SDL_SetWindowSize(renderer->window, renderer->area.w, renderer->area.h);
						SDL_ShowWindow(renderer->window);
					}

					// Update the SDL windows and renderers.
					SDL_UpdateTexture(renderer->texture, nullptr, seekframe_get_data(renderer->frame), static_cast<int>(renderer->frame_stride));
					SDL_RenderCopy(renderer->renderer, renderer->texture, nullptr, nullptr);
					SDL_RenderPresent(renderer->renderer);

					if(renderer->do_print_metadata)
					{
						auto* header = (seekcamera_frame_header_t*)seekframe_get_header(renderer->frame);
						std::cout
							<< "frame metadata: ("
							<< "CID: " << header->chipid << ", "
							<< "TIMESTAMP: " << header->timestamp_utc_ns << ", "
							<< "SPOT: " << header->thermography_spot_value << ')'
							<< std::endl;
						renderer->do_print_metadata = false;
					}

					// Invalidate the frame handle.
					renderer->frame = nullptr;

					break;
				}
				default:
				{
					// Unhandled event type.
					break;
				}
			}
		}

		// Handle the SDL window events.
		// The events need to be polled in order for the window to be responsive.
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			auto renderer = get_renderer_by_window_id(event.window.windowID);
			if(renderer == nullptr)
			{
				break;
			}

			if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
			{
				if(renderer->is_active.load())
				{
					seekcamera_capture_session_stop(renderer->camera);
					renderer->is_active.store(false);
				}

				seekrenderer_close_window(renderer);

				// Keep the application alive as long as one renderer is still active.
				if(get_num_active_renderers() == 0)
				{
					g_exit_requested.store(true);
				}
			}
			else if(event.type == SDL_MOUSEBUTTONDOWN)
			{
				if(seekrenderer_switch_color_palette(renderer))
				{
					std::cout << "color palette: " << renderer->palette.first << std::endl;
				}
				else
				{
					std::cerr << "failed to set color palette" << std::endl;
				}
			}
			else if(event.type == SDL_KEYUP)
			{
				if(event.key.keysym.sym == SDLK_c)
				{
					if(seekrenderer_switch_color_palette(renderer))
					{
						std::cout << "color palette: " << renderer->palette.first << std::endl;
					}
					else
					{
						std::cerr << "failed to set color palette" << std::endl;
					}
				}
				else if(event.key.keysym.sym == SDLK_a)
				{
					if(seekrenderer_switch_agc_mode(renderer))
					{
						std::cout << "agc mode: " << renderer->agc_mode.first << std::endl;
					}
					else
					{
						std::cerr << "failed to set agc mode" << std::endl;
					}
				}
				else if(event.key.keysym.sym == SDLK_s)
				{
					if(seekrenderer_switch_shutter_mode(renderer))
					{
						std::cout << "shutter mode: " << renderer->shutter_mode.first << std::endl;
					}
					else
					{
						std::cerr << "failed to set shutter mode" << std::endl;
					}
				}
				else if(event.key.keysym.sym == SDLK_t)
				{
					if(seekrenderer_shutter_trigger(renderer))
					{
						std::cout << "shutter triggered" << std::endl;
					}
					else
					{
						std::cerr << "failed to trigger shutter" << std::endl;
					}
				}
				else if(event.key.keysym.sym == SDLK_p)
				{
					if(!seekrenderer_print_frame_header(renderer))
					{
						std::cerr << "failed to print frame metadata " << std::endl;
					}
				}
				else if(event.key.keysym.sym == SDLK_r)
				{
					if(seekrenderer_capture_session_restart(renderer))
					{
						std::cout << "restarted capture session" << std::endl;
					}
					else
					{
						std::cerr << "failed to restart capture session" << std::endl;
					}
				}
				else if(event.key.keysym.sym == SDLK_h)
				{
					print_user_controls();
				}
				else if(event.key.keysym.sym == SDLK_q)
				{
					// Close all windows and exit.
					g_exit_requested.store(true);
				}
			}
		}
	}

	// Teardown the camera manager.
	seekcamera_manager_destroy(&manager);

	// Teardown global variables.
	for(auto& renderer : g_renderers)
	{
		renderer->is_active.store(false);
		seekrenderer_close_window(renderer);

		delete renderer;
		renderer = nullptr;
	}

	// Teardown SDL.
	SDL_Quit();

	std::cout << "done" << std::endl;

	return 0;
}
