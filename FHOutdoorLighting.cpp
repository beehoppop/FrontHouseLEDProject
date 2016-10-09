/*
	Author: Brent Pease

	The MIT License (MIT)

	Copyright (c) 2015-FOREVER Brent Pease

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

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

/*
	ABOUT

	This is the main module for a front of house lighting project.
*/

#if !defined(WIN32)
	#include <OctoWS2811.h>
#endif

#include <ELModule.h>
#include <ELRealTime.h>
#include <ELDigitalIO.h>
#include <ELSunRiseAndSet.h>
#include <ELAssert.h>
#include <ELLuminositySensor.h>
#include <ELUtilities.h>
#include <ELInternet.h>
#include <ELInternetDevice_ESP8266.h>
#include <ELRemoteLogging.h>
#include <ELCalendarEvent.h>
#include <ELOutdoorLightingControl.h>

enum
{
	eTransformerRelayPin = 17,		// This output pin controls the relay for the main power transformer to the leds
	eToggleButtonPin = 9,			// This input pin is for a pushbutton that forces the leds on or off and can activate a test pattern and cycle through the holiday base patterns
	eMotionSensorPin = 22,			// This pin is triggered by the external motion sensor
	eESP8266ResetPin = 23,

	ePanelCount = 10,				// This is the number of led panels that go across the roof soffit
	eLEDsPerPanel = 38,				// This is the number of leds per panel
	eLEDPanelsCenterToRight = 4,	// This is the number of panels on the right side of the house roof (control system is at top center of the gable roof)
	eLEDPanelsCenterToLeft = 6,		// This is the number of panels on the left side of the house roof (control system is at top center of the gable roof)
	eLEDsPerStrip = eLEDPanelsCenterToLeft * eLEDsPerPanel,	// The total leds per strip is the max of the two strips in use
	eLEDCount = ePanelCount * eLEDsPerPanel,	// The total number of leds across the roof

	eLEDStripCenterToRight = 3,		// This is the Octo strip number (starting from 0) that goes from the center towards the right
	eLEDStripCenterToLeft = 0,		// This is the Octo strip number (starting from 0) that goes from the center towards the left

	ePushCount_CyclePatterns = 3,	// The number of pushes to trigger the cycling of the holiday base patterns
	ePushCount_TestPattern = 4,		// The number of pushes to trigger the test pattern

	eCyclePatternTime = 4000,	// The duration in ms for each holiday base pattern when cycling

	eMaxPatternCount = 10,
};

enum
{
	eViewMode_Normal,
	eViewMode_CyclePatterns,
	eViewMode_TestPattern,
};

char const*	gViewModeStr[] = {"Normal", "CyclePatterns", "Test"};

const float	cTestPatternPixelsPerSec = 100.0f;	// The speed for the test pattern

class CBasePattern;

DMAMEM int		gLEDDisplayMemory[eLEDsPerStrip * 6];
int				gLEDMap[eLEDCount];
int				gPatternCount;
CBasePattern*	gPatternList[eMaxPatternCount];

// A CBasePattern will fill in an array of these, float is used for easy manipulation by lighting intensity and future effect modifiers
struct SFloatPixel
{
	float	r, g, b;
};

struct SSettings
{
	SFloatPixel	defaultColor;
	float		defaultIntensity;
	float		activeIntensity;
	float		minLux;
	float		maxLux;
};

// Patterns inherit from CBasePattern
class CBasePattern
{
public:
	
	CBasePattern(
		)
	{
		gPatternList[gPatternCount++] = this;
	}

	// This does the actual drawing. must be defined by the derived class
	virtual void
	Draw(
		int				inPixels,
		SFloatPixel*	inPixelMem) = 0;

	virtual char const*
	GetName(
		void) = 0;
};

// The various patterns are defined below

class CXMasPattern : public CBasePattern
{
public:

	CXMasPattern(
		)
		:
		CBasePattern()
	{
	}

	virtual void
	Draw(
		int				inPixels,
		SFloatPixel*	inPixelMem)
	{
		for(int itr = 0; itr < inPixels; ++itr)
		{
			if(((itr / eLEDsPerPanel) & 1) == 0)
			{
				inPixelMem[itr].r = 1.0;
				inPixelMem[itr].g = 0.0;
				inPixelMem[itr].b = 0.0;
			}
			else
			{
				inPixelMem[itr].r = 0.0;
				inPixelMem[itr].g = 1.0;
				inPixelMem[itr].b = 0.0;
			}
		}
	}

	virtual char const*
	GetName(
		void)
	{
		return "Christmas";
	}
};
static CXMasPattern	gXMasPattern;

class CValintinePattern : public CBasePattern
{
public:

	CValintinePattern(
		)
		:
		CBasePattern()
	{
	}

	virtual void
	Draw(
		int				inPixels,
		SFloatPixel*	inPixelMem)
	{
		for(int itr = 0; itr < inPixels; ++itr)
		{
			inPixelMem[itr].r = 1.0;
			inPixelMem[itr].g = 0.0;
			inPixelMem[itr].b = 0.0;
		}
	}

	virtual char const*
	GetName(
		void)
	{
		return "Valentine's Day";
	}
};
static CValintinePattern	gValintinePattern;

class CJuly4Pattern : public CBasePattern
{
public:

	CJuly4Pattern(
		)
		:
		CBasePattern()
	{
	}

	virtual void
	Draw(
		int				inPixels,
		SFloatPixel*	inPixelMem)
	{
		for(int itr = 0; itr < inPixels; ++itr)
		{
			int	primaryColor = (itr / eLEDsPerPanel) % 3;
			if(primaryColor == 0)
			{
				inPixelMem[itr].r = 1.0;
				inPixelMem[itr].g = 0.0;
				inPixelMem[itr].b = 0.0;
			}
			else if(primaryColor == 1)
			{
				inPixelMem[itr].r = 1.0;
				inPixelMem[itr].g = 1.0;
				inPixelMem[itr].b = 1.0;
			}
			else
			{
				inPixelMem[itr].r = 0.0;
				inPixelMem[itr].g = 0.0;
				inPixelMem[itr].b = 1.0;
			}
		}
	}

	virtual char const*
	GetName(
		void)
	{
		return "4th Of July";
	}
};
static CJuly4Pattern	gJuly4Pattern;

class CHalloweenPattern : public CBasePattern
{
public:

	CHalloweenPattern(
		)
		:
		CBasePattern()
	{
	}

	virtual void
	Draw(
		int				inPixels,
		SFloatPixel*	inPixelMem)
	{
		for(int itr = 0; itr < inPixels; ++itr)
		{
			if(((itr / eLEDsPerPanel) & 1) == 0)
			{
				inPixelMem[itr].r = 1.0f;
				inPixelMem[itr].g = 0.65f;
				inPixelMem[itr].b = 0.0f;
			}
			else
			{
				inPixelMem[itr].r = 0.5f;
				inPixelMem[itr].g = 0.0f;
				inPixelMem[itr].b = 0.5f;
			}
		}
	}

	virtual char const*
	GetName(
		void)
	{
		return "Halloween";
	}
};
static CHalloweenPattern	gHalloweenPattern;

class CStPattyPattern : public CBasePattern
{
public:

	CStPattyPattern(
		)
		:
		CBasePattern()
	{
	}

	virtual void
	Draw(
		int				inPixels,
		SFloatPixel*	inPixelMem)
	{
		for(int itr = 0; itr < inPixels; ++itr)
		{
			inPixelMem[itr].r = 0.0f;
			inPixelMem[itr].g = 1.0f;
			inPixelMem[itr].b = 0.0f;
		}
	}

	virtual char const*
	GetName(
		void)
	{
		return "St. Patty's Day";
	}
};
static CStPattyPattern	gStPattyPattern;

class CEasterPattern : public CBasePattern
{
public:

	CEasterPattern(
		)
		:
		CBasePattern()
	{
	}

	virtual void
	Draw(
		int				inPixels,
		SFloatPixel*	inPixelMem)
	{
		for(int itr = 0; itr < inPixels; ++itr)
		{
			int	primaryColor = (itr / eLEDsPerPanel) % 7;
			if(primaryColor == 0)
			{
				inPixelMem[itr].r = 1.0f;
				inPixelMem[itr].g = 1.0f;
				inPixelMem[itr].b = 0.0f;
			}
			else if(primaryColor == 1)
			{
				inPixelMem[itr].r = 0.5f;
				inPixelMem[itr].g = 0.0f;
				inPixelMem[itr].b = 0.5f;
			}
			else if(primaryColor == 2)
			{
				inPixelMem[itr].r = 1.0f;
				inPixelMem[itr].g = 0.0f;
				inPixelMem[itr].b = 0.0f;
			}
			else if(primaryColor == 3)
			{
				inPixelMem[itr].r = 0.0f;
				inPixelMem[itr].g = 1.0f;
				inPixelMem[itr].b = 0.0f;
			}
			else if(primaryColor == 4)
			{
				inPixelMem[itr].r = 0.0f;
				inPixelMem[itr].g = 0.0f;
				inPixelMem[itr].b = 1.0f;
			}
			else if(primaryColor == 5)
			{
				inPixelMem[itr].r = 1.0f;
				inPixelMem[itr].g = 0.41f;
				inPixelMem[itr].b = 0.71f;
			}
			else
			{
				inPixelMem[itr].r = 1.0f;
				inPixelMem[itr].g = 0.65f;
				inPixelMem[itr].b = 0.0f;
			}
		}
	}

	virtual char const*
	GetName(
		void)
	{
		return "Easter";
	}
};
static CEasterPattern	gEasterPattern;

// This defines our main module
class COutdoorLightingModule : public CModule, public IRealTimeHandler, public ISunRiseAndSetEventHandler, public IDigitalIOEventHandler, public ICmdHandler, public IInternetHandler, public IOutdoorLightingInterface
{
public:

	MModule_Declaration(COutdoorLightingModule)

private:
	
	COutdoorLightingModule(
		)
		:
		CModule(
			sizeof(SSettings),
			0,
			&settings,
			30000),
		leds(eLEDsPerStrip, gLEDDisplayMemory, NULL, WS2811_RGB)
	{
		// Create a map of indices going from the left side of the house (facing it) to the right side
		// This allows use to address the LEDs in linear order left to right given that the octo layout is by strip
		for(int i = 0; i < eLEDPanelsCenterToLeft * eLEDsPerPanel; ++i)
		{
			gLEDMap[i] = eLEDStripCenterToLeft * eLEDsPerStrip + eLEDPanelsCenterToLeft * eLEDsPerPanel - i - 1;
		}

		for(int i = 0; i < eLEDPanelsCenterToRight * eLEDsPerPanel; ++i)
		{
			gLEDMap[eLEDPanelsCenterToLeft * eLEDsPerPanel + i] = eLEDStripCenterToRight * eLEDsPerStrip + i;
		}

		viewMode = 0;
		memset(frameBuffer, 0, sizeof(frameBuffer));
		basePattern = NULL;
		cyclePatternTimeMS = 0;
		cyclePatternCount = 0;
		testPatternValue = 0;

		ledsOn = false;
		motionSensorTriggered = false;
		timeOfDay = 0;

		luminosityInterface = new CTSL2561Sensor(0x39, eGain_1X, eIntegrationTime_13_7ms);
		if(!luminosityInterface->IsPresent())
		{
			luminosityInterface = NULL;
		}

		// Include dependent modules

		CModule_Loggly*			loggly = CModule_Loggly::Include("front_house");
		IRealTimeDataProvider*	ds3234Provider = CreateDS3234Provider(10);
		IInternetDevice*		internetDevice = CModule_ESP8266::Include(5, &Serial1, eESP8266ResetPin);

		CModule_RealTime::Include();
		CModule_Internet::Include();
		CModule_Command::Include();
		CModule_OutdoorLightingControl::Include(this, eMotionSensorPin, eTransformerRelayPin, eToggleButtonPin, luminosityInterface);

		AddSysMsgHandler(loggly);
		gRealTime->Configure(ds3234Provider, 24 * 60 * 60);
		gInternetModule->Configure(internetDevice);
		ledsOn = false;
	}

	void
	Setup(
		void)
	{
		if(luminosityInterface != NULL)
		{
			luminosityInterface->SetMinMaxLux(settings.minLux, settings.maxLux);
		}

		// Configure the time provider on the standard SPI chip select pin

		// Instantiate the wireless networking device and configure it to serve pages
		gInternetModule->WebServer_Start(8080);
		MInternetRegisterPage("/", COutdoorLightingModule::CommandHomePageHandler);

		// Register the commands
		MCommandRegister("test_pattern", COutdoorLightingModule::TestPattern, "");
		MCommandRegister("color_set", COutdoorLightingModule::SetColor, "");
		MCommandRegister("color_get", COutdoorLightingModule::GetColor, "");
		MCommandRegister("intensity_set", COutdoorLightingModule::SetIntensity, "[default] [active] : set the intensity levels");
		MCommandRegister("intensity_get", COutdoorLightingModule::GetIntensity, "");
		MCommandRegister("luxminmax_set", COutdoorLightingModule::SetMinMaxLux, "");
		MCommandRegister("luxminmax_get", COutdoorLightingModule::GetMinMaxLux, "");

		leds.begin();

		viewMode = eViewMode_Normal;
	}

	void
	CommandHomePageHandler(
		IOutputDirector*	inOutput,
		int					inParamCount,
		char const**		inParamList)
	{
		// Send html via in Output to add to the command server home page served to clients

		inOutput->printf("<table border=\"1\">");
		inOutput->printf("<tr><th>Parameter</th><th>Value</th></tr>");

		// add current holiday
		inOutput->printf("<tr><td>Holiday</td><td>%s</td></tr>", basePattern != NULL ? basePattern->GetName() : "None");

		// add viewMode
		inOutput->printf("<tr><td>View Mode</td><td>%s</td></tr>", gViewModeStr[viewMode]);

		// add settings.defaultColor
		inOutput->printf("<tr><td>Default Color</td><td>r:%01.02f g:%01.02f b:%01.02f</td></tr>", settings.defaultColor.r, settings.defaultColor.g, settings.defaultColor.b);

		// add settings.defaultIntensity
		inOutput->printf("<tr><td>Default Intensity</td><td>%01.02f</td></tr>", settings.defaultIntensity);

		// add settings.activeIntensity
		inOutput->printf("<tr><td>Active Intensity</td><td>%01.02f</td></tr>", settings.activeIntensity);

		// add settings.minLux, settings.maxLux
		inOutput->printf("<tr><td>Lux Range</td><td>%f %f</td></tr>", settings.minLux, settings.maxLux);

		inOutput->printf("</table>");
	}

	virtual void
	LEDStateChange(
		bool	inLEDsOn)
	{
		ledsOn = inLEDsOn;

		if(ledsOn == false)
		{
			// turn all LEDs off
			for(uint32_t itr = 0; itr < eLEDCount; ++itr)
			{
				SetRoofPixel(itr, 0, 0, 0);
			}
			leds.show();
		}
		else
		{
			FindBasePattern();
		}
	}

	virtual void
	MotionSensorStateChange(
		bool	inMotionSensorTriggered)
	{
		motionSensorTriggered = inMotionSensorTriggered;
	}

	virtual void
	LuxSensorStateChange(
		bool	inTriggered)
	{
	}

	virtual void
	PushButtonStateChange(
		int	inToggleCount)
	{
		if(inToggleCount == ePushCount_CyclePatterns)
		{
			SystemMsg("Entering pattern cycling\n");
			viewMode = eViewMode_CyclePatterns;
			gOutdoorLighting->SetOverride(true, true);
		}
		else if(inToggleCount == ePushCount_TestPattern)
		{
			SystemMsg("Entering test pattern\n");
			viewMode = eViewMode_TestPattern;
			gOutdoorLighting->SetOverride(true, true);
		}
		else
		{
			SystemMsg("Entering normal mode\n");
			viewMode = eViewMode_Normal;
			gOutdoorLighting->SetOverride(false, false);
		}
	}

	virtual void
	TimeOfDayChange(
		int	inTimeOfDay)
	{
		timeOfDay = inTimeOfDay;
	}

	void
	Update(
		uint32_t inDeltaTimeUS)
	{
		if(ledsOn == false)
		{
			return;
		}

		switch(viewMode)
		{
			case eViewMode_Normal:
			{
				if(basePattern != NULL)
				{
					basePattern->Draw(eLEDCount, frameBuffer);
				}
				else
				{
					for(uint32_t itr = 0; itr < eLEDCount; ++itr)
					{
						frameBuffer[itr].r = settings.defaultColor.r;
						frameBuffer[itr].g = settings.defaultColor.g;
						frameBuffer[itr].b = settings.defaultColor.b;
					}
				}

				float	intensity;

				if(timeOfDay == eTimeOfDay_Day)
				{
					intensity = 1.0;
				}
				else
				{
					if(motionSensorTriggered)
					{
						intensity = settings.activeIntensity;
					}
					else if(luminosityInterface != NULL)
					{
						intensity = (1.0f - gOutdoorLighting->GetAvgBrightness()) * settings.defaultIntensity;
					}
					else
					{
						intensity = settings.defaultIntensity;
					}
				}

				// Perhaps eventually apply some effects here

				for(uint32_t itr = 0; itr < eLEDCount; ++itr)
				{
					SetRoofPixel(itr, uint8_t(frameBuffer[itr].r * intensity * 255.0f), uint8_t(frameBuffer[itr].g * intensity * 255.0f), uint8_t(frameBuffer[itr].b * intensity * 255.0f));
				}
				break;
			}

			case eViewMode_CyclePatterns:
				if(gCurLocalMS - cyclePatternTimeMS >= eCyclePatternTime || basePattern == NULL)
				{
					SystemMsg("Cycling patterns\n");
					basePattern = gPatternList[cyclePatternCount++ % gPatternCount];
					cyclePatternTimeMS = gCurLocalMS;
				}
				basePattern->Draw(eLEDCount, frameBuffer);
				for(uint32_t itr = 0; itr < eLEDCount; ++itr)
				{
					SetRoofPixel(itr, int(frameBuffer[itr].r * 255.0f), int(frameBuffer[itr].g * 255.0f), int(frameBuffer[itr].b * 255.0f));
				}
				break;

			case eViewMode_TestPattern:
				testPatternValue += cTestPatternPixelsPerSec * (float)inDeltaTimeUS / 1000000.0f;

				if(testPatternValue >= eLEDCount * 2.0f)
				{
					testPatternValue -= eLEDCount * 2.0f;
				}

				int	intValue = (int)testPatternValue;
				int	indexR = intValue <  eLEDCount ? intValue : eLEDCount * 2 - intValue - 1;
				int	indexG = (intValue - 1) <  eLEDCount ? (intValue - 1) : eLEDCount * 2 - (intValue - 1);
				int	indexB = (intValue - 2) <  eLEDCount ? (intValue - 2)  : eLEDCount * 2 - (intValue - 2) ;

				for(int itr = 0; itr < eLEDCount; ++itr)
				{
					uint8_t	r, g, b;
				
					r = uint8_t(indexR == itr ? 0xFF : 0);
					g = uint8_t(indexG == itr ? 0xFF : 0);
					b = uint8_t(indexB == itr ? 0xFF : 0);
					SetRoofPixel(itr, r, g, b);
				}
				break;
		}

		leds.show();
	}

	void
	SetRoofPixel(
		int		inIndex,	// 0 is left side facing house, max led index is right side facing house
		uint8_t	inRed,
		uint8_t	inGreen,
		uint8_t	inBlue)
	{
		leds.setPixel(gLEDMap[inIndex], inRed, inGreen, inBlue);
	}

	uint8_t
	TestPattern(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		if(inArgC != 2)
		{
			return eCmd_Failed;
		}

		if(strcmp(inArgv[1], "on") == 0)
		{
			gOutdoorLighting->SetOverride(true, true);
			viewMode = eViewMode_TestPattern;
			return true;
		}
		else if(strcmp(inArgv[1], "off") == 0)
		{
			gOutdoorLighting->SetOverride(false, false);
			viewMode = eViewMode_Normal;
			return true;
		}

		return eCmd_Succeeded;
	}
	
	uint8_t
	SetColor(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		if(inArgC != 4)
		{
			return eCmd_Failed;
		}

		settings.defaultColor.r = (float)atof(inArgv[1]);
		settings.defaultColor.g = (float)atof(inArgv[2]);
		settings.defaultColor.b = (float)atof(inArgv[3]);

		EEPROMSave();

		return eCmd_Succeeded;
	}
	
	uint8_t
	GetColor(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		inOutput->printf("%f %f %f\n", settings.defaultColor.r, settings.defaultColor.g, settings.defaultColor.b);

		return eCmd_Succeeded;
	}

	uint8_t
	SetIntensity(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		if(inArgC != 3)
		{
			return eCmd_Failed;
		}

		settings.defaultIntensity = (float)atof(inArgv[1]);
		settings.activeIntensity = (float)atof(inArgv[2]);

		EEPROMSave();

		return eCmd_Succeeded;
	}

	uint8_t
	GetIntensity(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		inOutput->printf("%f %f\n", settings.defaultIntensity, settings.activeIntensity);

		return eCmd_Succeeded;
	}

	uint8_t
	SetMinMaxLux(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		if(inArgC != 3)
		{
			return eCmd_Failed;
		}

		settings.minLux = (float)atof(inArgv[1]);
		settings.maxLux = (float)atof(inArgv[2]);

		if(luminosityInterface != NULL)
		{
			luminosityInterface->SetMinMaxLux(settings.minLux, settings.maxLux);
		}

		EEPROMSave();

		return eCmd_Succeeded;
	}

	uint8_t
	GetMinMaxLux(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		inOutput->printf("%f %f\n", settings.minLux, settings.maxLux);

		return eCmd_Succeeded;
	}

	void
	FindBasePattern(
		void)
	{
		// Find a pattern given the date
		basePattern = NULL;
		int	year, month, day, dow, hour, min, sec;
		gRealTime->GetComponentsFromEpochTime(gRealTime->GetEpochTime(false), year, month, day, dow, hour, min, sec);

		if(month == 12)
		{
			basePattern = &gXMasPattern;
		}
		else
		{
			EHoliday	curHoliday = GetHolidayForDate(year, month, day);

			if(curHoliday == eHoliday_None)
			{
				// If today is not a holiday check if tomorrow is one
				gRealTime->GetComponentsFromEpochTime(gRealTime->GetEpochTime(false) + 24 * 60 * 60, year, month, day, dow, hour, min, sec);
				curHoliday = GetHolidayForDate(year, month, day);
			}

			switch(curHoliday)
			{
				case eHoliday_ValintinesDay:
					basePattern = &gValintinePattern;
					break;

				case eHoliday_SaintPatricksDay:
					basePattern = &gStPattyPattern;
					break;

				case eHoliday_Easter:
					basePattern = &gEasterPattern;
					break;

				case eHoliday_IndependenceDay:
					basePattern = &gJuly4Pattern;
					break;

				case eHoliday_Holloween:
					basePattern = &gHalloweenPattern;
					break;

				default:
					basePattern = NULL;
			}
		}

		SystemMsg("Setting pattern to %s", basePattern == NULL ? "None" : basePattern->GetName());
	}

	OctoWS2811	leds;
	
	uint8_t		viewMode;
	bool		toggleState;

	SFloatPixel		frameBuffer[eLEDCount];
	CBasePattern*	basePattern;

	uint64_t	cyclePatternTimeMS;
	int			cyclePatternCount;
	float		testPatternValue;

	SSettings	settings;

	ILuminosity*	luminosityInterface;

	int		timeOfDay;
	bool	ledsOn;
	bool	motionSensorTriggered;
};

MModuleImplementation_Start(COutdoorLightingModule)
MModuleImplementation_Finish(COutdoorLightingModule)

void
SetupFHOutdoorLighting(
	void)
{
	COutdoorLightingModule::Include();
}
