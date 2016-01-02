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

	This is the main module for a front of house lighting project
*/

#include <ELModule.h>
#include <ELRealTime.h>
#include <ELDigitalIO.h>
#include <ELSunRiseAndSet.h>
#include <ELAssert.h>
#include <ELLuminositySensor.h>
#include <ELUtilities.h>

#if defined(WIN32)
// for testing
	#define DMAMEM
	#define WS2811_RGB 1
	#define WS2811_400kHz 2

	class OctoWS2811
	{
	public:
		
		OctoWS2811(
			int		inCount,
			int*	inDisplay,
			int*	inDraw,
			int		inFlags)
		{

		}

		void
		begin(
			void)
		{

		}

		void
		show(
			void)
		{

		}

		void
		setPixel(
			int	inIndex,
			uint8_t	inR,
			uint8_t	inG,
			uint8_t	inB)
		{

		}
	};

#else
	#include <OctoWS2811.h>
#endif

enum
{
	eTransformRelayPin = 1,			// This output pin controls the relay for the main power transformer to the leds
	eToggleButtonPin = 22,			// This input pin is for a pushbutton that forces the leds on or off and can activate a test pattern and cycle through the holiday base patterns
	eMotionSensorPin = 23,

	ePanelCount = 10,				// This is the number of led panels that go across the roof soffit
	eLEDsPerPanel = 38,				// This is the number of leds per panel
	eLEDPanelsCenterToRight = 4,	// This is the number of panels on the right side of the house roof (control system is at top center of the gable roof)
	eLEDPanelsCenterToLeft = 6,		// This is the number of panels on the left side of the house roof (control system is at top center of the gable roof)

	eLEDStripCenterToRight = 3,		// This is the Octo strip number that goes from the center towards the right
	eLEDStripCenterToLeft = 0,		// This is the Octo strip number that goes from the center towards the left

	eLEDsPerStrip = eLEDPanelsCenterToLeft * eLEDsPerPanel,	// The total leds per strip is the max of the two strips in use

	eMotionTripLightsOffSecs = 15 * 60,	// How long to keep the lights on after motion sensor trip has gone off
	eLateNightTimeoutSecs = 20 * 60,	// How long to keep the lights on after a toggle on during late night

	eLEDCount = ePanelCount * eLEDsPerPanel,	// The total number of leds across the roof

	eMaxPatternCount = 10,	// maximum number of led patterns

	eToggleCountResetMS = 1000,	// The time in ms to reset the pushbutton toggle count
	eCyclePatternsCount = 3,	// The number of pushes to trigger the cycling of the holiday base patterns
	eTestPatternCount = 4,		// The number of pushes to trigger the test pattern

	eCyclePatternTime = 4000,	// The duration in ms for each holiday base pattern when cycling

	eTransformerWarmUpSecs = 2,	// The number of seconds to allow the transformer to warm up before 
	eLEDUpdateMS = 100,			// The number of MS to allow the leds to update before powering down the transformer
};

enum
{
	eMode_Normal,
	eMode_CyclePatterns,
	eMode_TestPattern,
};

enum
{
	eTimeOfDay_Day,
	eTimeOfDay_Night,
	eTimeOfDay_LateNight,
};

const float	cTestPatternPixelsPerSec = 100.0f;	// The speed for the testing pattern

class CBasePattern;

DMAMEM int		gLEDDisplayMemory[eLEDsPerStrip * 6];
int				gLEDDrawingMemory[eLEDsPerStrip * 6];
int				gLEDMap[eLEDCount];
int				gPatternCount;
CBasePattern*	gPatternList[eMaxPatternCount];

// A CBasePattern will fill in an array of these, float is used for easy manipulation by lighting intensity and future effect modifiers
struct SFloatPixel
{
	float	r, g, b;
};

// A SDateRange is used to specify potentially multiple dates (for Easter) for a base pattern 
struct SDateRange
{
	int	year;					// -1 means any year
	int	firstMonth, firstDay;	// the first month and day the pattern should be applied
	int	lastMonth, lastDay;		// the last day (inclusive) the pattern should be applied
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

SDateRange	gValintineDateRange[1] = {-1, 2, 14, 2, 14};
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

SDateRange	gJuly4DateRange[1] = {-1, 7, 3, 7, 4};
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

SDateRange	gHolloweenDateRange[1] = {-1, 10, 30, 10, 31};
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
				inPixelMem[itr].r = 1.0;
				inPixelMem[itr].g = 0.65;
				inPixelMem[itr].b = 0.0;
			}
			else
			{
				inPixelMem[itr].r = 0.5;
				inPixelMem[itr].g = 0.0;
				inPixelMem[itr].b = 0.5;
			}
		}
	}
};
static CHolloweenPattern	gHolloweenPattern;

SDateRange	gStPattyDateRange[1] = {-1, 3, 16, 3, 17};
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
			inPixelMem[itr].r = 0.0;
			inPixelMem[itr].g = 1.0;
			inPixelMem[itr].b = 0.0;
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
				inPixelMem[itr].r = 1.0;
				inPixelMem[itr].g = 1.0;
				inPixelMem[itr].b = 0.0;
			}
			else if(primaryColor == 1)
			{
				inPixelMem[itr].r = 0.5;
				inPixelMem[itr].g = 0.0;
				inPixelMem[itr].b = 0.5;
			}
			else if(primaryColor == 2)
			{
				inPixelMem[itr].r = 1.0;
				inPixelMem[itr].g = 0.0;
				inPixelMem[itr].b = 0.0;
			}
			else if(primaryColor == 3)
			{
				inPixelMem[itr].r = 0.0;
				inPixelMem[itr].g = 1.0;
				inPixelMem[itr].b = 0.0;
			}
			else if(primaryColor == 4)
			{
				inPixelMem[itr].r = 0.0;
				inPixelMem[itr].g = 0.0;
				inPixelMem[itr].b = 1.0;
			}
			else if(primaryColor == 5)
			{
				inPixelMem[itr].r = 1.0;
				inPixelMem[itr].g = 0.41;
				inPixelMem[itr].b = 0.71;
			}
			else
			{
				inPixelMem[itr].r = 1.0;
				inPixelMem[itr].g = 0.65;
				inPixelMem[itr].b = 0.0;
			}
		}
	}
};
static CEasterPattern	gEasterPattern;

// This defines our main module
class COutdoorLightingModule : public CModule, public IRealTimeHandler, public ISunRiseAndSetEventHandler, public IDigitalIOEventHandler, public ISerialCmdHandler
{
	
	COutdoorLightingModule(
		)
		:
		CModule(
			"otdr",
			sizeof(SFloatPixel),
			0,
			30000),
		leds(eLEDsPerStrip, gLEDDisplayMemory, gLEDDrawingMemory, WS2811_RGB)
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
	}

	void
	Setup(
		void)
	{
		// Setup the transformer relay output pin
		pinMode(eTransformRelayPin, OUTPUT);
		digitalWriteFast(eTransformRelayPin, false);

		// Configure the time provider on the standard SPI chip select pin
		ds3234Provider = gRealTime->CreateDS3234Provider(10);
		gRealTime->SetProvider(ds3234Provider, 24 * 60 * 60);

		LoadDataFromEEPROM(&defaultColor, eepromOffset, sizeof(defaultColor));

		#if defined(WIN32)
		// For testing
		gSunRiseAndSet->SetLongitudeAndLatitude(-118.176863, 34.102653);
		#endif

		// Register the alarms, events, and commands
		gSunRiseAndSet->RegisterSunsetEvent("Sunset1", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, this, static_cast<TSunRiseAndSetEventMethod>(&COutdoorLightingModule::Sunset));
		gSunRiseAndSet->RegisterSunriseEvent("Sunrise1", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, this, static_cast<TSunRiseAndSetEventMethod>(&COutdoorLightingModule::Sunrise));
		gRealTime->RegisterAlarm("LateNightAlarm", eAlarm_Any, eAlarm_Any, eAlarm_Any, eAlarm_Any, 23, 0, 0, this, static_cast<TRealTimeAlarmMethod>(&COutdoorLightingModule::LateNightAlarm), NULL);
		gDigitalIO->RegisterEventHandler(eToggleButtonPin, false, this, static_cast<TDigitalIOEventMethod>(&COutdoorLightingModule::ButtonPush), NULL, 100);
		gDigitalIO->RegisterEventHandler(eMotionSensorPin, false, this, static_cast<TDigitalIOEventMethod>(&COutdoorLightingModule::MotionSensorTrigger), NULL);
		gSerialCmd->RegisterCommand("force", this, static_cast<TSerialCmdMethod>(&COutdoorLightingModule::ForceOnOff));
		gSerialCmd->RegisterCommand("test_pattern", this, static_cast<TSerialCmdMethod>(&COutdoorLightingModule::TestPattern));
		gSerialCmd->RegisterCommand("set_color", this, static_cast<TSerialCmdMethod>(&COutdoorLightingModule::SetColor));

		gLuminositySensor->SetMinMaxLux(30.0, 1500, false);

		leds.begin();

		// Setup the initial state given the current time of day and sunset/sunrise time
		TEpochTime	curTime = gRealTime->GetEpochTime(false);
		int	year, month, day, dow, hour, min, sec;
		gRealTime->GetComponentsFromEpochTime(curTime, year, month, day, dow, hour, min, sec);

		DebugMsg(eDbgLevel_Basic, "current time is %02d/%02d/%04d %02d:%02d:%02d\n", month, day, year, hour, min, sec);

		TEpochTime sunriseTime, sunsetTime;
		gSunRiseAndSet->GetSunRiseAndSetEpochTime(sunriseTime, sunsetTime, year, month, day, false);
		
		int	eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec;
		gRealTime->GetComponentsFromEpochTime(sunriseTime, eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec);
		DebugMsg(eDbgLevel_Basic, "sunrise time is %02d/%02d/%04d %02d:%02d:%02d\n", eventMonth, eventDay, eventYear, eventHour, eventMin, eventSec);
		gRealTime->GetComponentsFromEpochTime(sunsetTime, eventYear, eventMonth, eventDay, eventDOW, eventHour, eventMin, eventSec);
		DebugMsg(eDbgLevel_Basic, "sunset time is %02d/%02d/%04d %02d:%02d:%02d\n", eventMonth, eventDay, eventYear, eventHour, eventMin, eventSec);

		TEpochTime lateNightTime = gRealTime->GetEpochTimeFromComponents(year, month, day, 23, 0, 0);

		motionSensorTrip = false;
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

		DebugMsg(eDbgLevel_Basic, "time of day = %d\n", timeOfDay);

		SetTransformerState(timeOfDay != eTimeOfDay_Day);
		lightsOn = timeOfDay == eTimeOfDay_Night;
		mode = eMode_Normal;
	}

	void
	Update(
		uint32_t inDeltaTimeUS)
	{
		// Check on the toggle button
		if(gCurLocalMS - toggleLastTimeMS >= eToggleCountResetMS && toggleCount > 0)
		{
			// It has timed out so now check the toggle count for the action
			switch(toggleCount)
			{
				case 1:
					lightsOn = !lightsOn;
					DebugMsg(eDbgLevel_Basic, "Toggle lights to %s\n", lightsOn ? "on" : "off");
					mode = eMode_Normal;

					if(lightsOn)
					{
						SetTransformerState(true);
						FindBasePattern();
					}

					if(timeOfDay == eTimeOfDay_Day)
					{
						if(!lightsOn)
						{
							SetTransformerState(false);
						}
					}
					else if(timeOfDay == eTimeOfDay_LateNight)
					{
						if(lightsOn)
						{
							gRealTime->RegisterEvent("LateNight", eLateNightTimeoutSecs * 1000000, true, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::LateNightTimerExpire), NULL);
						}
						else
						{
							gRealTime->CancelEvent("LateNight");
						}
					}
					break;

				case eCyclePatternsCount:
					DebugMsg(eDbgLevel_Basic, "Entering pattern cycling\n");
					mode = eMode_CyclePatterns;
					lightsOn = true;
					SetTransformerState(true);
					break;

				case eTestPatternCount:
					DebugMsg(eDbgLevel_Basic, "Entering test pattern\n");
					mode = eMode_TestPattern;
					lightsOn = true;
					SetTransformerState(true);
					break;
			}

			DebugMsg(eDbgLevel_Medium, "Reseting toggle count, was %d\n", toggleCount);
			toggleCount = 0;
		}

		if(curTransformerState == false)
		{
			// If the transformer is not on don't update LEDs
			return;
		}

		float	normalizedBrightness = gLuminositySensor->GetNormalizedLux();
		float	intensity = 1.0f - normalizedBrightness;

		switch(mode)
		{
			case eMode_Normal:
				if(lightsOn || motionSensorTrip)
				{
					if(basePattern != NULL)
					{
						basePattern->Draw(eLEDCount, frameBuffer);
					}
					else
					{
						for(uint32_t itr = 0; itr < eLEDCount; ++itr)
						{
							frameBuffer[itr].r = defaultColor.r;
							frameBuffer[itr].g = defaultColor.g;
							frameBuffer[itr].b = defaultColor.b;
						}
					}

					// Perhaps eventually apply some effects here

					for(uint32_t itr = 0; itr < eLEDCount; ++itr)
					{
						SetRoofPixel(itr, int(frameBuffer[itr].r * intensity * 255.0), int(frameBuffer[itr].g * intensity * 255.0), int(frameBuffer[itr].b * intensity * 255.0));
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

			case eMode_CyclePatterns:
				if(gCurLocalMS - cyclePatternTimeMS >= eCyclePatternTime || basePattern == NULL)
				{
					DebugMsg(eDbgLevel_Verbose, "Cycling patterns\n");
					basePattern = gPatternList[cyclePatternCount++ % gPatternCount];
					cyclePatternTimeMS = gCurLocalMS;
				}
				basePattern->Draw(eLEDCount, frameBuffer);
				for(uint32_t itr = 0; itr < eLEDCount; ++itr)
				{
					SetRoofPixel(itr, int(frameBuffer[itr].r * 255.0), int(frameBuffer[itr].g * 255.0), int(frameBuffer[itr].b * 255.0));
				}
				break;

			case eMode_TestPattern:
				testPatternValue += cTestPatternPixelsPerSec * (float)inDeltaTimeUS / 1000000.0f;

				if(testPatternValue >= eLEDCount * 2.0f)
				{
					testPatternValue -= eLEDCount * 2.0f;
				}

				int	intValue = (int)testPatternValue;
				int	indexR = intValue <  eLEDCount ? intValue : eLEDCount * 2 - intValue - 1;
				int	indexG = (intValue - 1) <  eLEDCount ? (intValue - 1) : eLEDCount * 2 - (intValue - 1);
				int	indexB = (intValue - 2) <  eLEDCount ? (intValue - 2)  : eLEDCount * 2 - (intValue - 2) ;

				//DebugMsg(eDbgLevel_Basic, "%d %d %d %d\n", intValue, indexR, indexG, indexB);

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

	void
	Sunset(
		char const*	inName)
	{
		DebugMsg(eDbgLevel_Basic, "Sunset\n");

		timeOfDay = eTimeOfDay_Night;

		// Turn on the transformer
		SetTransformerState(true);
		lightsOn = true;
		FindBasePattern();

		DebugMsg(eDbgLevel_Basic, "lux = %f\n", gLuminositySensor->GetActualLux());

		duskStartTime = gRealTime->GetEpochTime(false);
		gRealTime->RegisterEvent("DuskPeriodic", 15 * 60 * 1000000, false, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::DuskPeriodic), NULL);
	}

	void
	DuskPeriodic(
		char const*	inName,
		void*		inRef)
	{
		if(gRealTime->GetEpochTime(false) - duskStartTime > 60 * 60)
		{
			gRealTime->CancelEvent("DuskPeriodic");
		}

		DebugMsg(eDbgLevel_Basic, "lux = %f\n", gLuminositySensor->GetActualLux());
	}

	void
	Sunrise(
		char const*	inName)
	{
		DebugMsg(eDbgLevel_Basic, "Sunrise\n");

		timeOfDay = eTimeOfDay_Day;

		// Turn off the transformer
		SetTransformerState(false);
		lightsOn = false;
	}

	bool
	LateNightAlarm(
		char const*	inName,
		void*		inRef)
	{
		DebugMsg(eDbgLevel_Basic, "Late Night Alarm\n");
		timeOfDay = eTimeOfDay_LateNight;
		lightsOn = false;
		// Transformer is left on for motion trip
		return true;	// return true to reschedule the alarm
	}

	void
	LateNightTimerExpire(
		char const*	inName,
		void*		inRef)
	{
		DebugMsg(eDbgLevel_Basic, "Late night timer expired\n");
		lightsOn = false;
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
			DebugMsg(eDbgLevel_Basic, "toggleCount = %d\n", toggleCount);
		}
	}

	void
	MotionSensorTrigger(
		uint8_t		inPin,
		EPinEvent	inEvent,
		void*		inReference)
	{
		if(timeOfDay == eTimeOfDay_Day)
		{
			// Don't care about motion trips during the day
			return;
		}

		if(inEvent == eDigitalIO_PinActivated)
		{
			DebugMsg(eDbgLevel_Basic, "Motion sensor tripped\n");
			motionSensorTrip = true;
		}
		else
		{
			DebugMsg(eDbgLevel_Basic, "Motion sensor off\n");

			// Set an event for eMotionTripLightsOffMins mins from now
			gRealTime->RegisterEvent("MotionTripCD", eMotionTripLightsOffSecs * 1000000, true, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::MotionTripCooldown), NULL);
		}
	}

	void
	MotionTripCooldown(
		char const*	inName,
		void*		inRef)
	{
		DebugMsg(eDbgLevel_Basic, "Motion trip cooled down\n");
		motionSensorTrip = false;
	}
	
	bool
	ForceOnOff(
		int			inArgC,
		char const*	inArgv[])
	{
		if(inArgC != 2)
		{
			return false;
		}

		if(strcmp(inArgv[1], "on") == 0)
		{
			lightsOn = true;
		}
		else if(strcmp(inArgv[1], "off") == 0)
		{
			lightsOn = false;
		}
		else
		{
			return false;
		}

		mode = eMode_Normal;
		FindBasePattern();

		if(timeOfDay == eTimeOfDay_Day)
		{
			SetTransformerState(lightsOn);
		}

		return true;
	}

	bool
	TestPattern(
		int			inArgC,
		char const*	inArgv[])
	{
		if(inArgC != 2)
		{
			return false;
		}

		if(strcmp(inArgv[1], "on") == 0)
		{
			SetTransformerState(true);
			lightsOn = true;
			mode = eMode_TestPattern;
			return true;
		}
		else if(strcmp(inArgv[1], "off") == 0)
		{
			if(timeOfDay == eTimeOfDay_Day)
			{
				SetTransformerState(false);
			}
			lightsOn = false;
			mode = eMode_Normal;
			return true;
		}

		return false;
	}

	bool
	SetColor(
		int			inArgC,
		char const*	inArgv[])
	{
		if(inArgC != 4)
		{
			return false;
		}

		defaultColor.r = atof(inArgv[1]);
		defaultColor.g = atof(inArgv[2]);
		defaultColor.b = atof(inArgv[3]);

		WriteDataToEEPROM(&defaultColor, eepromOffset, sizeof(defaultColor));

		return true;
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

		DebugMsg(eDbgLevel_Basic, "Transformer state to %d\n", inState);

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
			leds.show();

			// Let led update finish
			gRealTime->RegisterEvent("XfmrWarmup", eLEDUpdateMS * 1000, true, this, static_cast<TRealTimeEventMethod>(&COutdoorLightingModule::TransformerTransitionEvent), NULL);
		}
	}

	void
	TransformerTransitionEvent(
		char const*	inName,
		void*		inRef)
	{
		DebugMsg(eDbgLevel_Basic, "Transformer transition %d\n", curTransformerTransitionState);
		curTransformerState = curTransformerTransitionState;
		digitalWriteFast(eTransformRelayPin, curTransformerState);
	}

	OctoWS2811	leds;
	IRealTimeDataProvider*	ds3234Provider;
	
	uint8_t		mode;
	uint8_t		timeOfDay;
	bool		lightsOn;
	bool		motionSensorTrip;
	bool		curTransformerState;
	bool		curTransformerTransitionState;

	SFloatPixel		frameBuffer[eLEDCount];
	CBasePattern*	basePattern;

	int			toggleCount;
	uint64_t	toggleLastTimeMS;
	uint64_t	cyclePatternTimeMS;
	int			cyclePatternCount;
	float		testPatternValue;

	SFloatPixel	defaultColor;

	TEpochTime	duskStartTime;

	static COutdoorLightingModule	gOutdoorLightingModule;

};

COutdoorLightingModule	COutdoorLightingModule::gOutdoorLightingModule;
