//
//  CrbMenu.h
//  ChargerControl
//
//  Created by corbin dunn on 5/28/12 .
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef ChargerControl_CrbMenu_h
#define ChargerControl_CrbMenu_h

#include "Adafruit_RGBLCDShield.h"

#define DEBUG_MENU 1 // Set to 1 to debug the setup and stuff

class CrbMenuItem;

class CrbMenu {
private:
    Adafruit_RGBLCDShield *_lcd;
    CrbMenuItem *_rootItem;
    CrbMenuItem *_currentItem;
    
    
    void handleButton(uint8_t buttons);
public:
    CrbMenu();
    
    void init(Adafruit_RGBLCDShield *lcd, CrbMenuItem *rootItem);
    
    void loopOnce(); // loops and checks for the buttons on the lcd to navigate the menu

    void print(); // Writes to the lcd

    // manual control; these do nothing if there is no item available
    void gotoPriorSibling(); // Go "up" 
    void gotoNextSibling(); // Go "down"
    void gotoParent(); // Go "left"  (aka: back)
    void gotoFirstChild(); // Go "right" (aka next)
};

// This is just a simple linked list
class CrbMenuItem {
private:
	const char*_name;
    
    CrbMenuItem *_parent; // The item we go "back" to
    CrbMenuItem *_prior; // The previous sibling
    CrbMenuItem *_next; // The next sibling (may be NULL, can be set as the first item in the list to have lists loop around)
    CrbMenuItem *_firstChild; // First child in the next group of menu items    

protected: // For our friend CrbMenu to touch
	inline CrbMenuItem *getParent() const { return _parent; }
	inline CrbMenuItem *getPrior() const { return _prior; }
	inline CrbMenuItem *getNext() const { return _next; }
	inline CrbMenuItem *getFirstChild() const { return _firstChild; }
    friend class CrbMenu;
    
public:
    CrbMenuItem(const char *name);
    
    CrbMenuItem *addNextWithName(const char *name); // If _next is set, it searches till it finds a nil next and sets it
    CrbMenuItem *addChildWithName(const char *name);

    void addNext(CrbMenuItem *next); // If _next is set, it searches till it finds a nil next and sets it
    void addChild(CrbMenuItem *child);
    
	inline const char *getName() const { return _name; }
};



#endif
