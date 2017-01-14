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

	This is the main ino for a front of house lighting project
*/

#include <EEPROM.h>
#include <FlexCAN.h>
#include <OctoWS2811.h>
#include <i2c_t3.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include <ELAssert.h>
#include <ELModule.h>
#include <ELInternet.h>
#include <ELRealTime.h>
#include <ELCommand.h>
#include <ELUtilities.h>
#include <ELSunRiseAndSet.h>
#include <ELDigitalIO.h>
#include <ELLuminositySensor.h>

void
SetupFHOutdoorLighting(
	void);

void
setup(
	void)
{
	Serial.begin(115200);

	//WaitForSerialPort();

	CModule_SerialCmdHandler::Include();
	CModule_SysMsgCmdHandler::Include();

	SetupFHOutdoorLighting();

	CModule::SetupAll("v0.2", true);
}

void
loop(
	void)
{
	CModule::LoopAll();
}
