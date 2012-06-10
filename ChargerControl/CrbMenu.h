//
//  CrbMenu.h
//  ChargerControl
//
//  Created by corbin dunn on 5/28/12 .
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef ChargerControl_CrbMenu_h
#define ChargerControl_CrbMenu_h

#include "Arduino.h"
#include <Wire.h>
#include <Time.h>  
#include <stdint.h>

#include "Adafruit_RGBLCDShield.h"


#define DEBUG_MENU 0 // Set to 1 to debug the setup and stuff

class CrbMenuItem;

enum _CrbMenuItemOptions {
    // Selected items will have a * before the name to show it as selected
    CrbMenuItemOptionSelected = 1 << 0,
//    ChargingModeDisabled = 1 << 2,
//    ChargingModeNotSet = 255,
};
typedef uint8_t CrbMenuItemOption;

class CrbMenu {
private:
    Adafruit_RGBLCDShield *_lcd;
    CrbMenuItem *_rootItem;
    CrbMenuItem *_currentItem;
    unsigned long _showStartTime;
    int _characterScrolled;
    
    void showCurrentItem();
public:
    CrbMenu();
    
    void init(Adafruit_RGBLCDShield *lcd, CrbMenuItem *rootItem);
    
    void loopOnce(); // loops and checks for the buttons on the lcd to navigate the menu

    void print(); // Writes to the lcd
    void printItem(CrbMenuItem *item); // Writes the message for this item, if it is the current item
    void printItemLine2(CrbMenuItem *item); // Updates line 2 without clearing the LCD

    // Each menu item may call into this class to do the actual work of navigating the menu structure
    void gotoPriorSibling(); // Go "up" 
    void gotoNextSibling(); // Go "down"
    void gotoParent(); // Go "left"  (aka: back)
    void gotoFirstChild(); // Go "right" (aka next)
    
    inline Adafruit_RGBLCDShield *getLCD() { return _lcd; }
};

// This is just a simple linked list
class CrbMenuItem {
private:
	const char *_name;
    const char *_secondLineMessage;
    
    CrbMenuItem *_parent; // The item we go "back" to
    CrbMenuItem *_priorSibling; // The previous sibling
    CrbMenuItem *_nextSibling; // The next sibling (may be NULL, can be set as the first item in the list to have lists loop around)
    CrbMenuItem *_child; // First child in the next group of menu items

    CrbMenuItemOption _options;
protected:
    // These can be overridden to make specific menu item actions that are done instead of the normal ones. By default, we just call back into CrbMenu to goto the prior/next, etc
    virtual void handleUpButton(CrbMenu *sender);
    virtual void handleDownButton(CrbMenu *sender);
    virtual void handleLeftButton(CrbMenu *sender);
    virtual void handleRightButton(CrbMenu *sender);
    virtual void handleEnterButton(CrbMenu *sender);

    virtual void printLine1(Adafruit_RGBLCDShield *lcd);
    virtual void printLine2(Adafruit_RGBLCDShield *lcd);
    
    // Called each loop() when active
    virtual void tick(CrbMenu *sender) { }
    virtual void willBeShown(CrbMenu *sender) { } // Called once when the menu item is shown to allow it to udpate things

    int totalColumnCount();
    
    friend class CrbMenu; // so it can access the above protected methods
public:
    CrbMenuItem(const char *name);
    
    inline void setSecondLineMessage(const char *msg) { _secondLineMessage = msg; }
    
	inline CrbMenuItem *getParent() const { return _parent; }
	inline CrbMenuItem *getPrior() const { return _priorSibling; }
	inline CrbMenuItem *getNext() const { return _nextSibling; }
	inline CrbMenuItem *getChild() const { return _child; }

    void addNext(CrbMenuItem *next); // If _next is set, it searches till it finds a nil next and sets it
    void addChild(CrbMenuItem *child);
#if DEBUG_MENU
	inline const char *getName() const { return _name; }
#endif
    inline void setName(const char *name) { _name = name; }
    inline void addOption(CrbMenuItemOption option) { _options = _options | option; }
    inline void removeOption(CrbMenuItemOption option) { _options = _options & ~option; }
    inline bool hasOption(CrbMenuItemOption option) { return (option & _options) != 0; }
};

typedef void (* CrbMenuItemAction)(CrbMenuItem *item);

class CrbActionMenuItem : public CrbMenuItem {
private:
    CrbMenuItemAction _action;
protected:
    unsigned int _tag;
    virtual void handleEnterButton(CrbMenu *sender);
public:
    // Fires the action on enter
    CrbActionMenuItem(const char *name, CrbMenuItemAction action, int tag);
    inline int getTag() { return _tag; }
};

class CrbTimeSetMenuItem : public CrbMenuItem {
private:
    CrbMenuItemAction _action;
protected:
    time_t *_originalTime; // pointer to the original time; may be NULL
    time_t _time; // the time we are editing; updated when the menu is shown
    uint8_t _editLocation;
    
    virtual bool isDuration() { return false; }
    
    void handleUpButton(CrbMenu *sender);
    void handleDownButton(CrbMenu *sender);
    void handleLeftButton(CrbMenu *sender);
    void handleRightButton(CrbMenu *sender);
    void handleEnterButton(CrbMenu *sender);
    
    virtual void printLine2(Adafruit_RGBLCDShield *lcd);
    void updateCursorForLine2(Adafruit_RGBLCDShield *lcd);
    void willBeShown(CrbMenu *sender) { if (_originalTime) _time = *_originalTime; else _time = now(); }
public:
    time_t getTime() { return _time; }
    // Fires the action on enter
    // NULL timePointer means we use now() as the value we are editing. Otherwise, we refresh our value to the pointer's value when being shown
    CrbTimeSetMenuItem(const char *name, CrbMenuItemAction action, time_t *timePointer);
};

// This is really an int editing option with a min/max
class CrbDurationMenuItem : public CrbTimeSetMenuItem {
protected:
    bool isDuration() { return true; }
public:
    CrbDurationMenuItem(const char *name, CrbMenuItemAction action, time_t *time);
};

class CrbClockMenuItem : public CrbMenuItem {
protected:    
    void tick(CrbMenu *sender);
    void printLine2(Adafruit_RGBLCDShield *lcd);
public:
    CrbClockMenuItem(const char *name);
};

// Edits numbers; currently I only need uint8_t, but we could make it use ints if needed
// NOTE: don't use the tag.
class CrbNumberEditMenuItem : public CrbMenuItem { 
private:
    uint8_t _currentValue; // used when editing
    uint8_t *_originalValue; // 0-255
    bool _isEditing;

    void handleUpButton(CrbMenu *sender);
    void handleDownButton(CrbMenu *sender);
    void handleEnterButton(CrbMenu *sender);
    
    virtual void printLine2(Adafruit_RGBLCDShield *lcd);
    void willBeShown(CrbMenu *sender) { _currentValue = *_originalValue; }
public:
    CrbNumberEditMenuItem(const char *name, uint8_t *originalValue);
    
};


#endif
