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

	To a degree this follows the model view controller pattern. 
	Model: The time of day(broken up into daytime, night, and late night), date, motion sensor state, luminosity, and a push button toggle state
	View: The RGB value of each LED and the LED transformer state
	Controller: Determines the view based on the model
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

#define MHardwarePresent 0	// For testing without led strips or sensors connected

enum
{
	eTransformRelayPin = 1,			// This output pin controls the relay for the main power transformer to the leds
	eToggleButtonPin = 22,			// This input pin is for a pushbutton that forces the leds on or off and can activate a test pattern and cycle through the holiday base patterns
	eMotionSensorPin = 23,

	ePanelCount = 10,				// This is the number of led panels that go across the roof soffit
	eLEDsPerPanel = 38,				// This is the number of leds per panel
	eLEDPanelsCenterToRight = 4,	// This is the number of panels on the right side of the house roof (control system is at top center of the gable roof)
	eLEDPanelsCenterToLeft = 6,		// This is the number of panels on the left side of the house roof (control system is at top center of the gable roof)
	eLEDsPerStrip = eLEDPanelsCenterToLeft * eLEDsPerPanel,	// The total leds per strip is the max of the two strips in use
	eLEDCount = ePanelCount * eLEDsPerPanel,	// The total number of leds across the roof

	eLEDStripCenterToRight = 3,		// This is the Octo strip number (starting from 0) that goes from the center towards the right
	eLEDStripCenterToLeft = 0,		// This is the Octo strip number (starting from 0) that goes from the center towards the left

	eMaxPatternCount = 10,		// maximum number of led patterns

	eToggleCountResetMS = 1000,		// The time in ms to reset the pushbutton toggle count
	ePushCount_ToggleState = 1,		// The number of pushes to toggle state
	ePushCount_MotionTrip = 2,		// The number of pushes to simulate a motion trip
	ePushCount_CyclePatterns = 3,	// The number of pushes to trigger the cycling of the holiday base patterns
	ePushCount_TestPattern = 4,		// The number of pushes to trigger the test pattern

	eCyclePatternTime = 4000,	// The duration in ms for each holiday base pattern when cycling

	eTransformerWarmUpSecs = 2,	// The number of seconds to allow the transformer to warm up before 
	eLEDUpdateMS = 100,			// The number of MS to allow the leds to update before powering down the transformer
};

enum
{
	eViewMode_Normal,
	eViewMode_CyclePatterns,
	eViewMode_TestPattern,
};

enum
{
	eTimeOfDay_Day,
	eTimeOfDay_Night,
	eTimeOfDay_LateNight,
};

char const*	gTimeOfDayStr[] = {"Day", "Night", "Latenight"};
char const*	gViewModeStr[] = {"Normal", "CyclePatterns", "Test"};

const float	cTestPatternPixelsPerSec = 100.0f;	// The speed for the testing pattern

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

// A SDateRange is used to specify potentially multiple dates (ex Easter) for a base pattern 
struct SDateRange
{
	int	year;					// -1 means any year
	int	firstMonth, firstDay;	// the first month and day the pattern should be applied
	int	lastMonth, lastDay;		// the last month and day (inclusive) the pattern should be applied
};

struct SSettings
{
	SFloatPixel	defaultColor;
	float		defaultIntensity;
	float		activeIntensity;
	float		minLux;
	float		maxLux;
	float		triggerLux;
	int			lateNightStartHour;
	int			lateNightStartMin;
	int			motionTripTimeoutMins;
	int			lateNightTimeoutMins;
};

// Patterns inherit from CBasePattern
class CBasePattern
{
public:
	
	CBasePattern(
		)
	{
		// Add to the global list of patterns
		MAssert(gPatternCount < eMaxPatternCount);
		gPatternList[gPatternCount++] = this;
	}

	// This does the actual drawing. must be defined by the derived class
	virtual void
	Draw(
		int				inPixels,
		SFloatPixel*	inPixelMem) = 0;
	
	// The date ranges
	int					dateRangeCount;
	SDateRange const*	dateRangeArray;
};

// The various patterns are defined below

SDateRange	gXMasDateRange[] = {-1, 12, 1, 12, 26};
class CXMasPattern : public CBasePattern
{
public:

	CXMasPattern(
		)
		:
		CBasePattern()
	{
		dateRangeCount = 1;
		dateRangeArray = gXMasDateRange;
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
};
static CXMasPattern	gXMasPattern;

SDateRange	gValintineDateRange[] = {-1, 2, 14, 2, 14};
class CValintinePattern : public CBasePattern
{
public:

	CValintinePattern(
		)
		:
		CBasePattern()
	{
		dateRangeCount = 1;
		dateRangeArray = gValintineDateRange;
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
};
static CValintinePattern	gValintinePattern;

SDateRange	gJuly4DateRange[] = {-1, 7, 3, 7, 4};
class CJuly4Pattern : public CBasePattern
{
public:

	CJuly4Pattern(
		)
		:
		CBasePattern()
	{
		dateRangeCount = 1;
		dateRangeArray = gJuly4DateRange;
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
};
static CJuly4Pattern	gJuly4Pattern;

SDateRange	gHolloweenDateRange[] = {-1, 10, 30, 10, 31};
class CHolloweenPattern : public CBasePattern
{
public:

	CHolloweenPattern(
		)
		:
		CBasePattern()
	{
		dateRangeCount = 1;
		dateRangeArray = gHolloweenDateRange;
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
};
static CHolloweenPattern	gHolloweenPattern;

SDateRange	gStPattyDateRange[] = {-1, 3, 16, 3, 17};
class CStPattyPattern : public CBasePattern
{
public:

	CStPattyPattern(
		)
		:
		CBasePattern()
	{
		dateRangeCount = 1;
		dateRangeArray = gStPattyDateRange;
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
};
static CStPattyPattern	gStPattyPattern;

SDateRange	gEasterDateRange[] =
{
	2016, 3, 27, 3, 28,
	2017, 4, 15, 4, 16,
	2018, 3, 31, 4, 1,
	2019, 4, 20, 4, 21,
	2020, 4, 11, 4, 12,
	2021, 4, 3, 4, 4,
	2022, 4, 16, 4, 17,
	2023, 4, 8, 4, 9,
	2024, 3, 30, 3, 31,
	2025, 4, 19, 4, 20,
	2026, 4, 4, 4, 5,
	2027, 3, 27, 3, 28,
	2028, 4, 15, 4, 16,
	2029, 3, 31, 4, 1,
	2030, 4, 20, 4, 21,
	2031, 4, 12, 4, 13,
	2032, 3, 27, 3, 28,
	2033, 4, 16, 4, 17,
	2034, 4, 8, 4, 9,
	2035, 3, 24, 3, 25,
	2036, 4, 12, 4, 13,
	2037, 4, 4, 4, 5,
	2038, 4, 24, 4, 25,
	2039, 4, 9, 4, 10,
	2040, 3, 31, 4, 1,
	2041, 4, 20, 4, 21,
	2042, 4, 5, 4, 6,
	2043, 3, 28, 3, 29,
	2044, 4, 16, 4, 17,
	2045, 4, 8, 4, 9,
	2046, 3, 24, 3, 25,
	2047, 4, 13, 4, 14,
	2048, 4, 4, 4, 5,
	2049, 4, 17, 4, 18,
};

class CEasterPattern : public CBasePattern
{
public:

	CEasterPattern(
		)
		:
		CBasePattern()
	{
		dateRangeCount = sizeof(gEasterDateRange) / sizeof(gEasterDateRange[0]);
		dateRangeArray = gEasterDateRange;
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
};
static CEasterPattern	gEasterPattern;

// This defines our main module
class COutdoorLightingModule : public CModule, public IRealTimeHandler, public ISunRiseAndSetEventHandler, public IDigitalIOEventHandler, public ICmdHandler, public IInternetHandler
{
public:
	
	COutdoorLightingModule(
		)
		:
		CModule(
			"otdr",
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
		timeOfDay = 0;
		toggleState = false;
		motionSensorTrip = false;
		curTransformerState = false;
		curTransformerTransitionState = false;
		luxTriggerState = false;
		memset(frameBuffer, 0, sizeof(0));
		basePattern = NULL;
		toggleCount = 0;
		toggleLastTimeMS = 0;
		cyclePatternTimeMS = 0;
		cyclePatternCount = 0;
		testPatternValue = 0;
	}

	void
	Setup(
		void)
	{
		// Setup the transformer relay output pin
		pinMode(eTransformRelayPin, OUTPUT);
		digitalWriteFast(eTransformRelayPin, false);

		// Configure the time provider on the standard SPI chip select pin
		IRealTimeDataProvider*	ds3234Provider = gRealTime->CreateDS3234Provider(10);
		gRealTime->SetProvider(ds3234Provider, 24 * 60 * 60);

		IInternetDevice*	internetDevice = GetInternetDevice_ESP8266(&Serial1);
		gInternet->SetInternetDevice(internetDevice);
		gInternet->ServeCommands(8080, this, static_cast<TInternetServerHandlerMethod>(&COutdoorLightingModule::CommandHomePageHandler));

		CModule_Loggly*	loggly = new CModule_Loggly("front_house", "logs-01.loggly.com", "/inputs/568b321d-0d6f-47d3-ac34-4a36f4125612");
		AddSysMsgHandler(loggly);

		// Register the alarms, events, and commands
		gSunRiseAndSet->RegisterSunsetEvent("Sunset1", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, this, static_cast<TSunRiseAndSetEventMethod>(&COutdoorLightingModule::Sunset));
		gSunRiseAndSet->RegisterSunriseEvent("Sunrise1", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, this, static_cast<TSunRiseAndSetEventMethod>(&COutdoorLightingModule::Sunrise));
		gRealTime->RegisterAlarm("NightTurnOffAlarm", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, settings.lateNightStartHour, settings.lateNightStartMin, 0, this, static_cast<TRealTimeAlarmMethod>(&COutdoorLightingModule::NightTurnOffAlarm), NULL);
		gDigitalIO->RegisterEventHandler(eToggleButtonPin, false, this, static_cast<TDigitalIOEventMethod>(&COutdoorLightingModule::ButtonPush), NULL, 100);
		gDigitalIO->RegisterEventHandler(eMotionSensorPin, false, this, static_cast<TDigitalIOEventMethod>(&COutdoorLightingModule::MotionSensorTrigger), NULL);
		gCommand->RegisterCommand("set_toggle", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::SetToggleState));
		gCommand->RegisterCommand("test_pattern", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::TestPattern));
		gCommand->RegisterCommand("set_color", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::SetColor));
		gCommand->RegisterCommand("get_color", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::GetColor));
		gCommand->RegisterCommand("set_intensity", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::SetIntensity));
		gCommand->RegisterCommand("get_intensity", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::GetIntensity));
		gCommand->RegisterCommand("set_latenightstarttime", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::SetLateNightStartTime));
		gCommand->RegisterCommand("get_latenightstarttime", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::GetLateNightStartTime));
		gCommand->RegisterCommand("set_luxminmax", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::SetMinMaxLux));
		gCommand->RegisterCommand("get_luxminmax", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::GetMinMaxLux));
		gCommand->RegisterCommand("set_triggerlux", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::SetTriggerLux));
		gCommand->RegisterCommand("get_triggerlux", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::GetTriggerLux));
		gCommand->RegisterCommand("set_motionTO", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::SetMotionTripTimeout));
		gCommand->RegisterCommand("get_motionTO", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::GetMotionTripTimeout));
		gCommand->RegisterCommand("set_latenightTO", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::SetLateNightTimeout));
		gCommand->RegisterCommand("get_latenightTO", this, static_cast<TCmdHandlerMethod>(&COutdoorLightingModule::GetLateNightTimeout));

		#if MHardwarePresent
		gLuminositySensor->SetEnabledState(true);
		gLuminositySensor->SetMinMaxLux(settings.minLux, settings.maxLux, false);

		leds.begin();
		#endif

		// Setup the initial state given the current time of day and sunset/sunrise time
		TEpochTime	curTime = gRealTime->GetEpochTime(false);
		int	year, month, day, dow, hour, min, sec;
		gRealTime->GetComponentsFromEpochTime(curTime, year, month, day, dow, hour, min, sec);

		SystemMsg("Setup time is %02d/%02d/%04d %02d:%02d:%02d\n", month, day, year, hour, min, sec);

		TEpochTime sunriseTime, sunsetTime;
		gSunRiseAndSet->GetSunRiseAndSetEpochTime(sunriseTime, sunsetTime, year, month, day, false);
		
		int	eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec;
		gRealTime->GetComponentsFromEpochTime(sunriseTime, eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec);
		SystemMsg("Setup sunrise time is %02d/%02d/%04d %02d:%02d:%02d\n", eventMonth, eventDay, eventYear, eventHour, eventMin, eventSec);
		gRealTime->GetComponentsFromEpochTime(sunsetTime, eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec);
		SystemMsg("Setup sunset time is %02d/%02d/%04d %02d:%02d:%02d\n", eventMonth, eventDay, eventYear, eventHour, eventMin, eventSec);

		TEpochTime lateNightTime = gRealTime->GetEpochTimeFromComponents(year, month, day, settings.lateNightStartHour, settings.lateNightStartMin, 0);

		if(curTime > sunsetTime || curTime < sunriseTime)
		{
			if(lateNightTime < curTime || curTime < sunriseTime)
			{
				timeOfDay = eTimeOfDay_LateNight;
			}
			else
			{
				timeOfDay = eTimeOfDay_Night;
			}
		}
		else
		{
			timeOfDay = eTimeOfDay_Day;
		}

		SystemMsg("Setup time of day = %d\n", timeOfDay);

		toggleState = timeOfDay == eTimeOfDay_Night;
		SetTransformerState(timeOfDay != eTimeOfDay_Day);
		viewMode = eViewMode_Normal;

		gRealTime->RegisterEvent("LuxPeriodic", 5 * 60 * 1000000, false, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::LuxPeriodic), NULL);

		// Update lux trigger
		float	curLux = gLuminositySensor->GetActualLux();
		luxTriggerState = settings.triggerLux < curLux;
	}

	void
	CommandHomePageHandler(
		IOutputDirector*	inOutput,
		int					inDataSize,
		char const*			inData)
	{
		// Send html via in Output to add to the command server home page served to clients

		inOutput->printf("<table border=\"1\">");
		inOutput->printf("<tr><th>Parameter</th><th>Value</th></tr>");

		// Add cur local time
		int	year, month, dom, dow, hour, min, sec;
		gRealTime->GetDateAndTime(year, month, dom, dow, hour, min, sec, false);
		inOutput->printf("<tr><td>Time</td><td>%04d-%02d-%02d %02d:%02d:%02d</td></tr>", year, month, dom, hour, min, sec);

		// Add sunset time
		TEpochTime sunsetTime = gSunRiseAndSet->GetSunsetEpochTime(year, month, dom, false);
		gRealTime->GetComponentsFromEpochTime(sunsetTime, year, month, dom, dow, hour, min, sec);
		inOutput->printf("<tr><td>Sunset Time</td><td>%04d-%02d-%02d %02d:%02d:%02d</td></tr>", year, month, dom, hour, min, sec);

		// add timeOfDay
		inOutput->printf("<tr><td>Time Of Day</td><td>%s</td></tr>", gTimeOfDayStr[timeOfDay]);

		// Add cur lux
		inOutput->printf("<tr><td>Actual Lux</td><td>%f</td></tr>", gLuminositySensor->GetActualLux());

		// add viewMode
		inOutput->printf("<tr><td>View Mode</td><td>%s</td></tr>", gViewModeStr[timeOfDay]);

		// add toggleState
		inOutput->printf("<tr><td>Toggle State</td><td>%s</td></tr>", toggleState ? "On" : "Off");

		// add motionSensorTrip
		inOutput->printf("<tr><td>Motion Sensor Trip</td><td>%s</td></tr>", motionSensorTrip ? "On" : "Off");

		// add curTransformerState
		inOutput->printf("<tr><td>Transformer State</td><td>%s</td></tr>", curTransformerState ? "On" : "Off");

		// add luxTriggerState
		inOutput->printf("<tr><td>Lux Trigger State</td><td>%s</td></tr>", luxTriggerState ? "On" : "Off");

		// add settings.defaultColor
		inOutput->printf("<tr><td>Default Color</td><td>r:%02.02f g:%02.02f b:%02.02f</td></tr>", settings.defaultColor.r, settings.defaultColor.g, settings.defaultColor.b);

		// add settings.defaultIntensity
		inOutput->printf("<tr><td>Default Intensity</td><td>%02.02f</td></tr>", settings.defaultIntensity);

		// add settings.activeIntensity
		inOutput->printf("<tr><td>Active Intensity</td><td>%02.02f</td></tr>", settings.activeIntensity);

		// add settings.minLux, settings.maxLux
		inOutput->printf("<tr><td>Lux Range</td><td>%f %f</td></tr>", settings.minLux, settings.maxLux);

		// add settings.triggerLuxMin, settings.triggerLuxMax
		inOutput->printf("<tr><td>Trigger Lux</td><td>%f</td></tr>", settings.triggerLux);

		// add settings.lateNightStartHour and min
		inOutput->printf("<tr><td>Late Night Start</td><td>%02d:%02d</td></tr>", settings.lateNightStartHour, settings.lateNightStartMin);

		// add settings.motionTripTimeoutMins
		inOutput->printf("<tr><td>Motion Trip Timeout</td><td>%d</td></tr>", settings.motionTripTimeoutMins);

		// add settings.lateNightTimeoutMins
		inOutput->printf("<tr><td>Late Night Toggle Timeout</td><td>%d</td></tr>", settings.lateNightTimeoutMins);

		inOutput->printf("</table>");
	}

	void
	Update(
		uint32_t inDeltaTimeUS)
	{
		// Check on the toggle button
		if(gCurLocalMS - toggleLastTimeMS >= eToggleCountResetMS && toggleCount > 0)
		{
			// It has timed out so now check the toggle count for the action

			motionSensorTrip = false;	// Ensure motion trip is off

			switch(toggleCount)
			{
				case ePushCount_ToggleState:
					if(viewMode == eViewMode_Normal)
					{
						toggleState = !toggleState;
						SystemMsg("Toggle state to %d\n", toggleState);

						if(timeOfDay == eTimeOfDay_Day)
						{
							SetTransformerState(toggleState);
						}
						else if(timeOfDay == eTimeOfDay_LateNight)
						{
							if(toggleState)
							{
								gRealTime->RegisterEvent("LateNight", settings.lateNightTimeoutMins * 60 * 1000000, true, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::LateNightTimerExpire), NULL);
							}
							else
							{
								gRealTime->CancelEvent("LateNight");
							}
						}
					}
					else
					{
						viewMode = eViewMode_Normal;
					}

					FindBasePattern();
					break;

				case ePushCount_MotionTrip:
					SystemMsg("Simulating motion sensor trip\n");
					MotionSensorTrigger(eMotionSensorPin, eDigitalIO_PinActivated, NULL);
					MotionSensorTrigger(eMotionSensorPin, eDigitalIO_PinDeactivated, NULL);
					toggleState = true;
					break;

				case ePushCount_CyclePatterns:
					SystemMsg("Entering pattern cycling\n");
					viewMode = eViewMode_CyclePatterns;
					SetTransformerState(true);
					break;

				case ePushCount_TestPattern:
					SystemMsg("Entering test pattern\n");
					viewMode = eViewMode_TestPattern;
					SetTransformerState(true);
					break;
			}

			SystemMsg("Reseting toggle count, was %d\n", toggleCount);
			toggleCount = 0;
		}

		if(curTransformerState == false)
		{
			// If the transformer is not on don't update LEDs
			return;
		}

		switch(viewMode)
		{
			case eViewMode_Normal:
			{
				bool	lightsOn = toggleState;

				if(timeOfDay != eTimeOfDay_Day)
				{
					lightsOn |= motionSensorTrip;
				}
				else
				{
					lightsOn |= luxTriggerState;
				}

				if(lightsOn)
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
						if(motionSensorTrip)
						{
							intensity = settings.activeIntensity;
						}
						else
						{
							intensity = (1.0f - gLuminositySensor->GetNormalizedBrightness()) * settings.defaultIntensity;
						}
					}

					// Perhaps eventually apply some effects here

					for(uint32_t itr = 0; itr < eLEDCount; ++itr)
					{
						SetRoofPixel(itr, uint8_t(frameBuffer[itr].r * intensity * 255.0), uint8_t(frameBuffer[itr].g * intensity * 255.0), uint8_t(frameBuffer[itr].b * intensity * 255.0));
					}
				}
				else
				{
					for(uint32_t itr = 0; itr < eLEDCount; ++itr)
					{
						SetRoofPixel(itr, 0, 0, 0);
					}
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
					SetRoofPixel(itr, int(frameBuffer[itr].r * 255.0), int(frameBuffer[itr].g * 255.0), int(frameBuffer[itr].b * 255.0));
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

		#if MHardwarePresent
		leds.show();
		#endif
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

	void
	Sunset(
		char const*	inName)
	{
		SystemMsg("Sunset\n");

		// Update state
		timeOfDay = eTimeOfDay_Night;
		toggleState = true;

		// Turn on the transformer
		SetTransformerState(true);

		// Ensure we have the right base pattern
		FindBasePattern();

		SystemMsg("lux: lux = %f\n", gLuminositySensor->GetActualLux());
	}

	void
	LuxPeriodic(
		char const*	inName,
		void*		inRef)
	{
		// Update lux trigger
		float	curLux = gLuminositySensor->GetActualLux();
		luxTriggerState = curLux < settings.triggerLux;
		SystemMsg("lux: lux = %f, luxTriggerState=%d\n", curLux, luxTriggerState);
	}

	void
	Sunrise(
		char const*	inName)
	{
		SystemMsg("Sunrise\n");

		// Update state
		timeOfDay = eTimeOfDay_Day;
		toggleState = false;

		// Turn off the transformer
		SetTransformerState(false);
	}

	bool
	NightTurnOffAlarm(
		char const*	inName,
		void*		inRef)
	{
		SystemMsg("Late Night Alarm\n");

		// Update state
		timeOfDay = eTimeOfDay_LateNight;
		toggleState = false;

		return true;	// return true to reschedule the alarm
	}

	void
	LateNightTimerExpire(
		char const*	inName,
		void*		inRef)
	{
		SystemMsg("Late night timer expired\n");
		toggleState = false;
	}

	void
	ButtonPush(
		uint8_t		inPin,
		EPinEvent	inEvent,
		void*		inReference)
	{
		if(inEvent == eDigitalIO_PinActivated)
		{
			toggleLastTimeMS = gCurLocalMS;
			++toggleCount;
			SystemMsg("toggleCount = %d\n", toggleCount);
		}
	}

	void
	MotionSensorTrigger(
		uint8_t		inPin,
		EPinEvent	inEvent,
		void*		inReference)
	{
		if(inEvent == eDigitalIO_PinActivated)
		{
			SystemMsg("Motion sensor tripped\n");
			motionSensorTrip = true;
		}
		else if(motionSensorTrip)
		{
			if(timeOfDay != eTimeOfDay_Day)
			{
				SystemMsg("Motion sensor off, setting cooldown timer\n");

				// Set an event for settings.motionTripTimeoutMins mins from now
				gRealTime->RegisterEvent("MotionTripCD", settings.motionTripTimeoutMins * 60 * 1000000, true, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::MotionTripCooldown), NULL);
			}
			else
			{
				SystemMsg("Motion sensor off, daytime, no cooldown timer\n");
				motionSensorTrip = false;
			}
		}
	}

	void
	MotionTripCooldown(
		char const*	inName,
		void*		inRef)
	{
		SystemMsg("Motion trip cooled down\n");
		motionSensorTrip = false;
	}
	
	uint8_t
	SetToggleState(
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
			toggleState = true;
		}
		else if(strcmp(inArgv[1], "off") == 0)
		{
			toggleState = false;
		}
		else
		{
			return eCmd_Failed;
		}

		viewMode = eViewMode_Normal;
		FindBasePattern();

		if(timeOfDay == eTimeOfDay_Day)
		{
			SetTransformerState(toggleState);
		}

		return eCmd_Succeeded;
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
			SetTransformerState(true);
			viewMode = eViewMode_TestPattern;
			return true;
		}
		else if(strcmp(inArgv[1], "off") == 0)
		{
			if(timeOfDay == eTimeOfDay_Day)
			{
				SetTransformerState(false);
			}
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
	SetLateNightStartTime(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		if(inArgC != 3)
		{
			return eCmd_Failed;
		}

		settings.lateNightStartHour = atoi(inArgv[1]);
		settings.lateNightStartMin = atoi(inArgv[2]);
		
		gRealTime->RegisterAlarm("NightTurnOffAlarm", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, settings.lateNightStartHour, settings.lateNightStartMin, 0, this, static_cast<TRealTimeAlarmMethod>(&COutdoorLightingModule::NightTurnOffAlarm), NULL);

		EEPROMSave();

		return eCmd_Succeeded;
	}

	uint8_t
	GetLateNightStartTime(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		inOutput->printf("%02d:%02d\n", settings.lateNightStartHour, settings.lateNightStartMin);

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

	uint8_t
	SetTriggerLux(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		if(inArgC != 2)
		{
			return eCmd_Failed;
		}

		settings.triggerLux = (float)atof(inArgv[1]);

		EEPROMSave();

		return eCmd_Succeeded;
	}

	uint8_t
	GetTriggerLux(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		inOutput->printf("%f\n", settings.triggerLux);

		return eCmd_Succeeded;
	}

	uint8_t
	SetMotionTripTimeout(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		if(inArgC != 2)
		{
			return eCmd_Failed;
		}

		settings.motionTripTimeoutMins = atoi(inArgv[1]);

		EEPROMSave();

		return eCmd_Succeeded;
	}

	uint8_t
	GetMotionTripTimeout(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		inOutput->printf("%d\n", settings.motionTripTimeoutMins);

		return eCmd_Succeeded;
	}

	uint8_t
	SetLateNightTimeout(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		if(inArgC != 2)
		{
			return eCmd_Failed;
		}

		settings.lateNightTimeoutMins = atoi(inArgv[1]);

		EEPROMSave();

		return eCmd_Succeeded;
	}

	uint8_t
	GetLateNightTimeout(
		IOutputDirector*	inOutput,
		int					inArgC,
		char const*			inArgv[])
	{
		inOutput->printf("%d\n", settings.lateNightTimeoutMins);

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
		for(int i = 0; i < gPatternCount && basePattern == NULL; ++i)
		{
			CBasePattern*	curPattern = gPatternList[i];

			SDateRange const*	curDateRange = curPattern->dateRangeArray;
			for(int j = 0; j < curPattern->dateRangeCount; ++j, ++curDateRange)
			{
				if((curDateRange->year == -1 || curDateRange->year == year)
					&& (curDateRange->firstMonth <= month && curDateRange->firstDay <= day)
					&& (month <= curDateRange->lastMonth && day <= curDateRange->lastDay))
				{
					basePattern = curPattern;
					break;
				}
			}
		}
	}

	void
	SetTransformerState(
		bool	inState)
	{
		if(curTransformerTransitionState == inState)
		{
			return;
		}

		SystemMsg("Transformer state to %d\n", inState);

		curTransformerTransitionState = inState;

		if(inState)
		{
			digitalWriteFast(eTransformRelayPin, true);

			// Let transformer warm up for 2 seconds
			gRealTime->RegisterEvent("XfmrWarmup", eTransformerWarmUpSecs * 1000000, true, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::TransformerTransitionEvent), NULL);
		}
		else
		{
			// Set the cur state to false right away to stop updating leds
			curTransformerState = false;

			// Set all LEDs to off
			for(uint32_t itr = 0; itr < eLEDCount; ++itr)
			{
				SetRoofPixel(itr, 0, 0, 0);
			}
			
			#if MHardwarePresent
			leds.show();
			#endif

			// Let led update finish
			gRealTime->RegisterEvent("XfmrWarmup", eLEDUpdateMS * 1000, true, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::TransformerTransitionEvent), NULL);
		}
	}

	void
	TransformerTransitionEvent(
		char const*	inName,
		void*		inRef)
	{
		SystemMsg("Transformer transition %d\n", curTransformerTransitionState);
		curTransformerState = curTransformerTransitionState;
		digitalWriteFast(eTransformRelayPin, curTransformerState);
	}

	OctoWS2811	leds;
	
	uint8_t		viewMode;
	uint8_t		timeOfDay;
	bool		toggleState;
	bool		motionSensorTrip;
	bool		curTransformerState;
	bool		curTransformerTransitionState;
	bool		luxTriggerState;

	SFloatPixel		frameBuffer[eLEDCount];
	CBasePattern*	basePattern;

	int			toggleCount;
	uint64_t	toggleLastTimeMS;
	uint64_t	cyclePatternTimeMS;
	int			cyclePatternCount;
	float		testPatternValue;

	SSettings	settings;
};

void
SetupFHOutdoorLighting(
	void)
{
	new COutdoorLightingModule();
}
