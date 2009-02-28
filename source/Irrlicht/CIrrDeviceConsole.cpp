// Copyright (C) 2009 Gaz Davidson
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CIrrDeviceConsole.h"

#ifdef _IRR_USE_CONSOLE_DEVICE_

#include "os.h"

// to close the device on terminate signal
irr::CIrrDeviceConsole *DeviceToClose;

#ifdef _IRR_WINDOWS_NT_CONSOLE_
// Callback for Windows
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
    switch(CEvent)
    {
    case CTRL_C_EVENT:
		irr::os::Printer::log("Closing console device", "CTRL+C");
		break;
	case CTRL_BREAK_EVENT:
		irr::os::Printer::log("Closing console device", "CTRL+Break");
		break;
    case CTRL_CLOSE_EVENT:
		irr::os::Printer::log("Closing console device", "User closed console");
		break;
    case CTRL_LOGOFF_EVENT:
		irr::os::Printer::log("Closing console device", "User is logging off");
		break;
    case CTRL_SHUTDOWN_EVENT:
		irr::os::Printer::log("Closing console device", "Computer shutting down");
		break;
    }
	DeviceToClose->closeDevice();
    return TRUE;
}
#else
// sigterm handler
#include <signal.h>

void sighandler(int sig)
{
	irr::core::stringc code = "Signal ";
	code += sig;
	code += " received";
	irr::os::Printer::log("Closing console device", code.c_str());

	DeviceToClose->closeDevice();
}
#endif

namespace irr
{

const c8 ASCIIArtChars[] = " .,'~:;!+>=icopjtJY56SB8XDQKHNWM"; //MWNHKQDX8BS65YJtjpoci=+>!;:~',. ";
const u16 ASCIIArtCharsCount = 32;

//const c8 ASCIIArtChars[] = " \xb0\xb1\xf9\xb2\xdb";
//const u16 ASCIIArtCharsCount = 5;

//! constructor
CIrrDeviceConsole::CIrrDeviceConsole(const SIrrlichtCreationParameters& params)
  : CIrrDeviceStub(params), IsDeviceRunning(true), IsWindowFocused(true)
{
	DeviceToClose = this;

#ifdef _IRR_WINDOWS_NT_CONSOLE_
	MouseButtonStates = 0;

	WindowsSTDIn  = GetStdHandle(STD_INPUT_HANDLE);
	WindowsSTDOut = GetStdHandle(STD_OUTPUT_HANDLE);
	PCOORD Dimensions = 0;

	if (CreationParams.Fullscreen)
	{
		if (SetConsoleDisplayMode(WindowsSTDOut, CONSOLE_FULLSCREEN_MODE, Dimensions))
		{
			CreationParams.WindowSize.Width = Dimensions->X;
			CreationParams.WindowSize.Width = Dimensions->Y;
		}
	}
	else
	{
		COORD ConsoleSize;
		ConsoleSize.X = CreationParams.WindowSize.Width;
		ConsoleSize.X = CreationParams.WindowSize.Height;
		SetConsoleScreenBufferSize(WindowsSTDOut, ConsoleSize);
	}
	
	// catch windows close/break signals
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE);

#else
	// catch other signals
    signal(SIGABRT, &sighandler);
	signal(SIGTERM, &sighandler);
	signal(SIGINT,  &sighandler);
#endif

#ifdef _IRR_VT100_CONSOLE_
	// reset terminal
	printf("%cc", 27);
	// disable line wrapping
	printf("%c[7l", 27);
#endif

	switch (params.DriverType)
	{
	case video::EDT_SOFTWARE:
		#ifdef _IRR_COMPILE_WITH_SOFTWARE_
		VideoDriver = video::createSoftwareDriver(CreationParams.WindowSize, CreationParams.Fullscreen, FileSystem, this);
		#else
		os::Printer::log("Software driver was not compiled in.", ELL_ERROR);
		#endif
		break;

	case video::EDT_BURNINGSVIDEO:
		#ifdef _IRR_COMPILE_WITH_BURNINGSVIDEO_
		VideoDriver = video::createSoftwareDriver2(CreationParams.WindowSize, CreationParams.Fullscreen, FileSystem, this);
		#else
		os::Printer::log("Burning's Video driver was not compiled in.", ELL_ERROR);
		#endif
		break;

	case video::EDT_DIRECT3D8:
	case video::EDT_DIRECT3D9:
	case video::EDT_OPENGL:
		os::Printer::log("The console device cannot use hardware drivers", ELL_ERROR);
		break;
	default:
		break;
	}

#ifdef _IRR_WINDOWS_NT_CONSOLE_
	CursorControl = new CCursorControl(CreationParams.WindowSize);
#endif 

	if (VideoDriver)
		createGUIAndScene();
}

//! destructor
CIrrDeviceConsole::~CIrrDeviceConsole()
{
	// GUI and scene are dropped in the stub
	if (CursorControl)
	{
		CursorControl->drop();
		CursorControl = 0;
	}
#ifdef _IRR_VT100_CONSOLE_
	// reset terminal 
	printf("%cc", 27);
#endif
}

//! runs the device. Returns false if device wants to be deleted
bool CIrrDeviceConsole::run()
{
	// increment timer
	os::Timer::tick();

	// process Windows console input 
#ifdef _IRR_WINDOWS_NT_CONSOLE_

	INPUT_RECORD in;
	DWORD        oldMode;
	DWORD        count, waste;

	// get old input mode
	GetConsoleMode(WindowsSTDIn, &oldMode);
	SetConsoleMode(WindowsSTDIn, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);

	GetNumberOfConsoleInputEvents(WindowsSTDIn, &count);

	// read keyboard and mouse input
	while (count)
	{
		ReadConsoleInput(WindowsSTDIn, &in, 1, &waste );
		switch(in.EventType)
		{
		case KEY_EVENT:
		{
			SEvent e;
			e.EventType            = EET_KEY_INPUT_EVENT;
			e.KeyInput.PressedDown = (in.Event.KeyEvent.bKeyDown == TRUE);
			e.KeyInput.Control     = (in.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
			e.KeyInput.Shift       = (in.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) != 0;
			e.KeyInput.Key         = EKEY_CODE(in.Event.KeyEvent.wVirtualKeyCode);
			e.KeyInput.Char        = in.Event.KeyEvent.uChar.UnicodeChar;
			postEventFromUser(e);
			break;
		}
		case MOUSE_EVENT:
		{
			SEvent e;
			e.EventType        = EET_MOUSE_INPUT_EVENT;
			e.MouseInput.X     = in.Event.MouseEvent.dwMousePosition.X;
			e.MouseInput.Y     = in.Event.MouseEvent.dwMousePosition.Y;
			e.MouseInput.Wheel = 0.f;
			e.MouseInput.ButtonStates = 
				( (in.Event.MouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) ? EMBSM_LEFT   : 0 ) |
				( (in.Event.MouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED)     ? EMBSM_RIGHT  : 0 ) |
				( (in.Event.MouseEvent.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) ? EMBSM_MIDDLE : 0 ) |
				( (in.Event.MouseEvent.dwButtonState & FROM_LEFT_3RD_BUTTON_PRESSED) ? EMBSM_EXTRA1 : 0 ) |
				( (in.Event.MouseEvent.dwButtonState & FROM_LEFT_4TH_BUTTON_PRESSED) ? EMBSM_EXTRA2 : 0 );
			
			if (in.Event.MouseEvent.dwEventFlags & MOUSE_MOVED)
			{
				CursorControl->setPosition(core::position2di(e.MouseInput.X, e.MouseInput.Y));

				// create mouse moved event
				e.MouseInput.Event = EMIE_MOUSE_MOVED;
				postEventFromUser(e);
			}

			if (in.Event.MouseEvent.dwEventFlags & MOUSE_WHEELED)
			{
				e.MouseInput.Event = EMIE_MOUSE_WHEEL;
				e.MouseInput.Wheel = (in.Event.MouseEvent.dwButtonState & 0xFF000000) ? -1.0f : 1.0f;
				postEventFromUser(e);
			}

			if ( (MouseButtonStates & EMBSM_LEFT) != (e.MouseInput.ButtonStates & EMBSM_LEFT) )
			{
				e.MouseInput.Event = (e.MouseInput.ButtonStates & EMBSM_LEFT) ? EMIE_LMOUSE_PRESSED_DOWN : EMIE_LMOUSE_LEFT_UP;
				postEventFromUser(e);
			}

			if ( (MouseButtonStates & EMBSM_RIGHT) != (e.MouseInput.ButtonStates & EMBSM_RIGHT) )
			{
				e.MouseInput.Event = (e.MouseInput.ButtonStates & EMBSM_RIGHT) ? EMIE_RMOUSE_PRESSED_DOWN : EMIE_RMOUSE_LEFT_UP;
				postEventFromUser(e);
			}

			if ( (MouseButtonStates & EMBSM_MIDDLE) != (e.MouseInput.ButtonStates & EMBSM_MIDDLE) )
			{
				e.MouseInput.Event = (e.MouseInput.ButtonStates & EMBSM_MIDDLE) ? EMIE_MMOUSE_PRESSED_DOWN : EMIE_MMOUSE_LEFT_UP;
				postEventFromUser(e);
			}

			// save current button states
			MouseButtonStates = e.MouseInput.ButtonStates;

			break;
		}
		case WINDOW_BUFFER_SIZE_EVENT: 
			VideoDriver->OnResize(
				core::dimension2d<u32>(in.Event.WindowBufferSizeEvent.dwSize.X, 
				                       in.Event.WindowBufferSizeEvent.dwSize.Y));
			break;
		case FOCUS_EVENT:
			IsWindowFocused = (in.Event.FocusEvent.bSetFocus == TRUE);
			break;
		default:
			break;
		}
		GetNumberOfConsoleInputEvents(WindowsSTDIn, &count);
	}

	// set input mode
	SetConsoleMode(WindowsSTDIn, oldMode);
#else
	// todo: process terminal keyboard input
#endif
	
	return IsDeviceRunning;
}

//! Cause the device to temporarily pause execution and let other processes to run
// This should bring down processor usage without major performance loss for Irrlicht
void CIrrDeviceConsole::yield()
{
#ifdef _IRR_WINDOWS_API_
	Sleep(1);
#else
	struct timespec ts = {0,0};
	nanosleep(&ts, NULL);
#endif
}

//! Pause execution and let other processes to run for a specified amount of time.
void CIrrDeviceConsole::sleep(u32 timeMs, bool pauseTimer)
{
	const bool wasStopped = Timer ? Timer->isStopped() : true;

#ifdef _IRR_WINDOWS_API_
	Sleep(timeMs);
#else
	struct timespec ts;
	ts.tv_sec = (time_t) (timeMs / 1000);
	ts.tv_nsec = (long) (timeMs % 1000) * 1000000;

	if (pauseTimer && !wasStopped)
		Timer->stop();

	nanosleep(&ts, NULL);
#endif

	if (pauseTimer && !wasStopped)
		Timer->start();
}

//! sets the caption of the window
void CIrrDeviceConsole::setWindowCaption(const wchar_t* text)
{
#ifdef _IRR_WINDOWS_NT_CONSOLE_
	core::stringc txt(text);
	SetConsoleTitle(txt.c_str());
#endif
}

//! returns if window is active. if not, nothing need to be drawn
bool CIrrDeviceConsole::isWindowActive() const
{
	// there is no window, but we always assume it is active
	return true;
}

//! returns if window has focus
bool CIrrDeviceConsole::isWindowFocused() const
{
	return IsWindowFocused;
}

//! returns if window is minimized
bool CIrrDeviceConsole::isWindowMinimized() const
{
	return false;
}

//! presents a surface in the client area
bool CIrrDeviceConsole::present(video::IImage* surface, void* windowId, core::rect<s32>* src)
{
	if (surface)
	{
		OutputLine.reserve(surface->getDimension().Width + 1);

		for (u32 y=0; y < surface->getDimension().Height; ++y)
		{
			setTextCursorPos(0,y);

			for (u32 x=0; x< surface->getDimension().Width; ++x)
			{
				// get average pixel
				u32 avg = surface->getPixel(x,y).getAverage() * (ASCIIArtCharsCount-1);
				avg /= 255;
				OutputLine += ASCIIArtChars[avg];
			}
			printf("%s", OutputLine.c_str());
			OutputLine = "";
		}
	}

	return true;
}

//! notifies the device that it should close itself
void CIrrDeviceConsole::closeDevice()
{
	// return false next time we run()
	IsDeviceRunning = false;
}

//! Sets if the window should be resizeable in windowed mode.
void CIrrDeviceConsole::setResizeAble(bool resize)
{
	// do nothing
}

void CIrrDeviceConsole::setTextCursorPos(s16 x, s16 y)
{
#ifdef _IRR_WINDOWS_NT_CONSOLE_
	// move WinNT cursor
	COORD Position;
    Position.X = x;
    Position.Y = y;
    SetConsoleCursorPosition(WindowsSTDOut, Position);
#elif defined(_IRR_VT100_CONSOLE_)
	// send escape code
	printf("%c[%d;%dH", 27, y, x);
#else
	// not implemented
#endif
}

extern "C" IRRLICHT_API IrrlichtDevice* IRRCALLCONV createDeviceEx(
	const SIrrlichtCreationParameters& parameters)
{
	CIrrDeviceConsole* dev = new CIrrDeviceConsole(parameters);

	if (dev && !dev->getVideoDriver() && parameters.DriverType != video::EDT_NULL)
	{
		dev->closeDevice(); // close device
		dev->run(); // consume quit message
		dev->drop();
		dev = 0;
	}

	return dev;
}


} // end namespace irr

#endif // _IRR_USE_CONSOLE_DEVICE_