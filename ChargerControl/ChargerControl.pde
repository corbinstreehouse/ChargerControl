//
//  ChargerControl.pde
//  Sketch 
//  ----------------------------------
//  Developed with embedXcode
//
//  Project ChargerControl
//  Created by corbin dunn on 5/18/12 
//  Copyright (c) 2012 __MyCompanyName__
//

// Core library
#include  "Arduino.h" // — for Arduino 1.0

#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>

#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include <EEPROM.h>
#include <avr/eeprom.h>

#include <Time.h>  

#include <TimeAlarms.h>

#define DEBUG 1

#include "CrbMenu.h"


#define LCD_COLUMNS 16
#define LDC_ROWS 2

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield g_lcd = Adafruit_RGBLCDShield();
CrbMenu g_menu;

// The menu item structure
/*
    Initial screen values (Root menu item):
    -------
    1234567890123456
        1234567890123456
        
    -------
 
    Charging Enabled
    <blank>

    Charging Enabled
    <blank>
 
    Timer Set
    Start - 01:10 AM
 
    Timed Charging
    End - 05:50 AM

 
    
    -------
    Charging Mode <right arrow>
     [ Normal charging ]
        Normal charging
        Timed charging
    Settings <right arrow>
     >
        Set Start time
            01:10 AM
        Set Timer Duration
            05:05 (hrs:mins)
        Set Clock
            01:10 AM
        
Enter to edit
Update/down to change value
Left/right to move to the next
Enter to accept
 
*/

// enums take 2 bytes, which sucks, so I typedef it to a uint8_t
enum _ChargingMode {
    ChargingModeNormal = 0,
    ChargingModeTimed = 1,
};
typedef uint8_t ChargingMode;

enum _ChargingState {
    ChargingStateOff = 0, // Not doing anything
    ChargingStateWantToCharge = 1, // The timer fired, or we are set to charge, and we are just waiting for the Proximity signal to be right and the signal to be present
    ChargingStateCharging = 2, // Charging! We will actively look for the promity state to change, and stop charging when it changes. If it changes, we go to "stops" because we are being unplugged
    ChargingStateDoneCharging = 3, // Charging is done; we should turn off after a delay
};
typedef uint8_t ChargingState;


#pragma mark -
#pragma mark Globals
// UI
CrbMenuItem *g_rootItem;

// Charging mode and state
ChargingMode g_chargingMode = ChargingModeNormal;
time_t g_startTime = 0;
time_t g_duration = 0;
ChargingState g_chargingState = ChargingStateOff;


#pragma mark -
#pragma mark Defines

// EEPROM locations we read/write
#define EE_CHARGING_MODE_LOCATION 0
#define EE_CHARGING_DURATION_LOCATION 1 // Takes 4 bytes!
#define EE_CHARGING_START_TIME 5 // Takes 4 bytes!
#define EE_UNUSED 9 // free location

#define PIN_BMS_POWER 4 // Turns on the BMS when HIGH
#define PIN_EVSE_PILOT 5 // Turns on the pilot signal when HIGH
#define PIN_CHARGER_OFF 6  // Turns off the charger when HIGH (by sending 5v to the manzanita micro's pin 2). A safety feature is the 5v signal from pin 1 of the charger is looped to the "normally closed" relay controlled by the BMS "high limit". The BMS opens the circuit to let the charger charge.
#define PIN_ARDUINO_OFF 7
#define PIN_EVSE_PROXIMITY A0

// aliases to make the code more readable when setting values for the pins
#define BMS_MODE_TURNED_ON HIGH
#define BMS_MODE_TURNED_OFF LOW

#define PILOT_SIGNAL_MODE_ON HIGH
#define PILOT_SIGNAL_MODE_OFF LOW

#define CHARGER_MODE_ON LOW // Low signal cuts the 5v, letting it go on
#define CHARGER_MODE_OFF HIGH

#define ARDUINO_MODE_ALLOW_ON LOW
#define CHARGER_MODE_TURN_OFF HIGH


enum ProximityMode {
    ProximityModeUnknown = 0, // don't charge; something's wrong
    ProximityModeUnplugged = 1, // don't charge
    ProximityModePluggedButNotLatched = 2,  // don't charge, or stop charging if we see this
    ProximityModePluggedAndLatched = 3, // Okay to charge
};

/*
 Proximity Detection:
 1. Analog read from PIN_EVSE_PROXIMITY
 When using 2.7k resistor:
 if : ~4.3v (value=906), not connected  (stop charging)
 if: ~2.7v (value=568), connected but not latched (stop charging, if charging)
 if: ~1.55v (value=327), connected and latched (okay to charge)
 When using a 10k resistor:
 if : ~4.7v (value=991), not connected  (stop charging)
 if: ~2.85v (value=601), connected but not latched (stop charging, if charging)
 if: ~1.60v (value=337), connected and latched (okay to charge)
 
 */

static ProximityMode readProximityMode() {
    int proximityValue = digitalRead(PIN_EVSE_PROXIMITY);
    if (proximityValue > 900) {
        return ProximityModeUnplugged;
    }
    if (proximityValue > 500) {
        return ProximityModePluggedButNotLatched;
    }
    if (proximityValue > 300) {
        return ProximityModePluggedAndLatched;
    }
    return ProximityModeUnknown;
}


// TODO: use a pin wired to the HLIM of the BMS to find out when the BMS has reached the "High limit" so the Arduino can auto-turn off the EVSE (and itself) after a set time for the cells to balance.

static inline void setInitialPinStates() {
    // Setup our initial state
    // Pins default as INPUT, but we want to make them output
    
    pinMode(PIN_BMS_POWER, OUTPUT);  
    digitalWrite(PIN_BMS_POWER, BMS_MODE_TURNED_OFF);
    
    pinMode(PIN_EVSE_PILOT, OUTPUT);
    digitalWrite(PIN_EVSE_PILOT, PILOT_SIGNAL_MODE_OFF);
    
    pinMode(PIN_CHARGER_OFF, OUTPUT);
    digitalWrite(PIN_CHARGER_OFF, CHARGER_MODE_OFF); // Charger off is a HIGH signal
    
    pinMode(PIN_ARDUINO_OFF, OUTPUT);
    digitalWrite(PIN_ARDUINO_OFF, ARDUINO_MODE_ALLOW_ON); // set to HIGH to turn off the arduino (and everything!)
    
//    pinMode(PIN_EVSE_PROXIMITY, INPUT); // The default value is input, don't bother setting it
}

void ChargingModeChangedAction(CrbActionMenuItem *sender) {
    // TODO: move this into the menu class, if it makes more sense there and i need it again
    if (!sender->hasOption(CrbMenuItemOptionSelected)) {
        // Update the global and write it out
        g_chargingMode = sender->getTag();
        EEPROM.write(EE_CHARGING_MODE_LOCATION, g_chargingMode);
        
        sender->addOption(CrbMenuItemOptionSelected);
        // remove the option from the other menu items
        CrbMenuItem *parent = sender->getParent();
        CrbMenuItem *walker = parent->getChild();
        while (walker) {
            if (walker != sender) {
                walker->removeOption(CrbMenuItemOptionSelected);
            }
            walker = walker->getNext();
        }
        g_menu.print(); // Update to show the change
    }
}

static inline void defaultsWriteTime(int location, time_t time) {
    // time_t is an unsigned long; 32-bits
    eeprom_write_dword((uint32_t *)location, time);
}

static inline time_t defaultsReadTime(int location) {
    return eeprom_read_dword((uint32_t *)location);
}

static void ChargingSaveStartTimeAction(CrbTimeSetMenuItem *sender) {
    if (g_startTime != sender->getTime()) {
        g_startTime = sender->getTime();
        defaultsWriteTime(EE_CHARGING_START_TIME, g_startTime);
    }
}

static void ChargingSaveDurationAction(CrbTimeSetMenuItem *sender) {
    if (g_duration != sender->getTime()) {
        g_duration = sender->getTime();
        defaultsWriteTime(EE_CHARGING_DURATION_LOCATION, g_duration);
    }
}


//static void ChargingSaveDateAction(CrbTimeSetMenuItem *sender) {
//    
//}

static inline void loadSettings() {
    // Restore the previous values
    g_chargingMode = EEPROM.read(EE_CHARGING_MODE_LOCATION);
    
    // Validate values
    if (g_chargingMode > ChargingModeTimed) {
        g_chargingMode = ChargingModeNormal;
    }
    
    g_startTime = defaultsReadTime(EE_CHARGING_START_TIME);
    g_duration = defaultsReadTime(EE_CHARGING_DURATION_LOCATION);
}


static void ChargingModeEnter(CrbActionMenuItem *) {
    int value = digitalRead(PIN_BMS_POWER) == HIGH ? LOW : HIGH;
    digitalWrite(PIN_BMS_POWER, value);
}


static inline void setupMenu() {
    g_lcd.begin(LCD_COLUMNS, LDC_ROWS);
    g_lcd.clear();
    
    g_rootItem = new CrbActionMenuItem("Charging Enabled", (CrbMenuItemAction)ChargingModeEnter, 0l);
    CrbMenuItem *chargingModeItem = new CrbMenuItem("Charging Mode >");
    CrbMenuItem *itemNormalChargingMode = new CrbActionMenuItem("Normal charging", (CrbMenuItemAction) ChargingModeChangedAction, ChargingModeNormal);
    CrbMenuItem *itemTimedChargingMode = new CrbActionMenuItem("Timed charging", (CrbMenuItemAction)ChargingModeChangedAction, ChargingModeTimed);
    
    g_rootItem->addChild(chargingModeItem);
    chargingModeItem->addChild(itemNormalChargingMode);
    chargingModeItem->addChild(itemTimedChargingMode);
    
    if (g_chargingMode == ChargingModeNormal) {
        itemNormalChargingMode->addOption(CrbMenuItemOptionSelected);
    } else { // timed
        itemTimedChargingMode->addOption(CrbMenuItemOptionSelected);
    }
    
    // validate duration or the start time? 
    
    CrbMenuItem *itemSettings = new CrbMenuItem("Settings >");
    g_rootItem->addChild(itemSettings);
    
    // When this action 
    CrbMenuItem *itemSetStartTime = new CrbMenuItem("Set start time >");
    itemSettings->addChild(itemSetStartTime);
    itemSetStartTime->addChild(new CrbTimeSetMenuItem("Start time", (CrbMenuItemAction)ChargingSaveStartTimeAction, g_startTime));

    CrbMenuItem *itemTimerDuration = new CrbMenuItem("Set timer duration >");
    itemSettings->addChild(itemTimerDuration);
    itemTimerDuration->addChild(new CrbDurationMenuItem("Duration", (CrbMenuItemAction)ChargingSaveDurationAction, g_duration));
    
    
    // TODO: how to initialize these variables...so it is showing the current time/date when the menu is shown?
//    CrbMenuItem *itemSetDate = new CrbMenuItem("Set current date >");
//    itemSettings->addChild(itemSetDate);
//    itemSetDate->addChild(new CrbTimeSetMenuItem("Set the date", (CrbMenuItemAction)ChargingSaveDateAction, 0));
//
//    CrbMenuItem *itemSetTime = new CrbMenuItem("Set current time >");
//    itemSettings->addChild(itemSetTime);
//    itemSetDate->addChild(new CrbTimeSetMenuItem("Set the time", (CrbMenuItemAction)ChargingSaveDateAction, 0));
    
    
    
    g_menu.init(&g_lcd, g_rootItem);
    g_menu.print();
}

static inline void setupTime() { 
    setSyncProvider(RTC.get);   // the function to get the time from the RTC
#if DEBUG
    if (timeStatus() != timeSet) 
        Serial.println("Unable to sync with the RTC");
    else
        Serial.println("RTC has set the system time");      
#endif
}


// Turns on or off the BMS. mode==HIGH to on and mode==LOW to off
static inline void setBMSToMode(uint8_t mode) {
#if DEBUG
    Serial.print("Turning BMS");
    if (mode == BMS_MODE_TURNED_ON) {
        Serial.println("on");
    } else {
        Serial.println("off");
        // If turning off, make sure the charger is already off first!
        if (digitalRead(PIN_CHARGER_OFF) != HIGH) {
            Serial.println("WARNING: THE CHARGER SHOULD HAVE BEEN SIGNALED OFF BEFORE THE BMS IS TURNED OFF!");
        }
    }
#endif
    if (digitalRead(PIN_BMS_POWER) != mode) {
        digitalWrite(PIN_BMS_POWER, mode);
        // Give it a brief moment to kick on.
        Alarm.delay(200); // TODO make sure 200ms is enough time to let it start
    }
}

static inline void setChargerToMode(uint8_t mode) {
    // Just set the pin; we don't need to wait for anything to happen
    digitalWrite(PIN_CHARGER_OFF, mode);
}

static inline void setPilotSignalToMode(uint8_t mode) {
    digitalWrite(PIN_EVSE_PILOT, mode);
    // We don't wait; the charger won't go on until it has power
}

static void enableTimer(bool enabled) {
#warning implement
}

//enableTimer(false);
//enableCharging(true);

void setup() {
#if DEBUG
    Serial.begin(9600);
    Serial.println("setup"); 
#endif
    // Order is pretty important!
    setInitialPinStates();
    loadSettings();
    setupMenu();
    setupTime();

    // Kick us into the initial state of wanting to charge, and loop waiting for the promity signal to be right. 
    if (g_chargingMode == ChargingModeNormal) {
        g_chargingState = ChargingStateWantToCharge;
    }
}

static bool canCharge() {
    // Ideally we should look for the 12v pilot signal. I don't have anything that reads the signal. It would be interesting to see if the current passing through the signal is less than 40mA. If it is, the arduino pin can be set as an OUTPUT and sink the current (I think this means take it to ground). notes http://arduino.cc/en/Tutorial/DigitalPins
    // Instead, see if we can charge by looking for the proximity signal to be correct. 
    ProximityMode proximityMode = readProximityMode();
    return proximityMode == ProximityModePluggedAndLatched;
}

static void startCharging() {
    // Move our state to charging; from now on in the loop we will look for the proximity switch to be turned off to "soft stop" charging
    g_chargingState = ChargingStateCharging;

    // Make sure the BMS is on before we turn on the charger
    setBMSToMode(BMS_MODE_TURNED_ON);
    // Then tell the EVSE to give us power
    setPilotSignalToMode(PILOT_SIGNAL_MODE_ON);
    // At this point, the EVSE will open its relay and give the charger power
    // Stop signaling the charger to not charge (turn off the signal we send it). This is doubly controlled by the relay hooked up to the BMS.
    setChargerToMode(CHARGER_MODE_ON);

    // We will then be charging!
}

static bool timedChargingShouldBeEnabled() {
    // TODO: this
    return false; // This should see if the current time falls within the normal charge time, and if so, allow charging
#warning finish
}

static void stopCharging() {
    // Stop charging, because we are going to be unplugged
    // We "soft stop" the charger by sending it 5v; this kills it right away, and makes it stop drawing amps
    setChargerToMode(CHARGER_MODE_OFF);
    // Wait a bit, and then signal the EVSE that we are off
    Alarm.delay(200); // TODO make sure 200ms is enough time for the charger to stop; we need to make this as short as possible, since we might be being unplugged and prefer to turn off the pilot signal ourselves
    // Turn off the pilot signal; this cuts the power to the charger
    setPilotSignalToMode(PILOT_SIGNAL_MODE_OFF);
    Alarm.delay(500); // TODO make sure 500ms (half a second) is long enough to wait before we turn off the BMS
    // Turn off the BMS
    setBMSToMode(BMS_MODE_TURNED_ON);
    
    // Go back to "wantToCharge"
    if (g_chargingMode == ChargingModeNormal) {
        g_chargingState = ChargingStateWantToCharge;
    } else if (g_chargingMode == ChargingModeTimed) {
        if (timedChargingShouldBeEnabled()) {
            g_chargingState = ChargingStateWantToCharge;
        } else {
            // go to off, and wait for the timer to be fired
            // TODO: check to see if we are past the timer...if we are, set a delay to turn off the arduino
            g_chargingState = ChargingStateOff;
        }
    } else {
        g_chargingState = ChargingStateOff;
#if DEBUG
        Serial.println("stopCharging: Unhandled charging mode!");
#endif
    }
}

void loop() {
    g_menu.loopOnce();
    
    switch (g_chargingState) {
        case ChargingStateOff:
            // TODO: when in debug mode, validate things are off?
            break;
        // If we are "wanting to charge" because the timer fired, we loop until we can charge, and then actually start
        case ChargingStateWantToCharge:
            if (canCharge()) {
                startCharging();
            }
            break;
        case ChargingStateCharging:
            if (!canCharge()) {
                stopCharging();
            }
            break;
        case ChargingStateDoneCharging:
            // We don't do anyting; we are going to turn the arduino off after a delay

            break;
    }
}






