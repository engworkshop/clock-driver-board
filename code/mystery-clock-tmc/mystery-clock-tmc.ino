/*
	mystery-clock-tmc.ino:  TMC2208 Motor Driver for Mystery Clock

	Copyright 2024 by John Eng

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License version 3 (GPLv3) as
	published by the Free Software Foundation.

	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	or FITNESS FOR A PARTICULAR PURPOSE.  For more details, see
	https://www.gnu.org/licenses/gpl-3.0.en.html
	--------------------

	REVISION HISTORY:
	Oct 2024   Initial version, based on hollow-clock.ino

	HARDWARE COMPONENTS:
	Waveshare RP2040 Zero - similar to Raspberry Pi Pico
	Maxim DS3234 realtime clock
	Watterott SilentStepStick clone, based on Trinamic TMC2208
	Elegoo 28BYJ-48 stepper motor, modified for bipolar operation
		32 steps per revolution of ungeared motor
		9:32, 11:22, 9:27, 8:24 = exact 1:64 gear reduction
		therefore, 2048 steps per revolution
		verified by disassembly of gearbox and inspection of gears
	Gearing based on Hollow Clock 4 design by Shiura
		https://www.thingiverse.com/thing:5636482
		1 rotation of motor shaft turns minute gear 12 steps
		minute gear has 90 teeth
		therefore, motor must turn 90/12 = 7.5 rotations per hour
			or 90/12/60 = 1/8 rotation per minute
			or (1/8)*2048 = 256 steps per minute

	SOFTWARE REQUIREMENTS:  Arduino IDE with Earle Philhower's Arduino core
	for the Raspberry Pi Pico RP2040 processor installed, which is documented
	at

		https://github.com/earlephilhower/arduino-pico
		https://arduino-pico.readthedocs.io/en/latest

	Be sure to check and select correct serial port whenever connecting
	controller to computer, even if correct board identity is shown.

	SparkFun DS3234 RTC Arduino library installed.  The SparkFun library is
	old and must be installed from .zip file (Arduino > Sketch < Include
	Library > Add .ZIP Library).  The SparkFun library can be found at

		https://github.com/sparkfun/SparkFun_DS3234_RTC_Arduino_Library.

	PROGRAMMING NOTES
	1.	Arduino has a stepper motor library, but you cannot turn motor power
		on/off.  So, this code implements its own stepper motor functions.
		Otherwise, the motor gets pretty hot under normal conditions.
	2.	The SparkFun RTC library has some convenient features, but it is too
		simplistic for serious use.  It does not provide an explicit method
		for enabling only one of the RTC interrupts, nor does it provide a
		function for clearing the alarm/interrupt flag explicitly.  These
		deficiencies result in undesirable behavior when trying to use
		interrupts.  I had to add functions to provide more explicit control,
		essentially hijacking the SPI bus from the SparkFun library, meaning
		that I call SPI library functions without explicitly initializing SPI
		by calling SPI.begin().  SPI.begin() is called within the SparkFun RTC
		library.
	3.	Earle Philhower's Arduino core includes additional SPI functions that
		allow the SPI pins to be specified explicitly before SPI.begin() is
		called by third party libraries.  Smart!
	4.	When using Arduino hardware, disabling interrupts with noInterrupts()
		also disables motor stepping because motor stepping relies on delay(),
		and delay() requires interrupts to be enabled.  I assumed this is also
		true of the RP2040, but I didn't test it.

	OPERATION:  Press and hold the CW or CCW button to move the hands.  Press
	release to move the minute hand by one minute.  The clock's minute counter
	is synchronized to the last button press.

	If a button is pressed during startup, the TMC2208's reference voltage and
	corresponding motor current will be shown in the Arduino console.  The
	motor requires about a 220 mA current.  The reference voltage, and thus the
	motor current, are controlled by a small potentiometer on the
	SilentStepStick.  Press a button again to stop the console output and
	resume normal operation.

	To do:
	implement 2-speed motor switching
*/

///////////////////////////////////////////////////////////////////////////////
//
//   LIBRARIES, CONSTANTS, AND VARIABLES
//
///////////////////////////////////////////////////////////////////////////////

// Library headers
#include <SPI.h>
#include <SparkFunDS3234RTC.h>

// Constants for controller board pins
const int ENABLE_PIN   = 7;   // Motor driver: enable, active LOW
const int MSELECT1_PIN = 8;   // Motor driver: microstep configuration
const int MSELECT2_PIN = 9;   // Motor driver: microstep configuration
const int STEP_PIN     = 10;  // Motor driver: step motor, active on rising edge
const int DIR_PIN      = 11;  // Motor driver: motor direction
const int SCK_PIN      = 2;   // RTC: serial clock
const int PICO_PIN     = 3;   // RTC: peripheral in, controller out
const int POCI_PIN     = 4;   // RTC: peripheral out, controller in
const int RTC_CS_PIN   = 5;   // RTC: chip select (-CS)
const int RTC_INT_PIN  = 6;   // RTC: interrupt output(-INT/SQW)
const int CW_BTN_PIN   = 12;  // Button for moving hands counterclockwise; connected to GND
const int CCW_BTN_PIN  = 13;  // Button for moving hands clockwise; connected to GND
const int VREF_PIN     = 26;  // Motor driver: reference voltage

// Timing constants
const unsigned long POWER_DELAY     = 300;   // Wait for power up or down (ms)
const unsigned long DEBOUNCE_DELAY  = 10;    // Button debounce delay (ms)

// Motor constants
const int STEPS_PER_REVOLUTION = 32*64;               // Number of motor steps to turn geared shaft one revolution
const int STEPS_PER_MINUTE = STEPS_PER_REVOLUTION/8;  // Number of motor steps to move minute hand 1 minute
const int DIR_FORWARD = HIGH;                         // Determines which DIR_PIN level is considered "forward"
const int DIR_BACKWARD = LOW;                         // Determines which DIR_PIN level is considered "backward"

// Constants and variables for button state tracking
const int N_BUTTONS = 2;  // Number of connected button pins
int gBtnPin[] = {         // Table of button pin numbers, corresponding to gLastBtnState[] and gLastBtnTimestamp[]
	CW_BTN_PIN,
	CCW_BTN_PIN
	};
int gLastBtnState[N_BUTTONS];                // Last known state (LOW or HIGH) of button, maintained by readButtonByIndex()
unsigned long gLastBtnTimestamp[N_BUTTONS];  // Last time button was noted to change state
const int BUTTON_NONE  = -1;                 // "Null" button "pin" returned by button subroutines

// Constants and variables for reading of TMC2208 reference voltage
const int ADC_BUFFER_SIZE = 200;    // Size of circular buffer for storing ADC readings
int gAdcReadings[ADC_BUFFER_SIZE];  // Circular buffer for storing ADC readings
int gReadingsPtr = 0;               // Pointer to position of next ADC reading in circular buffer
long gReadingsSum = 0;              // Summation of ADC readings for averaging


// Table for microstep configuration
int gMicrostepConfig[][4] = {
	{8,  LOW,  LOW,  2000},  // microsteps, MSELECT1, MSELECT2, motor step time
	{2,  HIGH, LOW,  3000},
	{4,  LOW,  HIGH, 3000},
	{16, HIGH, HIGH, 2000}
	};

// Global variables
int gMicrosteps;                          // Number of microsteps per full step, initialized in setup()
unsigned int gMotorStepDelay;             // Delay between microsteps (microseconds), initialized in setup()
int gMotorDirection;                      // Current motor direction, initialized in setup()
int gNextAlarm = 59;                    // Second of the minute to generate alarm interrupt
volatile boolean gMoveMotorFlag = false;  // Set by interrupt service routine to signal need to move motor

///////////////////////////////////////////////////////////////////////////////
//
//   STANDARD ARDUINO IDE METHODS
//
///////////////////////////////////////////////////////////////////////////////

// Arduino initialization
void setup() {
	// Configure signal pins
	pinMode(ENABLE_PIN,   OUTPUT);
	pinMode(MSELECT1_PIN, OUTPUT);
	pinMode(MSELECT2_PIN, OUTPUT);
	pinMode(STEP_PIN,     OUTPUT);
	pinMode(DIR_PIN,      OUTPUT);
	pinMode(RTC_CS_PIN,   OUTPUT);
	pinMode(RTC_INT_PIN,  INPUT_PULLUP);
	pinMode(CW_BTN_PIN,   INPUT_PULLUP);
	pinMode(CCW_BTN_PIN,  INPUT_PULLUP);
	// Initialize digital outputs
	digitalWrite(ENABLE_PIN,   HIGH);
	digitalWrite(STEP_PIN,     LOW);
	digitalWrite(DIR_PIN,      DIR_FORWARD);
	digitalWrite(RTC_CS_PIN,   HIGH);
	gMotorDirection = DIR_FORWARD;
	setMicrosteps(8);  // Initialize gMicrosteps, gMotorStepDelay, MSELECT1_PIN, and MSELECT2_PIN
	// Initialize button state arrays
	for (int i = 0; i < N_BUTTONS; i++) {
		gLastBtnState[i] = HIGH;
		gLastBtnTimestamp[i] = millis();
		}
	delay(DEBOUNCE_DELAY*2);  // Debounce delay required before immediate first use!
	// If a button is pressed at startup, show TMC2208 reference voltage on console
	if (readButtons() != BUTTON_NONE) {
		showRefVoltage();  // Returns after a button is pressed again
		}
	// Initialize motor position
	motorPower(true);
	motorPower(false);
	// Initialize RTC chip
	rtc.begin(RTC_CS_PIN);      // Initialize SparkFun RTC library
	initTime();                 // Set RTC to a dummy time and date
	rtc.setAlarm1(gNextAlarm);  // Set alarm for specific second of a minute
	configureInterrupts();      // Configure RTC chip for interrupt generation
	// Set up interrupt service routine
	attachInterrupt(digitalPinToInterrupt(RTC_INT_PIN), handleInterrupt, FALLING);
	}

// Arduino main execution loop
void loop() {
	// Check if motor needs to be moved
	if (gMoveMotorFlag) {  // Flag set in handleInterrupt()
		motorPower(true);
		motorStep(STEPS_PER_MINUTE);
		motorPower(false);
		gMoveMotorFlag = false;
		}
	// Handle button presses
	checkButtons();
	}

///////////////////////////////////////////////////////////////////////////////
//
//   MAIN SUBROUTINES
//
///////////////////////////////////////////////////////////////////////////////

// Manually move motor with CW and CCW buttons
void checkButtons() {
	switch (readButtons()) {
		case CW_BTN_PIN:
			handleButton(CW_BTN_PIN);
			break;
		case CCW_BTN_PIN:
			handleButton(CCW_BTN_PIN);
			break;
		}
	}

// Handle button press and release
void handleButton(int btnPin) {
	// Turn on motor
	motorPower(true, false);
	// Move motor continuously if button held down
	unsigned long t0 = millis();
	while (readButton(btnPin) == LOW) {
		if (millisSince(t0) >= POWER_DELAY) {
			motorStepOne((btnPin == CW_BTN_PIN) ? 1 : -1);
			delayMicroseconds(gMotorStepDelay);
			}
		}
	// Move motor one minute if button pressed and released
	unsigned long t1 = millisSince(t0);
	if (t1 < POWER_DELAY) {
		delay(POWER_DELAY - t1);
		motorStep((btnPin == CW_BTN_PIN) ? STEPS_PER_MINUTE : -STEPS_PER_MINUTE);
		}
	// Reset time register
	initTime();
	// Turn off motor
	motorPower(false);
	}

// Interrupt service routine
void handleInterrupt() {
	clearAlarmFlags();
	gMoveMotorFlag = true;  // Indicate that motor needs to be moved
	}

///////////////////////////////////////////////////////////////////////////////
//
//   SUBROUTINES FOR READING TMC2208 REFERENCE VOLTAGE
//
///////////////////////////////////////////////////////////////////////////////

// Show TMC2208 reference voltage on console
void showRefVoltage() {
	// Initialize serial port
	Serial.begin();  // USB port requires no baud rate
	while (!Serial) {}
	// Initialize buffer of ADC readings
	for (int i = 0; i < ADC_BUFFER_SIZE; i++) {
		delay(2);
		gAdcReadings[i] = analogRead(VREF_PIN);
		gReadingsSum += long(gAdcReadings[i]);
		}
	// Wait until button released
	while (readButtons() != BUTTON_NONE) {}
	// Voltage polling loop
	do {
		int vref = updateMillivolts(VREF_PIN);
		float motorCurrent = (325.0*float(vref))/(140.0*sqrt(2.0)*2500.0);  // Formula from TMC2208 data sheet
		motorCurrent *= 1000.0;
		Serial.print(vref);
		Serial.print(" mV   ");
		Serial.print(int(motorCurrent + 0.5));
		Serial.println(" mA");
		}
	while (readButtons() == BUTTON_NONE);
	while (readButtons() != BUTTON_NONE) {}
	}

// Read one ADC value, update running average of voltage
int updateMillivolts(int adcPin) {
	// Subtract old reading from summation
	gReadingsSum -= long(gAdcReadings[gReadingsPtr]);
	// Store new reading
	delay(2);
	gAdcReadings[gReadingsPtr] = analogRead(adcPin);
	// Add new reading to summation
	gReadingsSum += gAdcReadings[gReadingsPtr];
	// Update circular buffer pointer
	gReadingsPtr++;
	if (gReadingsPtr >= ADC_BUFFER_SIZE) gReadingsPtr = 0;
	// Calculate voltage in millivolts
	float v = float(gReadingsSum)/float(ADC_BUFFER_SIZE);
	v = 3300.0*v/1024.0;
	return (int(v + 0.5));
	}

///////////////////////////////////////////////////////////////////////////////
//
//   RTC SUBROUTINES
//
///////////////////////////////////////////////////////////////////////////////

// Set RTC to a dummy time
void initTime() {
	rtc.setTime(0, 0, 0, 7, 25, 11, 23);  // Set to midnight of Saturday, November 25, 2023
	clearAlarmFlags();
	gMoveMotorFlag = false;  // In case interrupt occurred prior to calling this method
	}

// Enable alarm 1 and disable alarm 2 interrupts
void configureInterrupts() {
	byte registerValue = readRtc(0x0E);
	writeRtc(0x8E, (registerValue & (~0x02)) | 0x01);
	}

// Clear both RTC alarm flags
void clearAlarmFlags() {
	byte registerValue = readRtc(0x0F);
	writeRtc(0x8F, registerValue & (~0x03));
	}

// Read value from RTC register
byte readRtc(byte address) {
	digitalWrite(RTC_CS_PIN, LOW);
	SPI.transfer(address);
	byte r = SPI.transfer(0x00);
	digitalWrite(RTC_CS_PIN, HIGH);
	return (r);
	}

// Write value to RTC register
void writeRtc(byte address, byte value) {
	digitalWrite(RTC_CS_PIN, LOW);
	SPI.transfer(address);
	SPI.transfer(value);
	digitalWrite(RTC_CS_PIN, HIGH);
	}

///////////////////////////////////////////////////////////////////////////////
//
//   MOTOR SUBROUTINES
//
///////////////////////////////////////////////////////////////////////////////

// Set number of microsteps
void setMicrosteps(int microsteps) {
	// Determine size of configuration array
	int len = sizeof(gMicrostepConfig)/sizeof(gMicrostepConfig[0]);
	// Scan configuration array for matching number of microsteps
	for (int i = 0; i < len; i++) {
		if (microsteps == gMicrostepConfig[i][0]) {
			gMicrosteps = microsteps;
			digitalWrite(MSELECT1_PIN, gMicrostepConfig[i][1]);
			digitalWrite(MSELECT2_PIN, gMicrostepConfig[i][2]);
			gMotorStepDelay = gMicrostepConfig[i][3];
			return;
			}
		}
	// Do nothing if specified microsteps is invalid
	}

// Move motor given number of steps
void motorStep(long n) {
	if (n == 0) return;
	long nAbs = abs(n);
	for (long i = 0; i < nAbs; i++) {
		motorStepOne(n);
		delayMicroseconds(gMotorStepDelay);
		}
	}

// Move motor one step in given direction
void motorStepOne(long direction) {
	// Set motor direction
	motorDirection(direction);
	// Step motor the number of microsteps equivalent to a full step
	for (int i = 0; i < gMicrosteps; i++) {
		digitalWrite(STEP_PIN, HIGH);
		digitalWrite(STEP_PIN, LOW);  // Pulse width measured to be 600 ns with Waveshare RP2040 Zero
		delayMicroseconds(gMotorStepDelay);
		}
	}

// Turn motor coils on/off with delay to let things settle down
void motorPower(boolean turnOn) {
	motorPower(turnOn, true);
	}

// Turn motor power on/off using ENABLE pin
void motorPower(boolean turnOn, boolean delayFlag) {
	if (turnOn) {
		digitalWrite(ENABLE_PIN, LOW);
		if (delayFlag) delay(POWER_DELAY);
		}
	else {
		if (delayFlag) delay(POWER_DELAY);
		digitalWrite(ENABLE_PIN, HIGH);
		}
	}

// Set motor direction while preventing redundant calls to digitalWrite()
void motorDirection(long direction) {
	if (direction >= 0) {
		if (gMotorDirection != DIR_FORWARD) {
			digitalWrite(DIR_PIN, DIR_FORWARD);
			gMotorDirection = DIR_FORWARD;
			}
		}
	else {
		if (gMotorDirection != DIR_BACKWARD) {
			digitalWrite(DIR_PIN, DIR_BACKWARD);
			gMotorDirection = DIR_BACKWARD;
			}
		}
	}

///////////////////////////////////////////////////////////////////////////////
//
//   BUTTON SUBROUTINES
//
///////////////////////////////////////////////////////////////////////////////

// Read state of all button switches, return identifier of button pressed
int readButtons() {
	for (int i = 0; i < N_BUTTONS; i++) {
		if (readButtonByIndex(i) == LOW) return (gBtnPin[i]);
		}
	return (BUTTON_NONE);
	}

// Read state of one button switch given button pin number (active LOW)
int readButton(int btnPin) {
	int btnIndex = pinToIndex(btnPin);
	return (readButtonByIndex(btnIndex));
	}

// Read state of one button switch with debouncing (active LOW), given index to button arrays
int readButtonByIndex(int btnIndex) {
	// Don't read button during debounce period
	if (millisSince(gLastBtnTimestamp[btnIndex]) < DEBOUNCE_DELAY) {
		return (gLastBtnState[btnIndex]);
		}
	// Read current button state
	int newBtnState = digitalRead(gBtnPin[btnIndex]);
	// If button state has changed, save new state along with a timestamp
	if (newBtnState != gLastBtnState[btnIndex]) {
		gLastBtnState[btnIndex] = newBtnState;
		gLastBtnTimestamp[btnIndex] = millis();
		}
	// Return LOW if button pressed, HIGH if not pressed
	return (newBtnState);
	}

// Convert button pin number to array index
int pinToIndex(int btnPin) {
	for (int i = 0; i < N_BUTTONS; i++) {
		if (btnPin == gBtnPin[i]) return (i);
		}
	return (0);  // Default to 0 for safety to avoid out-of-bounds index
	}

///////////////////////////////////////////////////////////////////////////////
//
//   UTILITY SUBROUTINES
//
///////////////////////////////////////////////////////////////////////////////

// Calculate milliseconds since last activity (modified from adb-blue-kbd.ino)
// Correct even when timer rolls over as long as all arithmetic is unsigned
unsigned long millisSince(unsigned long baseTime) {
	unsigned long d = millis() - baseTime;
	return (d);
	}
