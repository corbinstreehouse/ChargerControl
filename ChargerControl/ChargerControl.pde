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

#include "LCDDatePicker.h"
#include "RTClib.h"
#include <EEPROM.h>

#define DEBUG 1

#include <Time.h>  

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


CrbMenuItem g_itemSettings = CrbMenuItem("Settings >", NULL);
    CrbMenuItem g_itemSetStartTime = CrbMenuItem("Set start time >", NULL);
    CrbMenuItem g_itemSetTimerDuration = CrbMenuItem("Set timer duration >", NULL);
    CrbMenuItem g_itemSetClock = CrbMenuItem("Set date/time >", NULL);


// enums take 2 bytes, which sucks, so I typedef it to a uint8_t
enum _ChargingMode {
    ChargingModeNormal = 0,
    ChargingModeTimed = 1,
};
typedef uint8_t ChargingMode;

ChargingMode g_chargingMode;
CrbMenuItem *g_rootItem;

// EEPROM locations we read/write
#define EE_CHARGING_MODE_LOCATION 0

void ChargingModeChangedAction(CrbMenuItem *sender) {
    g_chargingMode = sender->getTag();

    // TODO: move this into the menu class, if it makes more sense there and i need it again
    if (!sender->hasOption(CrbMenuItemOptionSelected)) {
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

void setup() {
#if DEBUG
    Serial.begin(9600);
    Serial.println("setup"); 
#endif
    
    g_lcd.begin(LCD_COLUMNS, LDC_ROWS);
    g_lcd.clear();
    
    g_rootItem = new CrbMenuItem("Charging Enabled", NULL);
    CrbMenuItem *chargingModeItem = new CrbMenuItem("Charging Mode >", NULL);
    CrbMenuItem *itemNormalChargingMode = new CrbMenuItem("Normal charging", ChargingModeChangedAction);
    itemNormalChargingMode->setTag(ChargingModeNormal);
    
    CrbMenuItem *itemTimedChargingMode = new CrbMenuItem("Timed charging", ChargingModeChangedAction);
    itemTimedChargingMode->setTag(ChargingModeTimed);
    
    
    g_rootItem->addChild(chargingModeItem);
    chargingModeItem->addChild(itemNormalChargingMode);
    chargingModeItem->addChild(itemTimedChargingMode);
    
    // Restore the previous value
    g_chargingMode = EEPROM.read(EE_CHARGING_MODE_LOCATION);
#if DEBUG
    Serial.print("charging mode: ");
    Serial.println(g_chargingMode);
#endif
    
    // Validate values
    if (g_chargingMode > ChargingModeTimed) {
        g_chargingMode = ChargingModeNormal;
    }
    if (g_chargingMode == ChargingModeNormal) {
        itemNormalChargingMode->addOption(CrbMenuItemOptionSelected);
    } else { // timed
        itemTimedChargingMode->addOption(CrbMenuItemOptionSelected);
    }
    
    g_rootItem->addChild(&g_itemSettings);
        g_itemSettings.addChild(&g_itemSetStartTime);
        g_itemSettings.addChild(&g_itemSetTimerDuration);
        g_itemSettings.addChild(&g_itemSetClock);
    
    g_menu.init(&g_lcd, g_rootItem);
    g_menu.print();
    
    
}

void loop() {
    g_menu.loopOnce();
}






/*
time_t requestSync();

void processSyncMessage();
void digitalClockDisplay();
void printDigits(int digits);

void digitalClockDisplay(){
    // digital clock display of the time
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.print(" ");
    Serial.print(day());
    Serial.print(" ");
    Serial.print(month());
    Serial.print(" ");
    Serial.print(year()); 
    Serial.println(); 
}

void printDigits(int digits){
    // utility function for digital clock display: prints preceding colon and leading 0
    Serial.print(":");
    if(digits < 10)
        Serial.print('0');
    Serial.print(digits);
}

time_t requestSync() {
    DateTime now = RTC.now();
    return now.
    return 0; // the time will be sent later in response to serial mesg
}









void setup() {


    setSyncProvider(requestSync);
    
    Serial.println("Waiting for sync message");

    
    // set up the LCD's number of rows and columns: 
    lcd.begin(LCD_COLUMNS, LDC_ROWS);
    
//    DateTime t = 
    
    // TODO: get the initial time from the RTC
    
    

//    Serial.print("\n\n");
//    Serial.print("Took "); Serial.print(time); Serial.println(" ms");
    // lcd.setBacklight(BLUE);
    
    lcd.setCursor(0, 1);
    lcd.print("corbin dunn");
}

RTC_DS1307 RTC;

bool g_runClock = false;

//uint8_t i=0;
void loop() {
    // set the cursor to column 0, line 1
    // (note: line 1 is the second row, since counting begins with 0):
 //   lcd.setCursor(0, 1);
//    lcd.setCursor(0, 1);
//    lcd.print("corbin dunn");
    // print the number of seconds since reset:
  //  lcd.print(millis()/1000);
    
    uint8_t buttons = lcd.readButtons();
    
    if (buttons) {
      // lcd.clear();
        lcd.setCursor(0,0);
        if (buttons & BUTTON_UP) {
            lcd.print("UP ");
            //lcd.setBacklight(RED);
        }
        if (buttons & BUTTON_DOWN) {
            lcd.print("DOWN ");
            //lcd.setBacklight(YELLOW);
        }
        if (buttons & BUTTON_LEFT) {
            lcd.print("LEFT ");
            //   lcd.setBacklight(GREEN);
        }
        if (buttons & BUTTON_RIGHT) {
            lcd.print("RIGHT ");
            // lcd.setBacklight(TEAL);
        }
        if (buttons & BUTTON_SELECT) {
            lcd.print("SELECT ");
            //lcd.clear();  
            lcd.blink();
            //lcd.setBacklight(VIOLET);
            g_runClock = g_runClock ? 0 : 1;
            
            Serial.print("runclock:");
            Serial.print(g_runClock               );
            Serial.print("\n\n");
                         
        }
    }
    
    if (g_runClock) {
        // print the time
        DateTime now = RTC.now();
        
        lcd.setCursor(0,0);
        lcd.print(now.month(), DEC);
        lcd.print('/');
        lcd.print(now.day(), DEC);
        lcd.print('/');
        lcd.print(now.year() - 2000, DEC);
        lcd.print(' ');
        lcd.print(now.hour(), DEC);
        lcd.print(':');
        lcd.print(now.minute(), DEC);
        lcd.print(':');
        lcd.print(now.second(), DEC);
        
    }
    
//    if(timeStatus()!= timeNotSet)   
//    {
//        digitalWrite(13,timeStatus() == timeSet); // on if synced, off if needs refresh  
////        digitalClockDisplay();  
//    }
   // delay(1000);    
}

*/

