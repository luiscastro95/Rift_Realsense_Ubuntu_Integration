#pragma once

#include <iostream>
#include "librealsense2/rs.hpp"
#include <thread>
#include <mutex>

// Packed data for threaded computation
struct ThreadData_Intel {
	rs2::pipeline pipe;
	rs2::config cfg;
	int height, width;
	unsigned char* data;
	std::mutex mtx;
	bool run;
	bool new_frame;

	float get_frame_ratio() {
		return (float(width) / float(height));
	}

	ThreadData_Intel(rs2::pipeline pipe_, rs2::config cfg_, int height_, int width_) {
		pipe = pipe_;
		cfg = cfg_;
		height, width = height_, width_;
	}
};


// Intel Realsense Image Thread
void __intel_runner__(ThreadData_Intel &thread_data) {

	thread_data.pipe.start(thread_data.cfg);

	// Loop while the main loop is not over
	while (thread_data.run) {
		// try to grab a new image

		thread_data.mtx.lock();
		rs2::frameset frames = thread_data.pipe.wait_for_frames();
		rs2::video_frame color_frame = frames.get_color_frame();
		thread_data.data = (unsigned char*)color_frame.get_data();
		thread_data.mtx.unlock();
		thread_data.new_frame = true;
	}
}