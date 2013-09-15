#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/ImageIo.h"
#include "cinder/params/Params.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Fbo.h"
#include "Resources.h"
#include "dwmapi.h"
#include "OscListener.h"

using namespace ci;
using namespace ci::app;
using namespace std;
// RenderWindow class
class RenderWindow
{
	public:
		RenderWindow( string name, int width, int height, WindowRef wRef )
			: mName( name ), mWidth ( width ), mHeight ( height ), mWRef ( wRef )
		{
			
		}
		
		WindowRef mWRef;
		
	private:
		string mName;
		int mWidth;
		int mHeight;
		
};
class WaterApp : public AppNative {
  public:
	void prepareSettings( AppBasic::Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );	
	void mouseDrag( MouseEvent event );	
	void mouseUp( MouseEvent event );	
	void update();
	void draw();
	void fileDrop( FileDropEvent event );
	void shutdown();
	// image
	void addImage( const fs::path &imagePath );
	void drawFullScreenRect();
	// windows mgmt
	void createNewWindow();
	void deleteWindows();
	vector<RenderWindow>	renderWindows;
	WindowRef				controlWindow;
private:
	gl::Texture				mImage;
	// windows and params
	int						mMainDisplayWidth;
	int						mRenderX;
	int						mRenderY;
	int						mRenderWidth;
	int						mRenderHeight;
	float					mFrameRate;
	bool					mFullScreen;
	ci::params::InterfaceGl	mParams;
	Vec2i					mMousePos;
	Vec2i					mCurrentMouseDown;
	// Frame buffer objects to ping pong
	ci::gl::Fbo				mGpFbo;
	size_t					mFboIndex;
	// Shaders
	ci::gl::GlslProg		mShaderGpGpu;
	ci::gl::GlslProg		mShaderRefraction;
	// Refraction texture
	ci::gl::Texture			mTexture;
	// Mouse
	bool					mMouseDown;
	// True renders input to screen
	bool					mShowInput;
	Vec2i					kWindowSize;
	Vec2f					kPixel;

	// Reymenta
	void					quitProgram();
	ColorAf					mBackgroundColor;
	ColorAf					mColor;
	osc::Listener 			receiver;
};