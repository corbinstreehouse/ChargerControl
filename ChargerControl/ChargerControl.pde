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
#include <Time.h>  

#include "LCDDatePicker.h"


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

// Global charging state
ChargingMode g_chargingMode;
CrbMenuItem *g_rootItem;

// EEPROM locations we read/write
#define EE_CHARGING_MODE_LOCATION 0

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

static void ChargingSaveStartTimeAction(CrbTimeSetMenuItem *sender) {
    
}

static inline void setupMenu() {
    g_lcd.begin(LCD_COLUMNS, LDC_ROWS);
    g_lcd.clear();
    
    g_rootItem = new CrbMenuItem("Charging Enabled");
    CrbMenuItem *chargingModeItem = new CrbMenuItem("Charging Mode >");
    CrbMenuItem *itemNormalChargingMode = new CrbActionMenuItem("Normal charging", (CrbMenuItemAction) ChargingModeChangedAction, ChargingModeNormal);
    CrbMenuItem *itemTimedChargingMode = new CrbActionMenuItem("Timed charging", (CrbMenuItemAction)ChargingModeChangedAction, ChargingModeTimed);
    
    g_rootItem->addChild(chargingModeItem);
    chargingModeItem->addChild(itemNormalChargingMode);
    chargingModeItem->addChild(itemTimedChargingMode);
    
    // Restore the previous value
    g_chargingMode = EEPROM.read(EE_CHARGING_MODE_LOCATION);
    
    // Validate values
    if (g_chargingMode > ChargingModeTimed) {
        g_chargingMode = ChargingModeNormal;
    }
    if (g_chargingMode == ChargingModeNormal) {
        itemNormalChargingMode->addOption(CrbMenuItemOptionSelected);
    } else { // timed
        itemTimedChargingMode->addOption(CrbMenuItemOptionSelected);
    }
    
    CrbMenuItem *itemSettings = new CrbMenuItem("Settings >");
    g_rootItem->addChild(itemSettings);
    
    // When this action 
    CrbMenuItem *itemSetStartTime = new CrbMenuItem("Set start time >");
    itemSettings->addChild(itemSetStartTime);
    itemSetStartTime->addChild(new CrbTimeSetMenuItem("Start time", (CrbMenuItemAction)ChargingSaveStartTimeAction, 0));
    
    itemSettings->addChild(new CrbMenuItem("Set timer duration >"));
    itemSettings->addChild(new CrbMenuItem("Set date/time >"));
    
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

void setup() {
#if DEBUG
    Serial.begin(9600);
    Serial.println("setup"); 
#endif
    setupMenu();
    setupTime();
}

void loop() {
    g_menu.loopOnce();

}

