/*
 Copyright (c) 2012, The Cinder Project, All rights reserved.
 Copyright (c) Microsoft Open Technologies, Inc. All rights reserved.

 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "cinder/Cinder.h"
#include "cinder/app/Platform.h"
#include "cinder/app/Renderer.h"
#include "cinder/app/Window.h"
#include "cinder/Vector.h"
#include "cinder/app/MouseEvent.h"
#include "cinder/app/KeyEvent.h"
#include "cinder/app/FileDropEvent.h"

#include "cinder/Display.h"
#include "cinder/DataSource.h"
#include "cinder/Timer.h"
#include "cinder/Function.h"
#include "cinder/Thread.h"

#include <vector>
#include <algorithm>

namespace cinder {
class Timeline;
} // namespace cinder

namespace asio {
class io_service;
} // namespace asio

namespace cinder { namespace app {

//! Returns true if any slots return true, else false. Does not short-circuit. Returns true if there are no slots.
struct BooleanOrEventCombiner {
	typedef bool	result_type;

	template<typename InputIterator>
	bool	operator()( InputIterator first, InputIterator last ) const
	{
		bool handled = ( first == last ) ? true : false;
		while( first != last )
			handled = *first++ || handled;
		
		return handled;
	}
};

//! Returns true if all slots return true, else false. Does not short-circuit. Returns true if there are no slots.
struct BooleanAndEventCombiner {
	typedef bool	result_type;

	template<typename InputIterator>
	bool	operator()( InputIterator first, InputIterator last ) const
	{
		bool result = true;
		while( first != last )
			result = *first++ && result;
		
		return result;
	}
};

//! Returns a bitmask where in order for the bit in type T to be be 1, it has to be 1 from all slot.  Returns 0 if there are no slots.
template<typename T>
struct BitwiseAndEventCombiner {
	typedef T	result_type;

	template<typename InputIterator>
	T	operator()( InputIterator first, InputIterator last ) const
	{
		if( first == last )
			return 0;
		T mask = *first++;
		while( first != last )
			mask &= *first++;

		return mask;
	}
};

//! Base class that all apps derive from.
class AppBase {
 public:
	//! Startup settings, used during App construction. They are modified before the app is created by passing a SettingsFn to the app instanciation macros.
	class Settings {
	  public:
		Settings();
		virtual ~Settings() {}

		//! Sets the size of the default window measured in pixels
		void	setWindowSize( int windowSizeX, int windowSizeY ) { mDefaultWindowFormat.setSize( ivec2( windowSizeX, windowSizeY ) ); }
		//! Sets the size of the default window measured in pixels
		void	setWindowSize( const ivec2 &size ) { mDefaultWindowFormat.setSize( size ); }
		//! Gets the size of the default window measured in pixels
		ivec2	getWindowSize() const { return mDefaultWindowFormat.getSize(); }
		
		//! Returns the position of the default window in screen coordinates measured in pixels
		ivec2	getWindowPos() const { return mDefaultWindowFormat.getPos(); }
		//! Sets the position of the default window in screen coordinates measured in pixels
		void    setWindowPos( int windowPosX, int windowPosY ) { mDefaultWindowFormat.setPos( ivec2( windowPosX, windowPosY ) ); }
		//! Sets the position of the default window in screen coordinates measured in pixels
		void    setWindowPos( const ivec2 &windowPos ) { mDefaultWindowFormat.setPos( windowPos ); }
		//! Returns whether a non-default window position has been requested
		bool	isWindowPosSpecified() const { return mDefaultWindowFormat.isPosSpecified(); }
		//! Marks the window position setting as unspecified, effectively requesting the default
		void	unspecifyWindowPos() { mDefaultWindowFormat.unspecifyPos(); }

		//! Returns whether the default window is fullscreen
		bool	isFullScreen() { return mDefaultWindowFormat.isFullScreen(); }
		//! Sets whether the default window is fullscreen at startup with FullScreenOptions \a options. Kiosk Mode is enabled by default.
		void	setFullScreen( bool fullScreen = true, const FullScreenOptions &options = FullScreenOptions() ) { mDefaultWindowFormat.setFullScreen( fullScreen, options ); }

		//! Returns whether the default window is resizable
		bool	isResizable() const { return mDefaultWindowFormat.isResizable(); }
		//! Sets the default window to be resizable or not
		void	setResizable( bool resizable = true ) { mDefaultWindowFormat.setResizable( resizable ); }
		//! Returns whether the default window will be created without a border (chrome/frame)
		bool	isBorderless() const { return mDefaultWindowFormat.isBorderless(); }
		//! Sets the default window to be created without a border (chrome/frame)
		void	setBorderless( bool borderless = true ) { mDefaultWindowFormat.setBorderless( borderless ); }
		//! Returns whether the default  window always remains above all other windows
		bool	isAlwaysOnTop() const { return mDefaultWindowFormat.isAlwaysOnTop(); }
		//! Sets whether the default window always remains above all other windows
		void	setAlwaysOnTop( bool alwaysOnTop = true ) { mDefaultWindowFormat.setAlwaysOnTop( alwaysOnTop ); }

		//! Returns the display for the default window
		DisplayRef	getDisplay() const { return mDefaultWindowFormat.getDisplay(); }
		//! Sets the display for the default window
		void		setDisplay( DisplayRef display ) { mDefaultWindowFormat.setDisplay( display ); }

		//! Sets the default Renderer, overridding what was passed in during app instanciation.
		void		setDefaultRenderer( const RendererRef &renderer )	{ mDefaultRenderer = renderer; }
		//! Returns the default Renderer.
		RendererRef	getDefaultRenderer() const							{ return mDefaultRenderer; }

		void		prepareWindow( const Window::Format &format );
		std::vector<Window::Format>&		getWindowFormats()			{ return mWindowFormats; }
		const std::vector<Window::Format>&	getWindowFormats() const	{ return mWindowFormats; }

		//! Sets whether Windows created on a high-density (Retina) display will have their resolution doubled. Default is \c true on iOS and \c false on other platforms
		void		enableHighDensityDisplay( bool enable = true ) { mEnableHighDensityDisplay = enable; }
		//! Returns whether Windows created on a high-density (Retina) display will have their resolution doubled. Default is \c true on iOS and \c false on other platforms
		bool		isHighDensityDisplayEnabled() const { return mEnableHighDensityDisplay; }

		//! Returns the Window::Format which will be used if no calls are made to Settings::prepareWindow()
		Window::Format		getDefaultWindowFormat() const { return mDefaultWindowFormat; }
		//! Sets the Window::Format which will be used if no calls are made to Settings::prepareWindow()
		void				setDefaultWindowFormat( const Window::Format &format ) { mDefaultWindowFormat = format; }

		//! Registers the app to receive multiTouch events from the operating system. Disabled by default on desktop platforms, enabled on mobile.
		void		enableMultiTouch( bool enable = true ) { mEnableMultiTouch = enable; }
		//! Returns whether the app is registered to receive multiTouch events from the operating system. Disabled by default on desktop platforms, enabled on mobile.
		bool		isMultiTouchEnabled() const { return mEnableMultiTouch; }

		//! a value of \c true allows screensavers or the system's power management to hide the app. Default value is \c false on desktop, and \c true on mobile
		void	enablePowerManagement( bool enable = true );
		//! is power management enabled, allowing screensavers and the system's power management to hide the application
		bool	isPowerManagementEnabled() const { return mPowerManagement; }

		//! the title of the app reflected in ways particular to the app type and platform (such as its Window or menu)
		const std::string&	getTitle() const { return mTitle; }
		//! the title of the app reflected in ways particular to the app type and platform (such as its Window or menu)
		void				setTitle( const std::string &title ) { mTitle = title; }
		//! Returns the command line args passed to the application from its entry point (ex. a main's argc / argv).
		const std::vector<std::string>&		getCommandLineArgs() const	{ return mCommandLineArgs; }

		//! Sets maximum frameRate the update/draw loop will execute at, specified in frames per second. FrameRate limiting is on by default, at 60 FPS.
		void	setFrameRate( float frameRate );
		//! Disables the frameRate limiting, which is on by default. Restore using setFrameRate(). See also enableVerticalSync().
		void	disableFrameRate();
		//! Returns whether frameRate limiting is enabled. On by default, at 60 FPS.
		bool	isFrameRateEnabled() const { return mFrameRateEnabled; }
		//! Maximum frameRate of the application specified in frames per second
		float	getFrameRate() const { return mFrameRate; }

		//!	Set this to true if the app should terminate prior to launching.
		void	setShouldQuit( bool shouldQuit = true );
		//! Whether or not the app should terminate prior to launching
		bool	shouldQuit() const			{ return mShouldQuit; }

	  protected:
		void init( const RendererRef &defaultRenderer, const char *title, int argc, char * const argv[] );

		std::vector<Window::Format>		mWindowFormats;			// windows formats which have been requested using prepareWindow.
		Window::Format					mDefaultWindowFormat;	// used if no format was requested by user.
		RendererRef						mDefaultRenderer;
		std::string						mTitle;
		std::vector<std::string>		mCommandLineArgs;

		bool			mFrameRateEnabled;
		float			mFrameRate;
		bool			mPowerManagement; // allow screensavers or power management to hide app. default: false
		bool			mEnableHighDensityDisplay;
		bool			mEnableMultiTouch;
		bool			mShouldQuit; // defaults to false, facilitates early termination

		friend AppBase;
	};


 public:
	virtual ~AppBase();

	//! Override to perform any application setup after the Renderer has been initialized.
	virtual void	setup() {}
	//! Override to perform any once-per-loop computation.
	virtual void	update() {}
	//! Override to perform any rendering once-per-loop or in response to OS-prompted requests for refreshes.
	virtual void	draw() {}
	
	//! Override to receive mouse-down events.
	virtual void	mouseDown( MouseEvent event ) {}
	//! Override to receive mouse-up events.
	virtual void	mouseUp( MouseEvent event ) {}	
	//! Override to receive mouse-wheel events.
	virtual void	mouseWheel( MouseEvent event ) {}
	//! Override to receive mouse-move events.
	virtual void	mouseMove( MouseEvent event ) {}
	//! Override to receive mouse-drag events.
	virtual void	mouseDrag( MouseEvent event ) {}	

	//! Override to respond to the beginning of a multitouch sequence
	virtual void	touchesBegan( TouchEvent event ) {}
	//! Override to respond to movement (drags) during a multitouch sequence
	virtual void	touchesMoved( TouchEvent event ) {}
	//! Override to respond to the end of a multitouch sequence
	virtual void	touchesEnded( TouchEvent event ) {}
	
	//! Override to receive key-down events.
	virtual void	keyDown( KeyEvent event ) {}
	//! Override to receive key-up events.
	virtual void	keyUp( KeyEvent event ) {}
	//! Override to receive window resize events.
	virtual void	resize() {}
	//! Override to receive file-drop events.	
	virtual void	fileDrop( FileDropEvent event ) {}
	
	//! Quits the application gracefully
	virtual void	quit() = 0;

	//! Emitted at the start of each application update cycle
	signals::signal<void()>&	getSignalUpdate() { return mSignalUpdate; }

	//! Emitted prior to the application shutting down
	signals::signal<void()>&	getSignalShutdown() { return mSignalShutdown; }
	void 						emitShutdown();

	signals::signal<void()>&	getSignalWillResignActive() { return mSignalWillResignActive; }
    void 						emitWillResignActive();
	signals::signal<void()>&	getSignalDidBecomeActive() { return mSignalDidBecomeActive; }
	void 						emitDidBecomeActive();

	const std::vector<TouchEvent::Touch>& 	getActiveTouches() const { return getWindow()->getActiveTouches(); }

	//! Returns the Renderer of the active Window
	RendererRef			getRenderer() const { return getWindow()->getRenderer(); }
	//! Returns the Display of the active Window
	DisplayRef			getDisplay() const { return getWindow()->getDisplay(); }

	//! Returns the the currently active Window. Throws ExcInvalidWindow if called with no active window.
	virtual WindowRef	getWindow() const = 0;
	//! Returns the number of Windows the app has open
	virtual size_t		getNumWindows() const = 0;
	//! Gets a Window by index, in the range [0, getNumWindows()). Throw ExcInvalidWindow if \a index is out of bounds.
	virtual WindowRef	getWindowIndex( size_t index ) const = 0;
	//! Returns the current location of the mouse in screen coordinates measured in points. Can be called outside the normal event loop.
	virtual ivec2		getMousePos() const = 0;

	//! a value of true allows screensavers or the system's power management to hide the app. Default value is \c false on desktop, and \c true on mobile
	virtual void	enablePowerManagement( bool powerManagement = true ) { mPowerManagement = powerManagement; }
	//! is power management enabled, allowing screensavers and the system's power management to hide the application
	virtual bool	isPowerManagementEnabled() const { return mPowerManagement; }
	
	//! Returns the width of the App's current window measured in points
	int					getWindowWidth() const { return getWindow()->getWidth(); }
	//! Returns the height of the App's current window measured in points
	int					getWindowHeight() const { return getWindow()->getHeight(); }
	//! Sets the size of the App's current window measured in points. Ignored in full-screen mode.
	void				setWindowSize( int windowWidth, int windowHeight ) { setWindowSize( ivec2( windowWidth, windowHeight ) ); }
	//! Sets the size of the App's window measured in points. Ignored in full-screen mode.
	void				setWindowSize( const ivec2 &size ) { getWindow()->setSize( size ); }
	//! Returns the center of the App's window measured in points
	/** Equivalent to <tt>vec2( getWindowWidth() * 0.5, getWindowHeight() * 0.5 )</tt> **/	
	vec2				getWindowCenter() const { return vec2( (float)getWindowWidth(), (float)getWindowHeight() ) * 0.5f; }
	//! Returns the size of the App's current window measured in points
	ivec2				getWindowSize() const { return ivec2( getWindowWidth(), getWindowHeight() ); }
	//! Returns the aspect ratio of the App's current window
	float				getWindowAspectRatio() const { return getWindowWidth() / (float)getWindowHeight(); }
	//! Returns the bounding area of the App's current window measured in points.
	/** Equivalent to <tt>Area( 0, 0, getWindowWidth(), getWindowHeight() );</tt> **/	
	Area				getWindowBounds() const { return Area( 0, 0, getWindowWidth(), getWindowHeight() ); }
	//! Returns the contentScale of the App's window, which is the multiplier that maps points to pixels
	float				getWindowContentScale() const { return getWindow()->getContentScale(); }
	
	//! Returns tcoordinates of the top-left corner of the current window measured in points
	ivec2				getWindowPos() const { return getWindow()->getPos(); }
	//! Returns the X coordinate of the top-left corner of the current window measured in points
	int         		getWindowPosX() const { return getWindow()->getPos().x; }
	//! Returns the Y coordinate of the top-left corner of the current window contents measured in points
	int         		getWindowPosY() const { return getWindow()->getPos().y; }
	//! Sets the coordinates of the top-left corner of the current window measured in points
	void        		setWindowPos( int x, int y ) { setWindowPos( ivec2( x, y ) ); }
	//! Sets the coordinates of the top-left corner of the current window measured points
	virtual void        setWindowPos( const ivec2 &windowPos ) { getWindow()->setPos( windowPos ); }
    
	//! Returns the maximum frame-rate the App will attempt to maintain.
	virtual float		getFrameRate() const = 0;
	//! Sets the maximum frame-rate the App will attempt to maintain.
	virtual void		setFrameRate( float frameRate ) = 0;
	//! Returns the average frame-rate attained by the App as measured in frames-per-second
	float				getAverageFps() const { return mAverageFps; }
	//! Returns the sampling rate in seconds for measuring the average frame-per-second as returned by getAverageFps()
	double				getFpsSampleInterval() const { return mFpsSampleInterval; }
	//! Sets the sampling rate in seconds for measuring the average frame-per-second as returned by getAverageFps()
	void				setFpsSampleInterval( double sampleInterval ) { mFpsSampleInterval = sampleInterval; }	

	//! Returns whether the App is in full-screen mode or not.
	virtual bool		isFullScreen() const { return getWindow()->isFullScreen(); }
	//! Sets whether the active App is in full-screen mode based on \a fullScreen
	virtual void		setFullScreen( bool fullScreen, const FullScreenOptions &options = FullScreenOptions() ) { getWindow()->setFullScreen( fullScreen, options ); }

	//! Returns the number of seconds which have elapsed since application launch
	double				getElapsedSeconds() const { return mTimer.getSeconds(); }
	//! Returns the number of animation frames which have elapsed since application launch
	uint32_t			getElapsedFrames() const { return mFrameCount; }

	//! Returns whether the app is registered to receive multiTouch events from the operating system, configurable via Settings at startup. Disabled by default on desktop platforms, enabled on mobile.
	bool				isMultiTouchEnabled() const				{ return mMultiTouchEnabled; }
	//! Returns whether Windows created on a high-density (Retina) display will have their resolution doubled, configurable via Settings at startup. Default is \c true on iOS and \c false on other platforms.
	bool				isHighDensityDisplayEnabled() const		{ return mHighDensityDisplayEnabled; }

	// utilities
#if defined( CINDER_MSW )
	//! Returns a DataSourceRef to an application resource. \a mswID and \a mswType identify the resource as defined the application's .rc file(s). \sa \ref CinderResources
	DataSourceRef		loadResource( int mswID, const std::string &mswType ) const		{ return Platform::get()->loadResource( mswID, mswType ); }
#else
	DataSourceRef		loadResource( const fs::path &resourcePath ) const				{ return Platform::get()->loadResource( resourcePath ); }
#endif

	//! Returns a DataSourceRef to an application asset. Throws a AssetLoadExc on failure.
	DataSourceRef			loadAsset( const fs::path &relativePath )					{ return Platform::get()->loadAsset( relativePath ); }
	//! Returns a fs::path to an application asset. Returns an empty path on failure.
	fs::path				getAssetPath( const fs::path &relativePath ) const			{ return Platform::get()->getAssetPath( relativePath ); }
	//! Adds an absolute path 'dirPath' to the list of directories which are searched for assets.
	void					addAssetDirectory( const fs::path &dirPath )				{ return Platform::get()->addAssetDirectory( dirPath ); }
	
	//! Returns the path to the application on disk
	virtual fs::path			getAppPath() const = 0;

	//! \brief Presents the user with an open-file dialog and returns the selected file path.
	//!
	//! The dialog optionally begins at the path \a initialPath and can be limited to allow selection of files ending in the extensions enumerated in \a extensions.
	//!	\return the selected file path, or an empty fs::path if the user cancelled or this operation isn't supported on the current platform.
	fs::path getOpenFilePath( const fs::path &initialPath = fs::path(), const std::vector<std::string> &extensions = std::vector<std::string>() );
	//! Presents the user with an open-folder dialog. \return the selected file path, or an empty fs::path if the user cancelled or this operation isn't supported on the current platform.
	fs::path getFolderPath( const fs::path &initialPath = fs::path() );
	//! \brief Presents the user with a save-file dialog and returns the selected file path.
	//!
	//! The dialog optionally begins at the path \a initialPath and can be limited to allow selection of files ending in the extensions enumerated in \a extensions.
	//!	\return the selected file path, or an empty fs::path if the user cancelled or this operation isn't supported on the current platform.
	fs::path getSaveFilePath( const fs::path &initialPath = fs::path(), const std::vector<std::string> &extensions = std::vector<std::string>() );

	//! Returns a reference to an output console, which is by default an alias to std::cout. Other platforms may override to use other necessary console mechanisms.
	std::ostream&	console();

	//! Returns a vector of the command line arguments passed to the app when intantiated.
	const std::vector<std::string>&		getCommandLineArgs() const	{ return mCommandLineArgs; }

	//! Returns a reference to the App's Timeline
	Timeline&		timeline() { return *mTimeline; }

	//! Return \c true if the calling thread is the Application's primary thread
	static bool		isPrimaryThread();

	//! Returns a reference to the App's boost::asio::io_service()
	asio::io_service&	io_service() { return *mIo; }

	//! Executes a std::function on the App's primary thread ahead of the next update()
	void	dispatchAsync( const std::function<void()> &fn );
	
	template<typename T>
	typename std::result_of<T()>::type dispatchSync( T fn );

	//! Returns the default Renderer which will be used when creating a new Window. Set by the app instantiation macro automatically.
	RendererRef	getDefaultRenderer() const { return mDefaultRenderer; }
	/** \return a copy of the current window's contents as a Surface8u **/
	Surface	copyWindowSurface();
	/** \return a copy of the Area \a area (measured in pixels) from the current window's contents as a Surface8u **/
	Surface	copyWindowSurface( const Area &area );
	//! Restores the current rendering context to be the App's window or the screen in full-screen mode. Generally this is only necessary if the app has displayed a dialog box or some other external window.
	virtual void	restoreWindowContext()	{}

	//! Finds any Renderer of the same type as \a searchRenderer among existing windows. This is generally not necessary and used to enable context sharing between Windows. Returns NULL on failure.
	RendererRef		findSharedRenderer( RendererRef searchRenderer ) const;
	
	// DO NOT CALL - should be private but aren't for esoteric reasons
	//! \cond
	// Internal handlers - these are called into by AppImpl's. If you are calling one of these, you have likely strayed far off the path.
	virtual void	privateSetup__();
	virtual void	privateUpdate__();
	//! \endcond

	virtual bool		receivesEvents() const { return true; }

	//! Returns a pointer to the active App
	static AppBase*			get() { return sInstance; }

  protected:
	AppBase();

	//! \cond
	// These are called by the main application instantation functions and are only used in the launch process
	static void		prepareLaunch();
	static void		initialize( Settings *settings, const RendererRef &defaultRenderer, const char *title, int argc, char * const argv[] );
	static void		executeLaunch( const char *title, int argc, char * const argv[] );
	static void		cleanupLaunch();
	
	virtual void	launch( const char *title, int argc, char * const argv[] ) = 0;
	//! \endcond

  private:

	Timer					mTimer;
	uint32_t				mFrameCount;
	float					mAverageFps;
	uint32_t				mFpsLastSampleFrame;
	double					mFpsLastSampleTime;
	double					mFpsSampleInterval;
	bool					mMultiTouchEnabled, mHighDensityDisplayEnabled;
	RendererRef				mDefaultRenderer;

	std::vector<std::string>	mCommandLineArgs;
	std::shared_ptr<Timeline>	mTimeline;
	signals::signal<void()>		mSignalUpdate, mSignalShutdown, mSignalWillResignActive, mSignalDidBecomeActive;

	std::shared_ptr<asio::io_service>	mIo;
	std::shared_ptr<void>				mIoWork; // asio::io_service::work, but can't fwd declare member class

  protected:
	static AppBase*			sInstance;
	static Settings*		sSettingsFromMain;

	bool					mPowerManagement;
};

/** @name App Free Functions
	Convenience methods which mirror App member-functions and apply to the active application
**/
//@{
inline WindowRef	getWindow() { return AppBase::get()->getWindow(); }
//! Returns the number of Windows the app has open
inline size_t		getNumWindows() { return AppBase::get()->getNumWindows(); }
//! Gets a Window by index, in the range [0, getNumWindows()).
inline WindowRef	getWindowIndex( size_t index ) { return AppBase::get()->getWindowIndex( index ); }

//! Returns the width of the active App's window measured in points, or of the screen when in full-screen mode
inline int	getWindowWidth() { return AppBase::get()->getWindowWidth(); }
//! Sets the position of the active App's window measured in points. Ignored in full-screen mode
inline void		setWindowPos( const ivec2 &windowPos ) { AppBase::get()->setWindowPos( windowPos);  }
//! Sets the position of the active App's window measured in points. Ignored in full-screen mode
inline void		setWindowPos( int x, int y ) { setWindowPos( ivec2( x, y ) );  }
//! Returns the height of the active App's window measured in points, or the screen when in full-screen mode.
inline int	getWindowHeight() { return AppBase::get()->getWindowHeight(); }
//! Sets the size of the active App's window in points. Ignored in full-screen mode.
inline void		setWindowSize( int windowWidth, int windowHeight ) { AppBase::get()->setWindowSize( windowWidth, windowHeight ); }
//! Returns the center of the active App's window in pixels or of the screen in full-screen mode.
/** Equivalent to <tt>vec2( getWindowWidth() * 0.5, getWindowHeight() * 0.5 ) </tt> **/
inline vec2	getWindowCenter() { return AppBase::get()->getWindowCenter(); }
//! Returns the size of the active App's window or the screen in full-screen mode measured in points
inline ivec2	getWindowSize() { return AppBase::get()->getWindowSize(); }
//! Returns the position of the active App's window measured in points
inline ivec2	getWindowPos() { return AppBase::get()->getWindowPos(); }
//! Returns the aspect ratio of the active App's window or the screen in full-screen mode
inline float	getWindowAspectRatio() { return AppBase::get()->getWindowAspectRatio(); }
//! Returns the bounding area of the active App's window or the screen in full-screen mode measured in points
/** Equivalent to \code Area( 0, 0, getWindowWidth(), getWindowHeight() ); \endcode **/
inline Area		getWindowBounds() { return AppBase::get()->getWindowBounds(); }
//! Returns the contentScale of the active App's window, which is the multiplier that maps points to pixels
inline float	getWindowContentScale() { return AppBase::get()->getWindowContentScale(); }
//! Returns the maximum frame-rate the active App will attempt to maintain.
inline float	getFrameRate() { return AppBase::get()->getFrameRate(); }
//! Sets the maximum frame-rate the active App will attempt to maintain.
inline void		setFrameRate( float frameRate ) { AppBase::get()->setFrameRate( frameRate ); }
//! Returns whether the active App is in full-screen mode or not.
inline bool		isFullScreen() { return AppBase::get()->isFullScreen(); }
//! Sets whether the active App is in full-screen mode based on \a fullScreen
inline void		setFullScreen( bool fullScreen = true ) { AppBase::get()->setFullScreen( fullScreen ); }

//! Returns a scalar mapped from points to pixels for the current Window
inline float	toPixels( float s ) { return getWindow()->toPixels( s ); }
//! Returns a vec2 mapped from points to pixels for the current Window
inline vec2	toPixels( vec2 s ) { return getWindow()->toPixels( s ); }
//! Returns a ivec2 mapped from points to pixels for the current Window
inline	ivec2	toPixels( ivec2 s ) { return app::getWindow()->toPixels( s ); }
//! Returns an Area mapped from points to pixels for the current Window
inline	Area	toPixels( const Area &a ) { return getWindow()->toPixels( a ); }
//! Returns a Rectf mapped from points to pixels for the current Window
inline	Rectf	toPixels( const Rectf &a ) { return getWindow()->toPixels( a ); }
//! Returns a scalar mapped from pixels to points for the current Window
inline	float	toPoints( float s ) { return getWindow()->toPoints( s ); }
//! Returns a vec2 mapped from pixels to points for the current Window
inline	vec2	toPoints( vec2 s ) { return getWindow()->toPoints( s ); }
//! Returns a ivec2 mapped from pixels to points for the current Window
inline	ivec2	toPoints( ivec2 s ) { return getWindow()->toPoints( s ); }
//! Returns an Area mapped from pixels to points for the current Window
inline	Area	toPoints( const Area &a ) { return getWindow()->toPoints( a ); }
//! Returns a Rectf mapped from pixels to points for the current Window
inline	Rectf	toPoints( const Rectf &a ) { return getWindow()->toPoints( a ); }

//! Returns the number seconds which have elapsed since the active App launched.
inline double	getElapsedSeconds() { return AppBase::get()->getElapsedSeconds(); }
//! Returns the number of animation frames which have elapsed since the active App launched.
inline uint32_t	getElapsedFrames() { return AppBase::get()->getElapsedFrames(); }

#if defined( CINDER_MSW )
//! (MSW only) Returns a DataSource to an application resource. \a mswID and \a mswType identify the resource as defined the application's .rc file(s). \sa \ref CinderResources
inline DataSourceRef	loadResource( int mswID, const std::string &mswType ) { return Platform::get()->loadResource( mswID, mswType ); }
#else
//! Returns a DataSource to an application resource. \a resourcePath is defined on a per-platform basis. \sa \ref CinderResources
inline DataSourceRef	loadResource( const fs::path &resourcePath )		{ return Platform::get()->loadResource( resourcePath ); }
#endif // defined( CINDER_MSW )

//! Returns a DataSourceRef to the active App's's asset. Throws a AssetLoadExc on failure.
inline DataSourceRef		loadAsset( const fs::path &relativePath )		{ return Platform::get()->loadAsset( relativePath ); }
//! Returns a fs::path to the active App's asset. Returns an empty path on failure.
inline fs::path				getAssetPath( const fs::path &relativePath )	{ return Platform::get()->getAssetPath( relativePath ); }
//! Adds an absolute path \a dirPath to the active App's list of directories which are searched for assets.
inline void					addAssetDirectory( const fs::path &dirPath )	{ return Platform::get()->addAssetDirectory( dirPath ); }

//! Returns the path to the active App on disk
inline fs::path		getAppPath() { return AppBase::get()->getAppPath(); }
//! Presents the user with a file-open dialog and returns the selected file path.
/** The dialog optionally begins at the path \a initialPath and can be limited to allow selection of files ending in the extensions enumerated in \a extensions.
	If the active app is in full-screen mode it will temporarily switch to windowed-mode to present the dialog.
	\return the selected file path or an empty string if the user cancelled. **/
inline fs::path		getOpenFilePath( const fs::path &initialPath = fs::path(), std::vector<std::string> extensions = std::vector<std::string>() ) { return AppBase::get()->getOpenFilePath( initialPath, extensions ); }

//! Presents the user with a file-save dialog and returns the selected file path.
/** The dialog optionally begins at the path \a initialPath and can be limited to allow selection of files ending in the extensions enumerated in \a extensions.
	If the active app is in full-screen mode it will temporarily switch to windowed-mode to present the dialog.
	\return the selected file path or an empty string if the user cancelled. **/
inline fs::path		getSaveFilePath( const fs::path &initialPath = fs::path(), std::vector<std::string> extensions = std::vector<std::string>() ) { return AppBase::get()->getSaveFilePath( initialPath, extensions ); }

//! Returns a reference to an output console, which is an alias to std::cout on the mac, and a wrapper around OutputDebugString on MSW
/** On Mac OS X all output is echoed either to the Debugger Console in XCode or the system console
	On Windows output is echoed using OutputDebugString, echoed to the Output window of the debugger or to a stream viewable with Dbgview
	\code console() << "This line will be echoed" << std::endl; \endcode **/
inline std::ostream&	console()	{ return Platform::get()->console(); }

//! Returns a reference to the active App's Timeline
inline Timeline&	timeline() { return AppBase::get()->timeline(); }

//! Returns a copy of the current window's contents as a Surface8u
inline Surface	copyWindowSurface() { return AppBase::get()->copyWindowSurface(); }
//! Returns a copy of the Area \a area from the current window's contents as a Surface8u
inline Surface	copyWindowSurface( const Area &area ) { return AppBase::get()->copyWindowSurface( area ); }
//! Restores the current rendering context to be the App's window or the screen in full-screen mode. Generally this is only necessary if the app has display a dialog box or some other external window.
inline void		restoreWindowContext() { return AppBase::get()->restoreWindowContext(); }

#if defined( CINDER_COCOA )
//! Returns a CGContextRef for drawing to the Window using CoreGraphics under Cocoa & Cocoa Touch. Assumes your App uses a Renderer2d
inline ::CGContextRef	createWindowCgContext() { return (std::dynamic_pointer_cast<Renderer2d>(AppBase::get()->getRenderer()))->getCgContext(); }
#endif

//@}

template<typename T>
typename std::result_of<T()>::type AppBase::dispatchSync( T fn )
{
	if( isPrimaryThread() )
		return fn();
	else {
		typedef typename std::result_of<T()>::type result_type;
		std::packaged_task<result_type()> task( std::move( fn ) );

		auto fute = task.get_future();
		dispatchAsync( [&task]() { task(); } );
		return fute.get();
	}
}

} } // namespace cinder::app