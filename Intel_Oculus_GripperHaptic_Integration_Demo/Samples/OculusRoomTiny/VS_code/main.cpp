/*****************************************************************************

Filename    :   main.cpp
Content     :   Simple minimal VR demo
Created     :   December 1, 2014
Author      :   Tom Heath
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

/*****************************************************************************/
/// This sample has not yet been fully assimiliated into the framework
/// and also the GL support is not quite fully there yet, hence the VR
/// is not that great!



#include "camera_class.h"
#define CAMERA_CLASS_ADDED
#include "../../OculusRoomTiny_Advanced/Common/Win32_GLAppUtil.h"


// Include the Oculus SDK
#include "OVR_CAPI_GL.h"

#if defined(_WIN32)
#include <dxgi.h> // for GetDefaultAdapterLuid
#pragma comment(lib, "dxgi.lib")
#endif

#include <librealsense2/rs.hpp>
//#include "example.hpp"
#include <vector>
#include <string>
#include <cpprest/ws_client.h>

#include <Eigen/Geometry>
#include <boost/shared_array.hpp>


bool gripper_commad = true;
bool touch_sende = true;

using namespace OVR;

struct OculusTextureBuffer
{
	ovrSession          Session;
	ovrTextureSwapChain ColorTextureChain;
	ovrTextureSwapChain DepthTextureChain;
	GLuint              fboId;
	Sizei               texSize;

	OculusTextureBuffer(ovrSession session, Sizei size, int sampleCount) :
		Session(session),
		ColorTextureChain(nullptr),
		DepthTextureChain(nullptr),
		fboId(0),
		texSize(0, 0)
	{
		assert(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

		texSize = size;

		// This texture isn't necessarily going to be a rendertarget, but it usually is.
		assert(session); // No HMD? A little odd.

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = size.w;
		desc.Height = size.h;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = sampleCount;
		desc.StaticImage = ovrFalse;

		{
			ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &ColorTextureChain);

			int length = 0;
			ovr_GetTextureSwapChainLength(session, ColorTextureChain, &length);

			if (OVR_SUCCESS(result))
			{
				for (int i = 0; i < length; ++i)
				{
					GLuint chainTexId;
					ovr_GetTextureSwapChainBufferGL(Session, ColorTextureChain, i, &chainTexId);
					glBindTexture(GL_TEXTURE_2D, chainTexId);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
			}
		}

		desc.Format = OVR_FORMAT_D32_FLOAT;

		{
			ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &DepthTextureChain);

			int length = 0;
			ovr_GetTextureSwapChainLength(session, DepthTextureChain, &length);

			if (OVR_SUCCESS(result))
			{
				for (int i = 0; i < length; ++i)
				{
					GLuint chainTexId;
					ovr_GetTextureSwapChainBufferGL(Session, DepthTextureChain, i, &chainTexId);
					glBindTexture(GL_TEXTURE_2D, chainTexId);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
			}
		}

		glGenFramebuffers(1, &fboId);
	}

	~OculusTextureBuffer()
	{
		if (ColorTextureChain)
		{
			ovr_DestroyTextureSwapChain(Session, ColorTextureChain);
			ColorTextureChain = nullptr;
		}
		if (DepthTextureChain)
		{
			ovr_DestroyTextureSwapChain(Session, DepthTextureChain);
			DepthTextureChain = nullptr;
		}
		if (fboId)
		{
			glDeleteFramebuffers(1, &fboId);
			fboId = 0;
		}
	}

	Sizei GetSize() const
	{
		return texSize;
	}

	void SetAndClearRenderSurface()
	{
		GLuint curColorTexId;
		GLuint curDepthTexId;
		{
			int curIndex;
			ovr_GetTextureSwapChainCurrentIndex(Session, ColorTextureChain, &curIndex);
			ovr_GetTextureSwapChainBufferGL(Session, ColorTextureChain, curIndex, &curColorTexId);
		}
		{
			int curIndex;
			ovr_GetTextureSwapChainCurrentIndex(Session, DepthTextureChain, &curIndex);
			ovr_GetTextureSwapChainBufferGL(Session, DepthTextureChain, curIndex, &curDepthTexId);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curColorTexId, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, curDepthTexId, 0);

		glViewport(0, 0, texSize.w, texSize.h);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_FRAMEBUFFER_SRGB);
	}

	void UnsetRenderSurface()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	}

	void Commit()
	{
		ovr_CommitTextureSwapChain(Session, ColorTextureChain);
		ovr_CommitTextureSwapChain(Session, DepthTextureChain);
	}
};

static ovrGraphicsLuid GetDefaultAdapterLuid()
{
	ovrGraphicsLuid luid = ovrGraphicsLuid();

#if defined(_WIN32)
	IDXGIFactory* factory = nullptr;

	if (SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory))))
	{
		IDXGIAdapter* adapter = nullptr;

		if (SUCCEEDED(factory->EnumAdapters(0, &adapter)))
		{
			DXGI_ADAPTER_DESC desc;

			adapter->GetDesc(&desc);
			memcpy(&luid, &desc.AdapterLuid, sizeof(luid));
			adapter->Release();
		}

		factory->Release();
	}
#endif

	return luid;
}


static int Compare(const ovrGraphicsLuid& lhs, const ovrGraphicsLuid& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(ovrGraphicsLuid));
}



/*
static unsigned char* TransforImage(const rs2::video_frame& frame,  camera_stream_prop& came_1_prop, int side_to_cut)
{
	auto width = frame.get_width();
	auto height = frame.get_height();
	unsigned char* data = (unsigned char*)frame.get_data();

	unsigned char * mode_frame;
	int new_size_array = came_1_prop.Get_new_width() * height * 4;
	//boost::shared_ptr<unsigned char[]> mode_frame(new unsigned char[new_size_array]);
	mode_frame = new unsigned char[new_size_array];

	int ele_count = 0;

	if (side_to_cut == 0) // Cut Right side of the Image To give to Left Lens
	{  // Assuming construction mode in the width direction
		for (int i = 0; i < height; i++) // iterator though the height of image
		{
			for (int j = 0; j < came_1_prop.Get_new_width(); j++) // iterator though the width of image
			{
				for (int k = 0; k < 4; k++) // iterator though the pixel's RGBA values
				{
					mode_frame[ele_count] = data[(i*width * 4 + j * 4 + k)]; // (i*width *4 + j*4 + k ) Defines pixel value to be copied to new frame
					ele_count++;
				}
			}
		}
	}
	else if (side_to_cut == 1)  //Cut Left side of the Image To give to Right Lens
	{
		for (int i = 0; i < height; i++) // iterator though the height of image
		{
			for (int j = (width - came_1_prop.Get_new_width()); j < width; j++) // iterator though the width of image
			{
				for (int k = 0; k < 4; k++) // iterator though the pixel's RGBA values
				{
					mode_frame[ele_count] = data[(i*width * 4 + j * 4 + k)]; // (i*width *4 + j*4 + k ) Defines pixel value to be copied to new frame
					ele_count++;
				}

			}
		}
	}
	else // Does not cut anything
		return data;

	//auto imagedata = mode_frame.get();
	//return imagedata;
	return mode_frame;

}
*/










// Caso se use esta funcao deve-se mudar o rs2::video_frame para rs2::frame
static unsigned char* TransforImage(const rs2::video_frame& frame, camera_stream_prop& came_1_prop, int side_to_cut)
{
	unsigned char* data = (unsigned char*)frame.get_data();
	return data;
}


















static GLuint Frame_to_Texture(bool rendertarget, Sizei size, int mipLevels, unsigned char * data)
{
	Sizei texSize = size;
	GLuint           fboId;
	GLuint           texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	if (rendertarget)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
	// GL_SRGB8_ALPHA8 Mudar para RGBA8 Talvez
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texSize.w, texSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	if (mipLevels > 1)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	if (rendertarget)
	{
		glGenFramebuffers(1, &fboId);
	}

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);

	return texId;
}

//////////////////////////////////////////
// Added functions 
//////////////////////////////////////////

ovrPosef subtract_operation(ovrPosef current_pose, ovrPosef origin_pose)
{
	ovrPosef relative_pose;
	// Position 
	relative_pose.Position.x = current_pose.Position.x - origin_pose.Position.x;
	relative_pose.Position.y = current_pose.Position.y - origin_pose.Position.y;
	relative_pose.Position.z = current_pose.Position.z - origin_pose.Position.z;

	Eigen::Quaterniond origin(origin_pose.Orientation.w, origin_pose.Orientation.x, origin_pose.Orientation.y, origin_pose.Orientation.z);
	Eigen::Quaterniond current(current_pose.Orientation.w, current_pose.Orientation.x, current_pose.Orientation.y, current_pose.Orientation.z);

	origin.normalize();
	current.normalize();

	Eigen::Quaterniond relative = origin.inverse() * current;
	// Orientation
	relative_pose.Orientation.x = relative.x();
	relative_pose.Orientation.y = relative.y();
	relative_pose.Orientation.z = relative.z();
	relative_pose.Orientation.w = relative.w();
	return relative_pose;
}

void print_pose(ovrPosef pose_to_print)
{
	std::cout << "--------------------------" << std::endl;
	std::cout << "Position : " << "x: " << pose_to_print.Position.x << "y: " << pose_to_print.Position.y << "z: " << pose_to_print.Position.z << std::endl;
	std::cout << "Orientation : " << "x: " << pose_to_print.Orientation.x << "y: " << pose_to_print.Orientation.y << "z: " << pose_to_print.Orientation.z << "w: " << pose_to_print.Orientation.w << std::endl;
	std::cout << "--------------------------" << std::endl;
}

void send_gripper_command(web::websockets::client::websocket_client& client, const ovrSession& session, const ovrInputState & inputState, ovrControllerType touch_side, const std::string& topic, web::websockets::client::websocket_outgoing_message& out_msg, ovrButton button_type, const std::vector<float>& freq_amp) {
	// freq_amp[0] Frequency type os vibration
	// freq_amp[1] Amplitude of vibration
	if (inputState.Buttons & button_type) {
		std::string content = "{\"data\": 1}";
		ovr_SetControllerVibration(session, touch_side, freq_amp[0], freq_amp[1]);
		out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic + "\", \"msg\":" + content + "}");
		client.send(out_msg).wait();
	}
	else if (!inputState.Buttons & button_type) {
		ovr_SetControllerVibration(session, touch_side, freq_amp[0], 0.0f);
		std::string content = "{\"data\": 0}";
		out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic + "\", \"msg\":" + content + "}");
		client.send(out_msg).wait();
	}
}


std::string PoseToROs(ovrPosef handpose) {
	std::stringstream content;
	content << "{\"position\":{ \"x\":" << handpose.Position.x << ", \"y\": " << handpose.Position.y << " , \"z\": " << handpose.Position.z << " } , \"orientation\": { \"x\": " << handpose.Orientation.x << ", \"y\":" << handpose.Orientation.y << ", \"z\":" << handpose.Orientation.z << ", \"w\":" << handpose.Orientation.w << "}}";
	return content.str();

}

// 
void send_touch_pose(web::websockets::client::websocket_client& client, const ovrSession& session, const ovrTrackingState& trackState, const ovrInputState & inputState, ovrHandType touch_side, const std::string& topic, web::websockets::client::websocket_outgoing_message& out_msg, const std::vector<float>& freq_amp, ovrPosef& relative, ovrPosef& lastpose, ovrControllerType touch_side_2) {
	// freq_amp[0] Frequency type os vibration
	// freq_amp[1] Amplitude of vibration
	if (inputState.HandTrigger[touch_side] > 0.5f) {
		ovr_SetControllerVibration(session, touch_side_2, freq_amp[0], freq_amp[1]); // Vibrate the Oculus touch according to the hand
		relative = subtract_operation(trackState.HandPoses[touch_side].ThePose, lastpose);
		std::string content = PoseToROs(relative);
		out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic + "\", \"msg\":" + content + "}");
		client.send(out_msg).wait();
	}
	else if (inputState.HandTrigger[touch_side] <= 0.5f) {
		lastpose = trackState.HandPoses[touch_side].ThePose;
		ovr_SetControllerVibration(session, touch_side_2, freq_amp[0], 0.0f); // Shutdown viration 
	}

}




//////////////////////////////////////////
//////////////////////////////////////////

// return true to retry later (e.g. after display lost)
static bool MainLoop(bool retryCreate)
{

	/////////////////////////////////
	// Define websocket Variables
	/////////////////////////////////

	using namespace web;
	using namespace web::websockets::client;
	websocket_client client;
	std::string topic, topic_pose_r, topic2, topic_pose_l;
	websocket_outgoing_message out_msg;

	if (gripper_commad == true || touch_sende == true) { // If any of the global variables is true then we have to connect to ROS
	// Create Websocket client and give him a URL
	//client.connect(L"ws://192.168.1.147:8080").wait();   // Server Ip Adress for Net Casa 
		client.connect(L"ws://192.168.0.0:9090").wait();   // Server Ip Adress for Ethernet cable to Ubuntu (Meu Pc pequeno)
		topic = "/pub", topic_pose_r = "/pub_pose_r", topic2 = "/pub2", topic_pose_l = "/pub_pose_l";;
	}



	// Lambda funciton to tranform the Oculus touch pose into a string that is sent to ROS by Websocket protocol
	auto pose_2_ros = [](ovrPosef handpose)
	{
		std::stringstream content;
		content << "{\"position\":{ \"x\":" << handpose.Position.x << ", \"y\": " << handpose.Position.y << " , \"z\": " << handpose.Position.z << " } , \"orientation\": { \"x\": " << handpose.Orientation.x << ", \"y\":" << handpose.Orientation.y << ", \"z\":" << handpose.Orientation.z << ", \"w\":" << handpose.Orientation.w << "}}";
		return content.str();
	};

	/////////////////////////////////
	/////////////////////////////////

	// Variables to get the touch button state

	bool HasInputState;
	ovrInputState    inputState;// Variables to get the touch button state
	std::vector<float> freq_amp(2, 0);
	/////////////////////////////////
	/////////////////////////////////

	//std::string path = "cv_logo.png";
	/*
	*/
	// devia ativar alpha display
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//////////////////////////////////////////////////////////////
	////////////////// Intelrealsense Pipeline //////////////////
	//////////////////////////////////////////////////////////////

	camera_stream_prop came_1_prop(640, 480, 60);
	//camera_stream_prop came_1_prop(1280, 720, 30);
	//camera_stream_prop came_1_prop(1920, 1080, 30);  // Added new Method of the New Width 

	//Contruct a pipeline which abstracts the device
	rs2::pipeline pipe;

	//Create a configuration for configuring the pipeline with a non default profile
	rs2::config cfg;

	//Add desired streams to configuration
	cfg.enable_stream(RS2_STREAM_COLOR, came_1_prop.Get_Resolution('w'), came_1_prop.Get_Resolution('h'), RS2_FORMAT_RGBA8, came_1_prop.Get_Resolution('f'));

	//Instruct pipeline to start streaming with the requested configuration
	pipe.start(cfg);

	Sizei* size;
	size = new Sizei(came_1_prop.Get_new_width(), came_1_prop.Get_Resolution('h'));

	//Sizei size{ came_1_prop.Get_new_width(), came_1_prop.Get_Resolution('h') };

	int global_new_width = came_1_prop.Get_new_width();

	//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
	//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\


	OculusTextureBuffer * eyeRenderTexture[2] = { nullptr, nullptr };
	ovrMirrorTexture mirrorTexture = nullptr;
	GLuint          mirrorFBO = 0;
	Scene         * roomScene = nullptr;
	long long frameIndex = 0;

	ovrSession session;
	ovrGraphicsLuid luid;
	ovrResult result = ovr_Create(&session, &luid);
	if (!OVR_SUCCESS(result))
		return retryCreate;

	if (Compare(luid, GetDefaultAdapterLuid())) // If luid that the Rift is on is not the default adapter LUID...
	{
		VALIDATE(false, "OpenGL supports only the default graphics adapter.");
	}

	ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session);

	// Setup Window and Graphics
	// Note: the mirror window can be any size, for this sample we use 1/2 the HMD resolution
	ovrSizei windowSize = { hmdDesc.Resolution.w / 2, hmdDesc.Resolution.h / 2 };

	//////////////////////////////////////////////
	// Variables to get the traking state of the head set and the Oculus Touch
	//////////////////////////////////////////////

	double displayMidpointSeconds = ovr_GetPredictedDisplayTime(session, frameIndex);
	ovrTrackingState trackState;
	ovrPosef         handPoses[2];
	ovrPosef lastpose[2], relative[2];
	//////////////////////////////////////////////
	//////////////////////////////////////////////

	if (!Platform.InitDevice(windowSize.w, windowSize.h, reinterpret_cast<LUID*>(&luid)))
		goto Done;

	// Make eye render buffers
	for (int eye = 0; eye < 2; ++eye)
	{
		ovrSizei idealTextureSize = ovr_GetFovTextureSize(session, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], 1);
		eyeRenderTexture[eye] = new OculusTextureBuffer(session, idealTextureSize, 1);

		if (!eyeRenderTexture[eye]->ColorTextureChain || !eyeRenderTexture[eye]->DepthTextureChain)
		{
			if (retryCreate) goto Done;
			VALIDATE(false, "Failed to create texture.");
		}
	}

	ovrMirrorTextureDesc desc;
	memset(&desc, 0, sizeof(desc));
	desc.Width = windowSize.w;
	desc.Height = windowSize.h;
	desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

	// Create mirror texture and an FBO used to copy mirror texture to back buffer
	result = ovr_CreateMirrorTextureWithOptionsGL(session, &desc, &mirrorTexture);
	if (!OVR_SUCCESS(result))
	{
		if (retryCreate) goto Done;
		VALIDATE(false, "Failed to create mirror texture.");
	}

	// Configure the mirror read buffer
	GLuint texId;
	ovr_GetMirrorTextureBufferGL(session, mirrorTexture, &texId);

	glGenFramebuffers(1, &mirrorFBO);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
	glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	// Turn off vsync to let the compositor do its magic
	wglSwapIntervalEXT(0);

	////////////////////////////////////////////////////////////////////////////
	//////////////// Build the scene with all the elements /////////////
	////////////////////////////////////////////////////////////////////////////
	/*
	*/
	roomScene = new Scene(false, came_1_prop); // Commented I will try to initialize the world in the while loop so the boardwhere the texture is displayed is changes according to the new width

	//roomScene = new Scene(false, color_frame);
	//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
	//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

	// FloorLevel will give tracking poses where the floor height is 0
	ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);


	// Send the last frame recorded during the for loop in the begining of the MainLopp function 

// Main loop
	while (Platform.HandleMessages())
	{
		////////////////////////////////////////////////////////////////////////////
		//////////////// Get frames from the IntelRealsense Camera And build the Sceen with the size of the board updated /////////////
		////////////////////////////////////////////////////////////////////////////

		rs2::frameset frames = pipe.wait_for_frames();
		rs2::frame color_frame = frames.get_color_frame();  ///// Alterei ( rs2::frame ) para	( rs2::video_frame )

		//roomScene = new Scene(false, came_1_prop);  // The roomScene has to be deleted in the end of the Loop 
		//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
		//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\


		ovrSessionStatus sessionStatus;
		ovr_GetSessionStatus(session, &sessionStatus);
		if (sessionStatus.ShouldQuit)
		{
			// Because the application is requested to quit, should not request retry
			retryCreate = false;
			break;
		}
		if (sessionStatus.ShouldRecenter)
			ovr_RecenterTrackingOrigin(session);

		if (sessionStatus.IsVisible)
		{

			// Keyboard inputs to adjust the width of the frame
			/*
			if (Platform.Key[VK_LEFT] && --global_new_width >= came_1_prop.Get_Resolution('h') )
			{
				global_new_width -= 1;
				came_1_prop.Set_new_Width(global_new_width);
				delete size;
				size = new Sizei( came_1_prop.Get_new_width(), came_1_prop.Get_Resolution('h') );

			}
			if (Platform.Key[VK_RIGHT] && ++global_new_width <= came_1_prop.Get_Resolution('w'))
			{
				global_new_width += 1;
				came_1_prop.Set_new_Width(global_new_width);
				delete size;
				size = new Sizei(came_1_prop.Get_new_width(), came_1_prop.Get_Resolution('h'));

			}
			*/


			// Keyboard inputs to adjust player orientation
			static float Yaw(3.141592f);
			//if (Platform.Key[VK_LEFT])  Yaw += 0.02f;
			//if (Platform.Key[VK_RIGHT]) Yaw -= 0.02f;

			// Keyboard inputs to adjust player position
			static Vector3f Pos2(0.0f, 0.0f, -5.0f);
			if (Platform.Key['W'] || Platform.Key[VK_UP])     Pos2 += Matrix4f::RotationY(Yaw).Transform(Vector3f(0, 0, -0.05f));
			if (Platform.Key['S'] || Platform.Key[VK_DOWN])   Pos2 += Matrix4f::RotationY(Yaw).Transform(Vector3f(0, 0, +0.05f));
			if (Platform.Key['D'])                            Pos2 += Matrix4f::RotationY(Yaw).Transform(Vector3f(+0.05f, 0, 0));
			if (Platform.Key['A'])                            Pos2 += Matrix4f::RotationY(Yaw).Transform(Vector3f(-0.05f, 0, 0));

			/////////////////////////////////////
			// Get the Touch input Button and send to a ros Topic (TODO: Get the pose for each Oculus touch and send it to ROS)
			/////////////////////////////////////

			trackState = ovr_GetTrackingState(session, displayMidpointSeconds, ovrTrue);  // Has the Pose of the Touch controllers and the Head Set

			HasInputState = (ovr_GetInputState(session, ovrControllerType_Touch, &inputState) == ovrSuccess);

			if (HasInputState) {

				if (gripper_commad) { // Send the gripper commands to open and close

					freq_amp.at(0) = 0.0;
					freq_amp.at(1) = 200.0;
					send_gripper_command(client, session, inputState, ovrControllerType_RTouch, topic, out_msg, ovrButton_A, freq_amp);
					send_gripper_command(client, session, inputState, ovrControllerType_LTouch, topic2, out_msg, ovrButton_X, freq_amp);
				}

				if (touch_sende) { // Send the touch pose to ros
					freq_amp.at(0) = 1.0;
					freq_amp.at(1) = 0.0;
					send_touch_pose(client, session, trackState, inputState, ovrHand_Right, topic_pose_r, out_msg, freq_amp, relative[1], lastpose[1], ovrControllerType_RTouch);
					send_touch_pose(client, session, trackState, inputState, ovrHand_Left, topic_pose_l, out_msg, freq_amp, relative[0], lastpose[0], ovrControllerType_LTouch);
				}
				/*
				*/

				/*

				// Right button manger to Open and close the Gripper
				if (inputState.Buttons & ovrButton_A) {
					std::string content = "{\"data\": 1}";
					ovr_SetControllerVibration(session, ovrControllerType_RTouch, 0.0f, 200.0f);

					if (sessionStatus.HasInputFocus) // Pause the application if we are not supposed to have input.
						out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic + "\", \"msg\":" + content + "}");
						client.send(out_msg).wait();
				}
				else if (!inputState.Buttons & ovrButton_A) {
					ovr_SetControllerVibration(session, ovrControllerType_RTouch, 0.0f, 0.0f);
					std::string content = "{\"data\": 0}";
					out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic + "\", \"msg\":" + content + "}");
					client.send(out_msg).wait();
				}


				// Right button manger to Open and close the Gripper
				if (inputState.Buttons & ovrButton_X) {
					std::string content = "{\"data\": 1}";
					ovr_SetControllerVibration(session, ovrControllerType_RTouch, 0.0f, 200.0f);

					if (sessionStatus.HasInputFocus) // Pause the application if we are not supposed to have input.
						out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic2 + "\", \"msg\":" + content + "}");
					client.send(out_msg).wait();

				}
				else if (!inputState.Buttons & ovrButton_X) {
					ovr_SetControllerVibration(session, ovrControllerType_RTouch, 0.0f, 0.0f);
					std::string content = "{\"data\": 0}";
					out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic2 + "\", \"msg\":" + content + "}");
					client.send(out_msg).wait();
				}

				////////////////////////////////////////////////////
				*/


				////////////////////////////////////////////////
				////////////////////////////////////////////////
				/*
				// Right touch send pose to ROS
				if (inputState.HandTrigger[ovrHand_Right] > 0.5f) {

					relative[1] = subtract_operation(trackState.HandPoses[ovrHand_Right].ThePose, lastpose[1]);
					std::string content = pose_2_ros(relative[1]);

					out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic_pose_r + "\", \"msg\":" + content + "}");
					client.send(out_msg).wait();
				}
				else if (inputState.HandTrigger[ovrHand_Right] <= 0.5f) {
					lastpose[1] = trackState.HandPoses[ovrHand_Right].ThePose;
				}

				// Left touch send pose to ROS
				if (inputState.HandTrigger[ovrHand_Left] > 0.5f) {

					relative[0] = subtract_operation(trackState.HandPoses[ovrHand_Left].ThePose, lastpose[0]);
					std::string content = pose_2_ros(relative[0]);

					out_msg.set_utf8_message("{\"op\":\"publish\", \"topic\":\"" + topic_pose_l + "\", \"msg\":" + content + "}");
					client.send(out_msg).wait();
				}
				else if (inputState.HandTrigger[ovrHand_Left] <= 0.5f) {
					lastpose[0] = trackState.HandPoses[ovrHand_Left].ThePose;
				}
				*/
				////////////////////////////////////////////////
				////////////////////////////////////////////////
			}
			/*
			*/

			/////////////////////////////////////
			/////////////////////////////////////


			// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyePose) may change at runtime.
			ovrEyeRenderDesc eyeRenderDesc[2];
			eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
			eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

			// Get eye poses, feeding in correct IPD offset
			ovrPosef EyeRenderPose[2];
			ovrPosef HmdToEyePose[2] = { eyeRenderDesc[0].HmdToEyePose,
										 eyeRenderDesc[1].HmdToEyePose };

			double sensorSampleTime;    // sensorSampleTime is fed into the layer later
			ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyePose, EyeRenderPose, &sensorSampleTime);

			ovrTimewarpProjectionDesc posTimewarpProjectionDesc = {};


			// Render Scene to Eye Buffers
			for (int eye = 0; eye < 2; ++eye)
			{
				// Switch to eye render target
				eyeRenderTexture[eye]->SetAndClearRenderSurface();

				// Get view and projection matrices
				Matrix4f rollPitchYaw = Matrix4f::RotationY(Yaw);
				Matrix4f finalRollPitchYaw = rollPitchYaw * Matrix4f(EyeRenderPose[eye].Orientation);
				Vector3f finalUp = finalRollPitchYaw.Transform(Vector3f(0, 1, 0));
				Vector3f finalForward = finalRollPitchYaw.Transform(Vector3f(0, 0, -1));
				Vector3f shiftedEyePos = Pos2 + rollPitchYaw.Transform(EyeRenderPose[eye].Position);

				Matrix4f view = Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
				Matrix4f proj = ovrMatrix4f_Projection(hmdDesc.DefaultEyeFov[eye], 0.2f, 1000.0f, ovrProjection_None);
				posTimewarpProjectionDesc = ovrTimewarpProjectionDesc_FromProjection(proj, ovrProjection_None);

				////////////////////////////////////////////////////////////////////////////
				////////////////// Cut the frame from the camera by an amount Delta Width /////////////
				////////////////////////////////////////////////////////////////////////////


				// unsigned char* frame_data = TransforImage( color_frame,  came_1_prop,  eye);  // If you want to cut the edges of the image (Not necessary)
				unsigned char* frame_data = (unsigned char*)color_frame.get_data();

				////////////////////////////////////////////////////////////////////////////
				////////////////// Get texture out of the camera frame and send it to texture field of the Model Struct /////////////
				////////////////////////////////////////////////////////////////////////////

				GLuint texture = Frame_to_Texture(false, *size, 4, frame_data);

				int ModelesInScene = roomScene->numModels; // get number of elements in the scene
				roomScene->Models[--ModelesInScene]->SetTexture(texture);
				/*
				*/
				//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
		 		//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

				////////////////////////////////////////////////////////////////////////////
				////////////////// Render world /////////////
				////////////////////////////////////////////////////////////////////////////

				roomScene->Render(view, proj);

				//\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
	/			/\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

				// Avoids an error when calling SetAndClearRenderSurface during next iteration.
				// Without this, during the next while loop iteration SetAndClearRenderSurface
				// would bind a framebuffer with an invalid COLOR_ATTACHMENT0 because the texture ID
				// associated with COLOR_ATTACHMENT0 had been unlocked by calling wglDXUnlockObjectsNV.
				eyeRenderTexture[eye]->UnsetRenderSurface();

				// Commit changes to the textures so they get picked up frame
				eyeRenderTexture[eye]->Commit();
				// Delete texture from the frame
				glDeleteTextures(1, &texture);
				/*
				*/
			}

			// Do distortion rendering, Present and flush/sync

			ovrLayerEyeFovDepth ld = {};
			ld.Header.Type = ovrLayerType_EyeFovDepth;
			ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.
			ld.ProjectionDesc = posTimewarpProjectionDesc;
			ld.SensorSampleTime = sensorSampleTime;

			for (int eye = 0; eye < 2; ++eye)
			{
				ld.ColorTexture[eye] = eyeRenderTexture[eye]->ColorTextureChain;
				ld.DepthTexture[eye] = eyeRenderTexture[eye]->DepthTextureChain;
				ld.Viewport[eye] = Recti(eyeRenderTexture[eye]->GetSize());
				ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
				ld.RenderPose[eye] = EyeRenderPose[eye];
			}

			ovrLayerHeader* layers = &ld.Header;
			result = ovr_SubmitFrame(session, frameIndex, nullptr, &layers, 1);
			// exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
			if (!OVR_SUCCESS(result))
				goto Done;

			frameIndex++;
		}

		// Blit mirror texture to back buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		GLint w = windowSize.w;
		GLint h = windowSize.h;
		glBlitFramebuffer(0, h, w, 0,
			0, 0, w, h,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

		SwapBuffers(Platform.hDC);

		/* Delete the roomscene POiter
		delete roomScene;
		*/
	}

Done:
	delete size;
	delete roomScene;
	if (mirrorFBO) glDeleteFramebuffers(1, &mirrorFBO);
	if (mirrorTexture) ovr_DestroyMirrorTexture(session, mirrorTexture);
	for (int eye = 0; eye < 2; ++eye)
	{
		delete eyeRenderTexture[eye];
	}
	Platform.ReleaseDevice();
	ovr_Destroy(session);

	// Retry on ovrError_DisplayLost
	return retryCreate || (result == ovrError_DisplayLost);
}

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	// Initializes LibOVR, and the Rift
	ovrInitParams initParams = { ovrInit_RequestVersion | ovrInit_FocusAware, OVR_MINOR_VERSION, NULL, 0, 0 };
	ovrResult result = ovr_Initialize(&initParams);
	VALIDATE(OVR_SUCCESS(result), "Failed to initialize libOVR.");

	VALIDATE(Platform.InitWindow(hinst, L"Oculus Room Tiny (GL)"), "Failed to open window.");

	Platform.Run(MainLoop);

	ovr_Shutdown();

	return(0);
}
