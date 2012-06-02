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

#include "Adafruit_RGBLCDShield.h"

#define DEBUG_MENU 1 // Set to 1 to debug the setup and stuff

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
public:
    CrbMenu();
    
    void init(Adafruit_RGBLCDShield *lcd, CrbMenuItem *rootItem);
    
    void loopOnce(); // loops and checks for the buttons on the lcd to navigate the menu

    void print(); // Writes to the lcd

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

    friend class CrbMenu; // so it can access the above protected methods
public:
    CrbMenuItem(const char *name);
    
	inline CrbMenuItem *getParent() const { return _parent; }
	inline CrbMenuItem *getPrior() const { return _priorSibling; }
	inline CrbMenuItem *getNext() const { return _nextSibling; }
	inline CrbMenuItem *getChild() const { return _child; }

    void addNext(CrbMenuItem *next); // If _next is set, it searches till it finds a nil next and sets it
    void addChild(CrbMenuItem *child);
#if DEBUG_MENU
	inline const char *getName() const { return _name; }
#endif
    inline void addOption(CrbMenuItemOption option) { _options = _options | option; }
    inline void removeOption(CrbMenuItemOption option) { _options = _options & ~option; }
    inline bool hasOption(CrbMenuItemOption option) { return (option & _options) != 0; }
};

typedef void (* CrbMenuItemAction)(CrbMenuItem *item);

class CrbActionMenuItem : public CrbMenuItem {
private:
    CrbMenuItemAction _action;
    unsigned int _tag;
protected:
    virtual void handleEnterButton(CrbMenu *sender);
public:
    // Fires the action on enter
    CrbActionMenuItem(const char *name, CrbMenuItemAction action, int tag);
    inline int getTag() { return _tag; }
};

class CrbTimeSetMenuItem : public CrbMenuItem {
private:
    CrbMenuItemAction _action;
    time_t _time;
    uint8_t _editLocation;
protected:
    void handleUpButton(CrbMenu *sender);
    void handleDownButton(CrbMenu *sender);
    void handleLeftButton(CrbMenu *sender);
    void handleRightButton(CrbMenu *sender);
    
    void handleEnterButton(CrbMenu *sender);
    void printLine2(Adafruit_RGBLCDShield *lcd);
public:
    time_t getTime() { return _time; }
    // Fires the action on enter
    CrbTimeSetMenuItem(const char *name, CrbMenuItemAction action, time_t time);
};

class CrbDurationMenuItem : public CrbMenuItem {
private:
    CrbMenuItemAction _action;
    time_t _duration;
    uint8_t _editLocation;
protected:
    void handleUpButton(CrbMenu *sender);
    void handleDownButton(CrbMenu *sender);
    void handleLeftButton(CrbMenu *sender);
    void handleRightButton(CrbMenu *sender);
    
    void handleEnterButton(CrbMenu *sender);
    void printLine2(Adafruit_RGBLCDShield *lcd);
public:
    time_t getDuration() { return _duration; }
    // Fires the action on enter
    CrbTimeSetMenuItem(const char *name, CrbMenuItemAction action, time_t duration);
};



#endif
