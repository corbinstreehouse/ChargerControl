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

#include <TimeAlarms.h> // NOTE: not using; I could remove this 

#include <stdint.h> // We can compile without this, but it kills xcode completion without it! it took me a while to discover that..

#include "CrbMenu.h"

#define DEBUG 0
#define DEBUG_ERRORS 1 // Prints errors (you want this on until you are really sure things are right)

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
    ChargingStateCharging = 1, // Charging! We will actively look for the promity state to change, and stop charging when it changes. If it changes, we go to "stops" because we are being unplugged
    ChargingStateDoneCharging = 2, // Charging is done; we should turn off after a delay
    ChargingStateBalancingCells = 3, // After charging is done, we will balance the cells for g_balanceDuration
    ChargingStateManuallyStopped = 4,
};
typedef uint8_t ChargingState;


#pragma mark -
#pragma mark Globals
// UI
CrbMenuItem *g_rootItem;

// Charging mode and state
ChargingMode g_chargingMode = ChargingModeNormal;
time_t g_startTime = 0; // we only read the hour/minute
time_t g_duration = 0; // we only read the hour/minute
time_t g_endTime = 0;
uint8_t g_balanceDuration = 1; // In minutes...i should just store a time_t for ore consistency
time_t g_balancingEndTime = 0;

#pragma mark -
#pragma mark Defines

// EEPROM locations we read/write
#define EE_CHARGING_MODE_LOCATION 0
#define EE_CHARGING_DURATION_LOCATION 1 // Takes 4 bytes!
#define EE_CHARGING_START_TIME 5 // Takes 4 bytes!
#define EE_BALANCE_TIME 9 // 1 byte
#define EE_UNUSED 10 // free location

#define PIN_BMS_HLIM 2 // high when the BMS HLIM is hit and we should stop charging. Low otherwise

#define PIN_CHARGING_STATUS 3
#define PIN_BMS_POWER 4 // Turns on the BMS when HIGH
#define PIN_EVSE_PILOT 5 // Turns on the pilot signal when HIGH
#define PIN_CHARGER_OFF 6  // Turns off the charger when HIGH (by sending 5v to the manzanita micro's pin 2). A safety feature is the 5v signal from pin 1 of the charger is looped to the "normally closed" relay controlled by the BMS "high limit". The BMS opens the circuit to let the charger charge.
#define PIN_ARDUINO_OFF 7
#define PIN_EVSE_PROXIMITY A0

// aliases to make the code more readable when setting values for the pins
#define BMS_HLIM_REACHED HIGH // When at 5v the BMS has opened the circuit telling us the HLIM was hit
#define BMS_HLIM_NOT_REACHED LOW // The BMS pulls the line to ground making it low when it is okay to charge

#define BMS_MODE_TURNED_ON HIGH
#define BMS_MODE_TURNED_OFF LOW

#define PILOT_USING_RELAY 1 // WARNING: You want this at 1 meaning we are going to use a relay to pull the pilot signal down. It is the safest for the way things are setup.

#if PILOT_USING_RELAY
    #define PILOT_SIGNAL_MODE_ON HIGH // turn on the relay, which pulls the EVSE signal to ground
    #define PILOT_SIGNAL_MODE_OFF LOW
#else
    // We have to use a relay to pull the pilot signal down to ground. Sure, it does work to do it with the arduino pin, but the problem is it pulls to ground when the arduino is off! That isn't great for safety, in case the arduino turns off. It would also mean that you have to turn on the arduino before plugging in; otherwise, the charger would turn on when you plug in (since the EVSE would get the pilot pulled to ground). That seemed bad to me. It would be okay if you had the ardunio built into part of the charger (as is done in the DIY 10kw charger on diyelectric car by valery).
    #define PILOT_SIGNAL_MODE_ON LOW // pull to low (ground) to signal the EVSE
    #define PILOT_SIGNAL_MODE_OFF HIGH
#endif

#define ADD_TURN_OFF_MENU_ITEM 0 // adds a menu item to turn off the pololu; this isn't needed as you can do it by hitting the on/off button again


#define CHARGER_MODE_ON LOW // Low signal cuts the 5v, letting it go on
#define CHARGER_MODE_OFF HIGH

#define ARDUINO_MODE_ALLOW_ON LOW
#define ARDUINO_MODE_TURN_OFF HIGH

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
    int proximityValue = analogRead(PIN_EVSE_PROXIMITY);
    
#if DEBUG
//    Serial.print("Proximity read value: ");
//    Serial.println(proximityValue);
#endif
    
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


static inline void setInitialPinStates() {
    // Setup our initial state
    // Pins default as INPUT, but we want to make them output
//    pinMode(PIN_BMS_HLIM, INPUT); // default value
    
    pinMode(PIN_BMS_POWER, OUTPUT);  
    digitalWrite(PIN_BMS_POWER, BMS_MODE_TURNED_OFF);
    
    pinMode(PIN_EVSE_PILOT, OUTPUT);
    digitalWrite(PIN_EVSE_PILOT, PILOT_SIGNAL_MODE_OFF);
    
    pinMode(PIN_CHARGER_OFF, OUTPUT);
    digitalWrite(PIN_CHARGER_OFF, CHARGER_MODE_OFF); // Charger off is a HIGH signal
    
    pinMode(PIN_ARDUINO_OFF, OUTPUT);
    digitalWrite(PIN_ARDUINO_OFF, ARDUINO_MODE_ALLOW_ON); // set to HIGH to turn off the arduino (and everything!)
    
    pinMode(PIN_CHARGING_STATUS, OUTPUT);
    digitalWrite(PIN_CHARGING_STATUS, LOW);
    
    
//    pinMode(PIN_EVSE_PROXIMITY, INPUT); // The default value is input, don't bother setting it
}

static bool isBMSHighLimitHit() {
#if DEBUG
//    Serial.print("BMS high limit read value: ");
//    Serial.println(digitalRead(PIN_BMS_HLIM));
#endif
    return digitalRead(PIN_BMS_HLIM) == BMS_HLIM_REACHED;
}

void setStandardStatusMessage();

void ChargingModeChangedAction(CrbActionMenuItem *sender) {
    // TODO: move this into the menu class, if it makes more sense there and i need it again
    if (!sender->hasOption(CrbMenuItemOptionSelected)) {
        // Update the global and write it out
        g_chargingMode = sender->getTag();
        EEPROM.write(EE_CHARGING_MODE_LOCATION, g_chargingMode);
        sender->addOption(CrbMenuItemOptionSelected);

        // If we are charging, then stop charging
        
        // remove the option from the other menu items
        CrbMenuItem *parent = sender->getParent();
        CrbMenuItem *walker = parent->getChild();
        while (walker) {
            if (walker != sender) {
                walker->removeOption(CrbMenuItemOptionSelected);
            }
            walker = walker->getNext();
        }
        setStandardStatusMessage();
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

static void updateTimerTimes() {
    // Convert the start time to a time based on today
    time_t nowTime = now();
    tmElements_t nowElements;
    breakTime(nowTime, nowElements);
    tmElements_t startTimeElements;
    breakTime(g_startTime, startTimeElements);
    // Take the elements from now, and use the hour and minute from when to start (with a 0 second)
    // However, if the time 
    nowElements.Hour = startTimeElements.Hour;
    nowElements.Minute = startTimeElements.Minute;
    nowElements.Second = 0;
    g_startTime = makeTime(nowElements);    

    // find the end time
    tmElements_t durationElements;
    breakTime(g_duration, durationElements);
    g_endTime = g_startTime + durationElements.Hour*SECS_PER_HOUR + durationElements.Minute*SECS_PER_MIN;
    
    // If we are within the current duration; we are going to charge now. If the start time was past, then we want to start tomrrow..
    if (nowTime >= g_startTime) {
        if (nowTime < g_endTime) {
            // Charging...
        } else {
            // End already happened, so add 24 hours to start tommorrow for these times
            g_startTime += SECS_PER_DAY;
            g_endTime += SECS_PER_DAY;
        }
    }
}

static inline time_t getChargingEndTime() {
    return g_endTime;
}

static void ChargingSaveStartTimeAction(CrbTimeSetMenuItem *sender) {
    if (g_startTime != sender->getTime()) {
        g_startTime = sender->getTime();
        updateTimerTimes();
        setStandardStatusMessage();
        defaultsWriteTime(EE_CHARGING_START_TIME, g_startTime);
    }
}

static void ChargingSaveDurationAction(CrbTimeSetMenuItem *sender) {
    if (g_duration != sender->getTime()) {
        g_duration = sender->getTime();
        updateTimerTimes();
        setStandardStatusMessage();
        defaultsWriteTime(EE_CHARGING_DURATION_LOCATION, g_duration);
    }
}

static void mnuSaveBalanceDurationAction(CrbMenuItem *sender) {
    // the pointer updates our value already; just write it out
    EEPROM.write(EE_BALANCE_TIME, g_balanceDuration);
}

static void setCurrentTimeAction(CrbTimeSetMenuItem *sender) {
    // use juse the hour/minute from the sender
    tmElements_t newTimeElements;
    breakTime(sender->getTime(), newTimeElements);
    tmElements_t nowTimeElements;
    breakTime(now(), nowTimeElements);
    nowTimeElements.Hour = newTimeElements.Hour;
    nowTimeElements.Minute = newTimeElements.Minute;
    time_t newTime = makeTime(nowTimeElements);
    
    RTC.set(newTime);
    setTime(newTime);
    
    // Update all our timer values, since the clock changed
    updateTimerTimes();
    setStandardStatusMessage();
}

#if ADD_TURN_OFF_MENU_ITEM
static void mnuTurnOff(CrbMenuItem *sender);
#endif

static inline void loadSettings() {
    // TODO: the first time we ever power on, it would be nice to clear out the EEPROM to all 0, so we know what the defaults are for everything
    
    // Restore the previous values
    g_chargingMode = EEPROM.read(EE_CHARGING_MODE_LOCATION);
    
    // Validate values
    if (g_chargingMode > ChargingModeTimed) {
        g_chargingMode = ChargingModeNormal;
    }
    
    g_startTime = defaultsReadTime(EE_CHARGING_START_TIME);
    g_duration = defaultsReadTime(EE_CHARGING_DURATION_LOCATION);
    g_balanceDuration = EEPROM.read(EE_BALANCE_TIME); // Can we validate this duration?
    
    updateTimerTimes();
}

static void ChargingModeEnter(CrbActionMenuItem *);


static inline void setupMenu() {
    g_lcd.begin(LCD_COLUMNS, LDC_ROWS);
    g_lcd.clear();
    
    g_rootItem = new CrbActionMenuItem(NULL, (CrbMenuItemAction)ChargingModeEnter, 0l);
    
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
    
    // When this action 
    CrbMenuItem *itemSetStartTime = new CrbMenuItem("Set start time >");
    g_rootItem->addChild(itemSetStartTime);
    itemSetStartTime->addChild(new CrbTimeSetMenuItem("Start time", (CrbMenuItemAction)ChargingSaveStartTimeAction, &g_startTime));

    CrbMenuItem *itemTimerDuration = new CrbMenuItem("Set timer duration >");
    g_rootItem->addChild(itemTimerDuration);
    itemTimerDuration->addChild(new CrbDurationMenuItem("Duration", (CrbMenuItemAction)ChargingSaveDurationAction, &g_duration));

    CrbMenuItem *balanceDuration = new CrbMenuItem("Balance duration >");
    g_rootItem->addChild(balanceDuration);
    balanceDuration->addChild(new CrbNumberEditMenuItem("Balance in mins", (CrbMenuItemAction)mnuSaveBalanceDurationAction, &g_balanceDuration));

    // I don't use the date..just the time. but it might be nice to set it.
//    CrbMenuItem *itemSetDate = new CrbMenuItem("Set current date >");
//    g_rootItem->addChild(itemSetDate);
//    itemSetDate->addChild(new CrbTimeSetMenuItem("Set the date", (CrbMenuItemAction)ChargingSaveDateAction, 0));
//
    CrbMenuItem *itemSetTime = new CrbClockMenuItem("Set time >"); // shows the current time when visible..
    g_rootItem->addChild(itemSetTime);
    CrbTimeSetMenuItem *timeSetItem = new CrbTimeSetMenuItem("Set the time", (CrbMenuItemAction)setCurrentTimeAction, NULL);
    itemSetTime->addChild(timeSetItem);
    
#if ADD_TURN_OFF_MENU_ITEM
    CrbMenuItem *itemTurnOff = new CrbMenuItem("Turn off >");
    g_rootItem->addChild(itemTurnOff);
    itemTurnOff->addChild(new CrbActionMenuItem("Enter to turn off", (CrbMenuItemAction)mnuTurnOff, 0));
#endif

    CrbMenuItem *clockMenuItem = new CrbClockMenuItem("   - Clock - ");
    g_rootItem->addChild(clockMenuItem);
    
    g_menu.init(&g_lcd, g_rootItem);
    g_menu.print();
}

static inline void setupTime() { 
    setSyncProvider(RTC.get);   // the function to get the time from the RTC
#if DEBUG
//    if (timeStatus() != timeSet) 
//        Serial.println("Unable to sync with the RTC");
//    else
//        Serial.println("RTC has set the system time");      
#endif
}


// Turns on or off the BMS. mode==HIGH to on and mode==LOW to off
static inline void setBMSToMode(uint8_t mode) {

#if DEBUG
//    Serial.print("Turning BMS ");
//    if (mode == BMS_MODE_TURNED_ON) {
//        Serial.println("on");
//    } else {
//        Serial.println("off");
//        // If turning off, make sure the charger is already off first!
//        if (digitalRead(PIN_CHARGER_OFF) != HIGH) {
//            Serial.println("WARNING: THE CHARGER SHOULD HAVE BEEN SIGNALED OFF BEFORE THE BMS IS TURNED OFF!");
//        }
//    }
#endif
    if (digitalRead(PIN_BMS_POWER) != mode) {
        digitalWrite(PIN_BMS_POWER, mode);
        // Give it a brief moment to kick on; don't worry if we are turning it off
        if (mode == BMS_MODE_TURNED_ON) {
            Alarm.delay(200); // TODO make sure 200ms is enough time to let it start
        }
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

ChargingState getChargingState();

static const char *amOrPmStringForTime(time_t t) {
    return isAM(t) ? "AM" : "PM";
}

// buffer for formatting times. 16 columns + NULL. DON'T overrun it
char g_timeBuffer[17]; // 00:00AM-00:00AM

void updateBalancingCellsStatus() {
    // The second line will say how long we have till the balance is done
    // Make sure you don't overrun this buffer!
    // calc the diffence from now
    time_t timeLeft = g_balancingEndTime - now();
    if (timeLeft < 0) timeLeft = 0; // shouldn't happen...
    // assign to ints so we get the type right when formatting
    int hrs = numberOfHours(timeLeft);
    int mins = numberOfMinutes(timeLeft);
    int secs = numberOfSeconds(timeLeft);
    sprintf(g_timeBuffer, " %02u:%02u:%02u", hrs, mins, secs);
    g_rootItem->setSecondLineMessage(g_timeBuffer);
    // Update the LCD without clearing! (otherwise it flickers)
    g_menu.printItemLine2(g_rootItem);
    
#if DEBUG
//    Serial.print("Balance time left:");
//    Serial.print(numberOfHours(timeLeft));
//    Serial.print(" hrs ");
//    Serial.print(numberOfMinutes(timeLeft));
//    Serial.print(" mins ");
//    Serial.print(numberOfSeconds(timeLeft));
//    Serial.println(" secs ");
    
#endif
    
}

ProximityMode g_lastReadProxMode = ProximityModeUnknown;

void setStandardStatusMessage() {
    ChargingState currentChargingState = getChargingState();
    if (currentChargingState == ChargingStateCharging) {
        g_rootItem->setName("Charging...");
        if (g_chargingMode == ChargingModeNormal) {
            g_rootItem->setSecondLineMessage("...to 100% SOC");
        } else {
            // Show when we will be done charging at
            time_t endTime = getChargingEndTime();
            // Make sure you don't overrun this buffer!
            sprintf(g_timeBuffer, "...until %02u:%02u%s", hourFormat12(endTime), minute(endTime), amOrPmStringForTime(endTime));
            g_rootItem->setSecondLineMessage(g_timeBuffer);
        }
    } else if (currentChargingState == ChargingStateDoneCharging) {
        g_rootItem->setName("Done charging");
        g_rootItem->setSecondLineMessage(NULL);
    } else if (currentChargingState == ChargingStateManuallyStopped) {
        g_rootItem->setName("Manually stopped");
        g_rootItem->setSecondLineMessage("[Enter restart]");
    } else if (currentChargingState == ChargingStateBalancingCells) { 
        g_rootItem->setName("Balancing cells");
        updateBalancingCellsStatus();
    } else if (g_chargingMode == ChargingModeNormal) {
        g_rootItem->setName("Waiting for plug");
        g_rootItem->setSecondLineMessage(" -> Settings");
    } else if (g_chargingMode == ChargingModeTimed) {
        g_lastReadProxMode = readProximityMode();
        if (g_lastReadProxMode == ProximityModePluggedAndLatched) {
            g_rootItem->setName("Timer set:");
        } else {
            g_rootItem->setName("Timer: Unplugged car!");
        }
        time_t endTime = getChargingEndTime();
        // Make sure you don't overrun this buffer!
        sprintf(g_timeBuffer, "%02u:%02u%s-%02u:%02u%s", hourFormat12(g_startTime), minute(g_startTime), amOrPmStringForTime(g_startTime), hourFormat12(endTime), minute(endTime), amOrPmStringForTime(endTime));
        g_rootItem->setSecondLineMessage(g_timeBuffer);
    } else {
        g_rootItem->setName("ERROR");
        g_rootItem->setSecondLineMessage("Unknown charging mode.");
    }
    g_menu.printItem(g_rootItem);    
}

void setup() {
    Serial.begin(9600);
#if DEBUG
    Serial.println("setup"); 
#endif
    // Order is pretty important!
    setInitialPinStates();
    setupTime(); // Must be done early..before we load settings
    loadSettings();
    setupMenu();

    // Set the intitial state for the menu item
    setStandardStatusMessage();
}

static bool canCharge() {
    // First, see if we are within the time period we can charge (if timed charging)
    bool result = false;
    if (g_chargingMode == ChargingModeTimed) {
        // We can charge when doing timed charging if we are within the right time period
        time_t currentTime = now();
        if (currentTime >= g_startTime && currentTime <= g_endTime) {
            result = true;
        }
    } else {
        // Normal charging
        result = true;
    }
    
    if (result) {
        // Ideally we should look for the 12v pilot signal. I don't have anything that reads the signal. It would be interesting to see if the current passing through the signal is less than 40mA. If it is, the arduino pin can be set as an OUTPUT and sink the current (I think this means take it to ground). notes http://arduino.cc/en/Tutorial/DigitalPins
        // Instead, see if we can charge by looking for the proximity signal to be correct. 
        ProximityMode proximityMode = readProximityMode();
        result = proximityMode == ProximityModePluggedAndLatched;
    }
    return result;
}

static void turnOnBMSAndStartCharging() {
    // Make sure the BMS is on before we turn on the charger
    setBMSToMode(BMS_MODE_TURNED_ON);
    // Then tell the EVSE to give us power
    setPilotSignalToMode(PILOT_SIGNAL_MODE_ON);
    // At this point, the EVSE will open its relay and give the charger power
    // Stop signaling the charger to not charge (turn off the signal we send it). This is doubly controlled by the relay hooked up to the BMS.
    setChargerToMode(CHARGER_MODE_ON);
    digitalWrite(PIN_CHARGING_STATUS, HIGH);     
}

static inline void startBalancingCells() {
    // Calculate the time when we will be done balancing
    g_balancingEndTime = now() + g_balanceDuration*60; // time is in minutes, convert to seconds
    setStandardStatusMessage();
}

static inline bool doneBalancingCells() {
    return now() >= g_balancingEndTime;
}

static void stopChargingAndTurnOffBMS() {
    // Stop charging, because we are going to be unplugged
    // We "soft stop" the charger by sending it 5v; this kills it right away, and makes it stop drawing amps
    setChargerToMode(CHARGER_MODE_OFF);
    // Wait a bit, and then signal the EVSE that we are off
    Alarm.delay(200); // TODO make sure 200ms is enough time for the charger to stop; we need to make this as short as possible, since we might be being unplugged and prefer to turn off the pilot signal ourselves
    // Turn off the pilot signal; this cuts the power to the charger
    setPilotSignalToMode(PILOT_SIGNAL_MODE_OFF);
    Alarm.delay(500); // TODO make sure 500ms (half a second) is long enough to wait before we turn off the BMS
    
    // Turn off the BMS
    setBMSToMode(BMS_MODE_TURNED_OFF);
    setStandardStatusMessage();
    digitalWrite(PIN_CHARGING_STATUS, LOW); 
}

// NOTE: be careful on what we write to the state, as going from certain states to others might leave the charger charging!
static ChargingState g_chargingState = ChargingStateOff;

ChargingState getChargingState() {
    return g_chargingState;
}

static void ChargingModeEnter(CrbActionMenuItem *) {
    // Manually turn charging off if we are charging 
    if (g_chargingState == ChargingStateCharging || g_chargingState == ChargingStateBalancingCells){
        g_chargingState = ChargingStateManuallyStopped;
        stopChargingAndTurnOffBMS();
    } else if (g_chargingState == ChargingStateManuallyStopped || g_chargingState == ChargingStateDoneCharging)  {
        // Go back to off, and re-enable the timer (or start charging again)
        g_chargingState = ChargingStateOff;
        setStandardStatusMessage();
    }
}

#if ADD_TURN_OFF_MENU_ITEM
static void mnuTurnOff(CrbMenuItem *sender) {
    // stop the charger, if we are charging
    if (g_chargingState == ChargingStateCharging || g_chargingState == ChargingStateBalancingCells){
        g_chargingState = ChargingStateDoneCharging;
        stopChargingAndTurnOffBMS();
    } else {
        g_chargingState = ChargingStateDoneCharging;
    }
    // go back to the root menu item so we can display the results
    g_menu.gotoRootItem();
}
#endif


void loop() {
    g_menu.loopOnce();
    
    switch (g_chargingState) {
        case ChargingStateOff:
            // Check to see if we can move to the next charging state
            if (canCharge()) {
                // We can charge, do it
                g_chargingState = ChargingStateCharging;
                turnOnBMSAndStartCharging();
                setStandardStatusMessage();
            }
            // If we are timed...see if we should update if we have the plug ready or not
            if (g_chargingMode == ChargingModeTimed) {
                if (g_lastReadProxMode != readProximityMode()) {
                    setStandardStatusMessage();
                }
            }
            break;
        case ChargingStateCharging:
            // Check to see if we need to stop charging, because the timer stopped, or the proximity switch was flipped
            if (!canCharge()) {
                g_chargingState = ChargingStateOff;
                stopChargingAndTurnOffBMS();
            } else if (isBMSHighLimitHit()) {
                // Go into balancing of cells
                g_chargingState = ChargingStateBalancingCells; // Set the state first, since startBalancingCells updates the status which is based on it
                startBalancingCells();
            }
            break;
        case ChargingStateBalancingCells:
            // This state is similar to the charging state (things are still on!)
            if (!canCharge()) {
                g_chargingState = ChargingStateOff;
                stopChargingAndTurnOffBMS();
            } else if (doneBalancingCells()) {
                g_chargingState = ChargingStateDoneCharging;
                stopChargingAndTurnOffBMS();
            } else {
                // Update the menu where we show how many seconds are left when balancing
                updateBalancingCellsStatus();
            }
            break;
        case ChargingStateManuallyStopped:
            // Don't do anything
            break;
        case ChargingStateDoneCharging:
            // When we hit this, delay a short bit...then turn off
            delay(500);
            g_rootItem->setSecondLineMessage("...Turning off.");
            g_menu.printItem(g_rootItem);    
            delay(1000);
            digitalWrite(PIN_ARDUINO_OFF, ARDUINO_MODE_TURN_OFF);
            delay(1000); // Shouldn't need this...
            // If we get back here, go manually off, so the user can stop again
            g_chargingState = ChargingStateManuallyStopped;
            setStandardStatusMessage();
            break;
    }
}






