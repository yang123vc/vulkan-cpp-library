/*
* Copyright 2016 Google Inc. All Rights Reserved.

* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#ifndef _VCC_WINDOW_H_
#define _VCC_WINDOW_H_

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#endif // __ANDROID__
#include <thread>
#include <vector>
#include <vcc/command_pool.h>
#include <vcc/device.h>
#include <vcc/image.h>
#include <vcc/image_view.h>
#include <vcc/instance.h>
#include <vcc/keycode.h>
#include <vcc/queue.h>
#include <vcc/surface.h>
#ifdef _WIN32
#include <windows.h>
#endif // WIN32

namespace vcc {
namespace window {

namespace internal {
struct window_data_type;

/*
* Simple thread that executes given tasks on a queue
* as well as a default render function.
*/
struct render_thread {
	typedef std::function<void()> callback_type;

	render_thread() = default;

	explicit render_thread(callback_type &&draw_callback)
		: running(true), drawing(false),
		thread([draw_callback, this]() {
		while (running) {
			std::vector<callback_type> tasks;
			{
				std::unique_lock<std::mutex> lock(tasks_mutex);
				cv.wait(lock, [this]() { return !this->tasks.empty() || drawing; });
				tasks = std::move(this->tasks);
			}
			for (const callback_type &callback : tasks) {
				callback();
			}
			if (drawing) {
				draw_callback();
			}
		}
	}) {}

	void set_drawing(bool drawing) {
		this->drawing = drawing;
		cv.notify_one();
	}

	void post(callback_type &&callback) {
		std::unique_lock<std::mutex> lock(tasks_mutex);
		tasks.push_back(std::forward<callback_type>(callback));
	}

	void join() {
		running = false;
		cv.notify_one();
		thread.join();
	}

	std::vector<callback_type> tasks;
	std::mutex tasks_mutex;
	volatile bool running, drawing;
	std::condition_variable cv;
	std::thread thread;
};

} // namespace internal

// TODO(gardell): Rename to swapchain_image_type.
struct swapchain_type {
	friend void resize(internal::window_data_type &data, VkExtent2D extent);
	friend image::image_type &get_image(swapchain_type &swapchain);
	friend image_view::image_view_type &get_image_view(swapchain_type &swapchain);

	swapchain_type() = default;
	swapchain_type(const swapchain_type&) = delete;
	swapchain_type(swapchain_type&&) = default;
	swapchain_type &operator=(const swapchain_type&) = delete;
	swapchain_type &operator=(swapchain_type&&) = default;

private:
	swapchain_type(image::image_type &&image, image_view::image_view_type &&view)
		: image(std::forward<image::image_type>(image)),
		  view(std::forward<image_view::image_view_type>(view)) {}
	image::image_type image;
	image_view::image_view_type view;
};

inline image::image_type &get_image(swapchain_type &swapchain) {
	return swapchain.image;
}

inline image_view::image_view_type &get_image_view(swapchain_type &swapchain) {
	return swapchain.view;
}

typedef std::function<void(VkFormat)> initialize_callback_type;
// First argument is graphics queue, second is present queue.
typedef std::function<std::pair<queue::queue_type, queue::queue_type>(surface::surface_type &)> queue_callback_type;
typedef std::function<void(VkExtent2D, VkFormat, std::vector<swapchain_type> &)> resize_callback_type;
typedef std::function<void(uint32_t)> draw_callback_type;

enum mouse_button_type {
	mouse_button_left = 0,
	mouse_button_middle = 1,
	mouse_button_right = 2,
	mouse_button_4 = 3,
	mouse_button_5 = 4,
	mouse_button_6 = 5,
	mouse_button_7 = 6,
	mouse_button_8 = 7
};

typedef std::function<bool(mouse_button_type, int, int)> mouse_press_callback_type;
typedef std::function<bool(int, int)> mouse_move_callback_type;
typedef std::function<bool(keycode_type)> key_press_callback_type;

struct input_callbacks_type {
	VCC_LIBRARY input_callbacks_type();
	mouse_press_callback_type mouse_down_callback, mouse_up_callback;
	mouse_move_callback_type mouse_move_callback;
	key_press_callback_type key_down_callback, key_up_callback;

	VCC_LIBRARY input_callbacks_type &set_mouse_down_callback(const mouse_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_mouse_up_callback(const mouse_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_mouse_move_callback(const mouse_move_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_key_down_callback(const key_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_key_up_callback(const key_press_callback_type &callback);
};

namespace internal {

struct window_data_type {
#ifdef _WIN32
	HINSTANCE connection;        // hInstance - Windows Instance
	HWND        window;          // hWnd - window handle
#elif defined(__ANDROID__)
	android_app* state;
#endif // __ANDROID__

	window_data_type() = default;
	window_data_type(const window_data_type&) = delete;
	window_data_type(window_data_type&&) = default;
	window_data_type &operator=(const window_data_type&) = delete;
	window_data_type &operator=(window_data_type&&) = default;

	VCC_LIBRARY ~window_data_type();

	window_data_type(
#ifdef WIN32
			HINSTANCE hinstance,
#elif defined(__ANDROID__)
			android_app* state,
#endif // __ANDROID__
			type::supplier<instance::instance_type> &&instance, VkExtent2D extent,
			resize_callback_type resize_callback, draw_callback_type draw_callback,
			type::supplier<device::device_type> &&device, initialize_callback_type initialize_callback,
			queue_callback_type queue_callback,
			const input_callbacks_type &input_callbacks) :
#ifdef WIN32
				connection(hinstance),
#elif defined(__ANDROID__)
				state(state),
#endif // WIN32
				instance(std::forward<type::supplier<instance::instance_type>>(instance)),
				extent(extent),
				resize_callback(resize_callback),
				draw_callback(draw_callback),
				device(std::forward<type::supplier<device::device_type>>(device)),
				initialize_callback(initialize_callback),
				queue_callback(queue_callback),
				input_callbacks(input_callbacks),
				render_thread([this]() {draw(); }) {}

	void draw();

	type::supplier<instance::instance_type> instance;
	surface::surface_type surface;

	VkExtent2D extent;

	// TODO(gardell): Extract to generic worker thread.
	internal::render_thread render_thread;

	resize_callback_type resize_callback;
	draw_callback_type draw_callback;

	type::supplier<device::device_type> device;
	initialize_callback_type initialize_callback;
	queue_callback_type queue_callback;
	input_callbacks_type input_callbacks;
	queue::queue_type graphics_queue;
	queue::queue_type present_queue;
	VkFormat format;
	VkColorSpaceKHR color_space;
	swapchain::swapchain_type swapchain;
	std::vector<swapchain_type> swapchain_images;
	command_pool::command_pool_type cmd_pool;
};

}  // namespace internal

struct window_type {
	window_type() = default;
	window_type(const window_type&) = delete;
	window_type(window_type&&) = default;
	window_type &operator=(const window_type&) = delete;
	window_type &operator=(window_type&&) = default;

	explicit window_type(std::unique_ptr<internal::window_data_type> &&data) : data(std::forward<std::unique_ptr<internal::window_data_type>>(data)) {}
	std::unique_ptr<internal::window_data_type> data;
};

// queue_callback is called within the scope of this function. Return a graphics and a present capable queue (can be the same queue).
// The window will use the present queue to present the surface images. Before calling the draw_callback, it will make sure the
// surface images are ready for usage by the graphics queue.
VCC_LIBRARY window_type create(
#ifdef _WIN32
		HINSTANCE hinstance,
#elif defined(__ANDROID__)
		android_app* state,
#endif // __ANDROID__
		type::supplier<instance::instance_type> &&instance, type::supplier<device::device_type> &&device,
	VkExtent2D extent, VkFormat format, const std::string &title,
	const initialize_callback_type &initialize_callback, const queue_callback_type &queue_callback,
	const resize_callback_type &resize_callback, const draw_callback_type &draw_callback, const input_callbacks_type &input_callbacks = input_callbacks_type());

VCC_LIBRARY int run(window_type &window);

}  // namespace window
}  // namespace vcc

#endif // _VCC_WINDOW_H_
