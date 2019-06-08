#pragma once
#include <vector>
#include <librealsense2/rs.hpp>

class camera_stream_prop
{
private:
	std::vector <int> resol_vec;
	rs2_stream type_stream;
	rs2_format type_formate;
	float frame_ratio;
	int width_new_frame;
public:
	camera_stream_prop()
	{
		resol_vec.push_back(640);
		resol_vec.push_back(480);
		resol_vec.push_back(30);
	}
	camera_stream_prop(int width_input, int height_input, int frame_rate, int new_width) :frame_ratio(1.00), width_new_frame(new_width)
	{
		resol_vec.push_back(width_input);
		resol_vec.push_back(height_input);
		resol_vec.push_back(frame_rate);
	}

	camera_stream_prop(int width_input, int height_input, int frame_rate) :frame_ratio(1), width_new_frame(width_input)
	{
		resol_vec.push_back(width_input);
		resol_vec.push_back(height_input);
		resol_vec.push_back(frame_rate);
	}

	~camera_stream_prop()
	{
		resol_vec.clear();
		resol_vec.shrink_to_fit();
	}
	int  Get_Resolution(char dimension)
	{
		int resol_dim;

		if (dimension == 'w')
		{
			return (resol_vec[0]);
		}
		else if (dimension == 'h') {
			return resol_vec[1];
		}
		else if (dimension == 'f')
		{
			return resol_vec[2];
		}
		else
			return 640;
	}
	float Get_frame_ratio()
	{
		frame_ratio = ((float)width_new_frame) / ((float)resol_vec[1]);
		return frame_ratio;
	}

	int Get_new_width() {
		return width_new_frame;
	}

	void Set_new_Width(int new_width_value)
	{
		width_new_frame = new_width_value;
	}

};