
#include "WaterApp.h"

// add image
void WaterApp::addImage( const fs::path &imagePath )
{
	try 
	{
		if( ! imagePath.empty() ) 
		{ 
			mImage = Channel32f( loadImage( imagePath ) );
			gl::Texture::Format format;
			format.setInternalFormat( GL_RGBA32F_ARB );
			mTexture = gl::Texture( mImage );
			mTexture.setWrap( GL_REPEAT, GL_REPEAT );

		}
	}
	catch( ... ) {
		console() << "Unable to load the image." << std::endl;
	}
}
void WaterApp::fileDrop( FileDropEvent event )
{
	addImage( event.getFile( 0 ) );
}
void WaterApp::prepareSettings( Settings *settings )
{

	settings->setWindowSize( 450, 400 );
	settings->setFrameRate( 60.0f );
	settings->enableConsoleWindow();
}
void WaterApp::setup()
{
	// Display sizes
	mMainDisplayWidth = Display::getMainDisplay()->getWidth();
	mRenderX = mMainDisplayWidth;
	mRenderY = 0;
	for (auto display : Display::getDisplays() )
	{
		//std::cout << "Reso:" << display->getHeight() << "\n"; 
		mRenderWidth = display->getWidth();
		mRenderHeight = display->getHeight();
	}
	kWindowSize	= Vec2i( mRenderWidth, mRenderHeight );
	kPixel		= Vec2f::one() / Vec2f( kWindowSize );

	mMousePos = getWindowCenter();
	mShowInput	= false;

	// Load shaders
	try {
		mShaderGpGpu = gl::GlslProg( loadResource( RES_PASS_THRU_VERT ), loadResource( RES_GPGPU_FRAG ) );
	} catch ( gl::GlslProgCompileExc ex ) {
		console() << "Unable to compile GPGPU shader:\n" << ex.what() << "\n";
		quit();
	}
	try {
		mShaderRefraction = gl::GlslProg( loadResource( RES_PASS_THRU_VERT ), loadResource( RES_REFRACTION_FRAG ) );
	} catch ( gl::GlslProgCompileExc ex ) {
		console() << "Unable to compile refraction shader:\n" << ex.what() << "\n";
		quit();
	}
	// Load refraction texture
	{
		gl::Texture::Format format;
		format.setInternalFormat( GL_RGBA32F_ARB );
		mTexture = gl::Texture( loadImage( loadResource( RES_TEXTURE ) ) );
		mTexture.setWrap( GL_REPEAT, GL_REPEAT );
	}
	// Create FBO
	{
		// Set up format with 32-bit color for high resolution data
		gl::Fbo::Format format;
		format.enableColorBuffer( true, 2 );
		format.setMagFilter( GL_NEAREST );
		format.setMinFilter( GL_NEAREST );
		format.setColorInternalFormat( GL_RGBA32F_ARB );

		// Create a frame buffer object with two attachments ping pong
		mFboIndex	= 0;
		mGpFbo		= gl::Fbo( kWindowSize.x, kWindowSize.y, format );
		mGpFbo.bindFramebuffer();
		const GLenum buffers[ 2 ] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers( 2, buffers );
		gl::setViewport( mGpFbo.getBounds() );
		gl::clear();
		mGpFbo.unbindFramebuffer();
		for ( size_t i = 0; i < 2; ++i ) {
			mGpFbo.getTexture( i ).setWrap( GL_REPEAT, GL_REPEAT );
		}
	}
	mParams = params::InterfaceGl( "Params", Vec2i( 400, 300 ) );
	mParams.addParam( "Full screen",			&mFullScreen,									"key=f"		);
	mParams.addParam( "Render Window X",		&mRenderX,										"" );
	mParams.addParam( "Render Window Y",		&mRenderY,										"" );
	mParams.addParam( "Render Window Width",	&mRenderWidth,									"" );
	mParams.addParam( "Render Window Height",	&mRenderHeight,									"" );
	mParams.addButton( "Create window",			bind( &WaterApp::createNewWindow, this ),		"key=n" );
	mParams.addButton( "Delete windows",		bind( &WaterApp::deleteWindows, this ),		"key=d" );
	mParams.addButton( "Quit",					bind( &WaterApp::shutdown, this ),			"key=q" );

	//store window
	controlWindow = this->getWindow();
	int uniqueId = getNumWindows();
	controlWindow->getSignalClose().connect(
		[uniqueId,this] { shutdown(); this->console() << "You quit console window #" << uniqueId << std::endl; }
	);
	receiver.setup( 10008 );
	createNewWindow();
}
void WaterApp::createNewWindow()
{
	WindowRef renderWindow = createWindow( Window::Format().size( mRenderWidth, mRenderHeight ) );
	// create instance of the window and store in vector
	RenderWindow rWin = RenderWindow("name", mRenderWidth, mRenderHeight, renderWindow);	
	renderWindows.push_back( rWin );
	renderWindow->setPos(mRenderX, mRenderY);
	renderWindow->setBorderless();
	renderWindow->setAlwaysOnTop();

	HWND hWnd = (HWND)renderWindow->getNative();

	HRESULT hr = S_OK;
	// Create and populate the Blur Behind structure
	DWM_BLURBEHIND bb = {0};

	// Enable Blur Behind and apply to the entire client area
	bb.dwFlags = DWM_BB_ENABLE;
	bb.fEnable = true;
	bb.hRgnBlur = NULL;

	// Apply Blur Behind
	hr = DwmEnableBlurBehindWindow(hWnd, &bb);
	if (SUCCEEDED(hr))
	{
		HRESULT hr = S_OK;

		// Set the margins, extending the bottom margin.
		MARGINS margins = {-1};

		// Extend the frame on the bottom of the client area.
		hr = DwmExtendFrameIntoClientArea(hWnd,&margins);
	}

	// for demonstration purposes, we'll connect a lambda unique to this window which fires on close
	int uniqueId = getNumWindows();
	renderWindow->getSignalClose().connect(
		[uniqueId,this] { shutdown(); this->console() << "You closed window #" << uniqueId << std::endl; }
	);

}
void WaterApp::deleteWindows()
{
	for ( auto wRef : renderWindows ) DestroyWindow( (HWND)wRef.mWRef->getNative() );
}
void WaterApp::shutdown()
{
	WaterApp::quit();
}
void WaterApp::quitProgram()
{
	shutdown();
}
void WaterApp::mouseDown( MouseEvent event )
{
	mMouseDown = true;
	mouseDrag( event );

}
void WaterApp::mouseDrag( MouseEvent event )
{
	mMousePos = event.getPos();
}

void WaterApp::mouseUp( MouseEvent event )
{
	mMouseDown = false;
}
void WaterApp::update()
{
	while( receiver.hasWaitingMessages() ) {
		osc::Message m;
		receiver.getNextMessage( &m );

		console() << "New message received" << std::endl;
		console() << "Address: " << m.getAddress() << std::endl;
		console() << "Num Arg: " << m.getNumArgs() << std::endl;
		if(m.getAddress() == "/mouse/position"){
			// both the arguments are int32's
			Vec2i pos = Vec2i( m.getArgAsInt32(0), m.getArgAsInt32(1));
			mMousePos = pos;
			if ( m.getArgAsInt32(2) == 1 )
			{
				mMouseDown = true;
			}
			else
			{
				mMouseDown = false;
			}
			if ( mMouseDown )
			{
				mCurrentMouseDown = pos;
			}
		}
		// check for mouse button message
		else if(m.getAddress() == "/mouse/button"){
			// the single argument is a string
			Vec2i pos = Vec2i( m.getArgAsInt32(0), m.getArgAsInt32(1));
			mCurrentMouseDown = pos;
			if ( m.getArgAsInt32(2) == 1 )
			{
				mMouseDown = true;
			}
			else
			{
				mMouseDown = false;
			}
		}
		else if(m.getAddress() == "/water/loadimage"){
			fs::path imagePath = m.getArgAsString(0);
			addImage( imagePath );
		}			
		else if(m.getAddress() == "/quit"){
			quitProgram();
		}		
		else{
			// unrecognized message
			//cout << "not recognized:" << m.getAddress() << endl;
		}

	}
}
void WaterApp::drawFullScreenRect()
{
	// Begin drawing
	gl::begin( GL_TRIANGLES );

	// Define quad vertices
	Area bounds = mGpFbo.getBounds();//getWindowBounds();
	Vec2f vert0( (float)bounds.x1, (float)bounds.y1 );
	Vec2f vert1( (float)bounds.x2, (float)bounds.y1 );
	Vec2f vert2( (float)bounds.x1, (float)bounds.y2 );
	Vec2f vert3( (float)bounds.x2, (float)bounds.y2 );

	// Define quad texture coordinates
	Vec2f uv0( 0.0f, 0.0f );
	Vec2f uv1( 1.0f, 0.0f );
	Vec2f uv2( 0.0f, 1.0f );
	Vec2f uv3( 1.0f, 1.0f );

	// Draw quad (two triangles)
	gl::texCoord( uv0 );
	gl::vertex( vert0 );
	gl::texCoord( uv2 );
	gl::vertex( vert2 );
	gl::texCoord( uv1 );
	gl::vertex( vert1 );

	gl::texCoord( uv1 );
	gl::vertex( vert1 );
	gl::texCoord( uv2 );
	gl::vertex( vert2 );
	gl::texCoord( uv3 );
	gl::vertex( vert3 );

	// End drawing
	gl::end();
}
void WaterApp::draw()
{
	// clear out the window with transparency
	gl::clear( ColorAf( 0.0f, 0.0f, 0.0f, 0.0f ) );

	// Draw on render window only
	if (app::getWindow() == controlWindow)	
	{
		// Draw the params on control window only
		mParams.draw();
	}
	else
	{
		gl::color( Color::white() );

		// GPGPU pass
		// Enable textures
		gl::enable( GL_TEXTURE_2D );

		// Bind the other FBO to draw onto it
		size_t pong = ( mFboIndex + 1 ) % 2;
		mGpFbo.bindFramebuffer();
		glDrawBuffer( GL_COLOR_ATTACHMENT0 + pong );

		// Set up the window to match the FBO
		gl::setViewport( mGpFbo.getBounds() );
		gl::setMatricesWindow( mGpFbo.getSize(), false );// false to prevent vertical flipping

		// Bind the texture from the FBO on which we last 
		// wrote data
		mGpFbo.bindTexture( 0, mFboIndex );

		// Bind and configure the GPGPU shader
		mShaderGpGpu.bind();
		mShaderGpGpu.uniform( "pixel", kPixel );
		mShaderGpGpu.uniform( "texBuffer", 0 ); 


		// Draw a fullscreen rectangle to process data
		drawFullScreenRect();

		// End shader output
		mShaderGpGpu.unbind();

		// Unbind and disable textures
		mGpFbo.unbindTexture();
		gl::disable( GL_TEXTURE_2D );

		// Draw mouse input into red channel
		if ( mMouseDown ) {
			gl::color( ColorAf( 1.0f, 0.0f, 0.0f, 1.0f ) );
			gl::drawSolidCircle( Vec2f( mMousePos ), 5.0f, 32 );
			gl::color( Color::white() );
		}

		// Stop drawing to FBO
		mGpFbo.unbindFramebuffer();

		// Swap FBOs
		mFboIndex = pong;

		///////////////////////////////////////////////////////////////
		// Refraction pass

		// Clear screen and set up viewport
		//gl::clear( Color::black() );
		gl::setViewport( getWindowBounds() );
		gl::setMatricesWindow( getWindowSize() );

		// This flag draws the raw data without refraction
		if ( mShowInput ) 
		{
			gl::draw( mGpFbo.getTexture( mFboIndex ) );
		} 
		else 
		{

			// Bind the FBO we last rendered as a texture
			mGpFbo.bindTexture( 0, mFboIndex );

			// Bind and enable the refraction texture
			gl::enable( GL_TEXTURE_2D );
			mTexture.bind( 1 );

			// Bind and configure the refraction shader
			mShaderRefraction.bind();
			mShaderRefraction.uniform( "pixel", kPixel );
			mShaderRefraction.uniform( "texBuffer", 0 );
			mShaderRefraction.uniform( "iChannel0", 1 );

			// Fill the screen with the shader output
			drawFullScreenRect();

			// Unbind and disable the texture
			mTexture.unbind();
			gl::disable( GL_TEXTURE_2D );

			// End shader output
			mShaderRefraction.unbind();
		}

	}
}

CINDER_APP_NATIVE( WaterApp, RendererGl )
