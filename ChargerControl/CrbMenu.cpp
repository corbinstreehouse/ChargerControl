//
//  CrbMenu.cpp
//  ChargerControl
//
//  Created by corbin dunn on 5/28/12 .
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "CrbMenu.h"

CrbMenuItem::CrbMenuItem(const char *name) : _name(name) {
    _parent = _priorSibling = _nextSibling = _child = 0;
    _options = 0;
}

//CrbMenuItem *CrbMenuItem::addNextWithName(const char *name) {
//    CrbMenuItem *result = new CrbMenuItem(name);
//    this->addNext(result);
//    return result;
//}
//
//CrbMenuItem *CrbMenuItem::addChildWithName(const char *name) {
//    CrbMenuItem *result = new CrbMenuItem(name);
//    this->addChild(result);
//    return result;
//}
//
void CrbMenuItem::addNext(CrbMenuItem *next) {
    CrbMenuItem *walker = this;
    // Fine the first item without a next
    while (walker->_nextSibling) {
        walker = walker->_nextSibling;
    }
    // doubly linked list forwards/backwards from that last child
    walker->_nextSibling = next;
    next->_priorSibling = walker;
    next->_parent = _parent; // we have the same parent
}

void CrbMenuItem::addChild(CrbMenuItem *child) {
    if (_child == NULL) {
        child->_parent = this;
        _child = child;
    } else {
        // Otherwise, add a next to the child to put it at the end of the children list for this item
        _child->addNext(child);
    }
}

void CrbMenuItem::handleUpButton(CrbMenu *sender) {
    sender->gotoPriorSibling();
}

void CrbMenuItem::handleDownButton(CrbMenu *sender) {
    sender->gotoNextSibling();
}

void CrbMenuItem::handleLeftButton(CrbMenu *sender) {
    sender->gotoParent();
}

void CrbMenuItem::handleRightButton(CrbMenu *sender) {
    sender->gotoFirstChild();
}

void CrbMenuItem::handleEnterButton(CrbMenu *sender) {
    sender->gotoFirstChild();
}


void CrbMenuItem::printLine1(Adafruit_RGBLCDShield *lcd) {
    // Print the parent item that we are on
    if (this->hasOption(CrbMenuItemOptionSelected)) {
        lcd->print("*");
    }
    lcd->print(_name);

}

void CrbMenuItem::printLine2(Adafruit_RGBLCDShield *lcd) {
    lcd->setCursor(0,1);
    if (this->getPrior() && this->getNext()) {
        if (this->getChild()) {
            lcd->print("       [Up/Down]");
        } else {
            // This is a menu item to choose
            lcd->print(" [Up/Down|Enter]");
        }
    } else if (this->getPrior()) {
        if (this->getChild()) {
            lcd->print("            [Up]");
        } else {
            lcd->print("      [Up|Enter]");
        }
    } else if (this->getNext()) {
        if (this->getChild()) {
            lcd->print("          [Down]");
        } else {
            lcd->print("    [Down|Enter]");
        }
    }    
}


CrbActionMenuItem::CrbActionMenuItem(const char *name, CrbMenuItemAction action, int tag) : CrbMenuItem(name) {
    _action = action;
    _tag = tag;
}

void CrbActionMenuItem::handleEnterButton(CrbMenu *sender) {
    // Prefer to fire the action
    if (_action) {
        _action(this);
    } else {
        // Use it as an alias to goto the right
        CrbMenuItem::handleEnterButton(sender);
    }
}

typedef enum {
    EditingTimeLocationNotStarted = 0,
    EditingTimeLocationHour = 1,
    EditingTimeLocationMinuteTens = 2,
    EditingTimeLocationMinute = 3,
    EditingTimeLocationAMPM = 4,
} EditingTimeLocation;

CrbTimeSetMenuItem::CrbTimeSetMenuItem(const char *name, CrbMenuItemAction action, time_t time) : CrbMenuItem(name) {
    _action = action;
    _time = time;
    _editLocation = EditingTimeLocationNotStarted;
}

void CrbTimeSetMenuItem::printLine2(Adafruit_RGBLCDShield *lcd) {
    lcd->setCursor(0,1);
    int timeHour = hour(_time);
    int timeHourToPrint = timeHour > 12 ? timeHour - 12 : timeHour;
    if (timeHourToPrint < 10) {
        lcd->print("0");
    }
    lcd->print(timeHourToPrint);
    lcd->print(":");
    
    int timeMinute = minute(_time);
    if (timeMinute < 10) {
        lcd->print("0");
    }
    lcd->print(timeMinute);
    
    if (timeHour > 12) {
        lcd->print(" PM");
    } else {
        lcd->print(" AM");
    }
    
    lcd->print(" [Enter]");
    
}

static void updateCursorForTime(Adafruit_RGBLCDShield *lcd, uint8_t timeLocation) {
    if (timeLocation == EditingTimeLocationNotStarted) {
        lcd->noBlink();
    } else {
        // move to the right area, and then blink
        switch (timeLocation) {
            case EditingTimeLocationHour: {
                lcd->setCursor(1, 1);  // 00
                break;
            }
            case EditingTimeLocationMinuteTens: {
                lcd->setCursor(3, 1);  // 00:0
                break;
            }
            case EditingTimeLocationMinute: {
                lcd->setCursor(4, 1);  // 00:00
                break;
            }
            case EditingTimeLocationAMPM: {
                lcd->setCursor(7, 1);  // 00:00 AM
                break;
            }
        }
        lcd->blink();
    }
}

void CrbTimeSetMenuItem::handleUpButton(CrbMenu *sender) {
    tmElements_t tm;
    breakTime(_time, tm);

    if (_editLocation == EditingTimeLocationHour) {
        tm.Hour++;
        if (tm.Hour > 12) {
            tm.Hour = 1; // Wrap
        }
    } else if (_editLocation == EditingTimeLocationMinuteTens) {
        tm.Minute += 10;
        if (tm.Minute >= 60) {
            tm.Minute -= 60;
        }
    } else if (_editLocation == EditingTimeLocationMinute) {
        int min = tm.Minute % 10;
        min++; 
        if (min > 9) {
            min = 0;
        }
        tm.Minute = floor(tm.Minute / 10)*10 + min;
    } else if (_editLocation == EditingTimeLocationAMPM) {
        tm.Hour += 12;
    }
    _time = makeTime(tm);
    // We don't clear the LCD (don't need to)
    this->printLine2(sender->getLCD());
    updateCursorForTime(sender->getLCD(), _editLocation);
}

void CrbTimeSetMenuItem::handleDownButton(CrbMenu *sender) {
    tmElements_t tm;
    breakTime(_time, tm);
    
    if (_editLocation == EditingTimeLocationHour) {
        if (tm.Hour > 2) {
            tm.Hour--; 
        } else {
            tm.Hour = 12; // wrap after 1
        }
    } else if (_editLocation == EditingTimeLocationMinuteTens) {
        int localMin = tm.Minute; // avoid unsigned so we can subtract 10 and compare to < 0
        localMin -= 10;
        if (localMin < 0) {
            localMin += 60;
        }
        tm.Minute = localMin;
    } else if (_editLocation == EditingTimeLocationMinute) {
        int min = tm.Minute % 10;
        min--; 
        if (min < 0) {
            min = 9;
        }
        tm.Minute = floor(tm.Minute / 10)*10 + min;
    } else if (_editLocation == EditingTimeLocationAMPM) {
        tm.Hour += 12;
    }
    _time = makeTime(tm);
    // We don't clear the LCD (don't need to)
    this->printLine2(sender->getLCD());
    updateCursorForTime(sender->getLCD(), _editLocation);
}

void CrbTimeSetMenuItem::handleLeftButton(CrbMenu *sender) {
    // Cancel editing
    if (_editLocation > EditingTimeLocationNotStarted) {
        _editLocation--;
        if (_editLocation < EditingTimeLocationHour) {
            _editLocation = EditingTimeLocationAMPM; // wrap
        }
        updateCursorForTime(sender->getLCD(), _editLocation);
    } else {    
        sender->gotoParent();
    }
}

void CrbTimeSetMenuItem::handleRightButton(CrbMenu *sender) {
    // Implicitly starts editing if we aren't
    if (_editLocation > EditingTimeLocationNotStarted) {
        _editLocation++;
        if (_editLocation > EditingTimeLocationAMPM) {
            _editLocation = EditingTimeLocationHour; // wrap
        }
        updateCursorForTime(sender->getLCD(), _editLocation);
    }
}


void CrbTimeSetMenuItem::handleEnterButton(CrbMenu *sender) {
    // If not editing, start editing
    if (_editLocation == EditingTimeLocationNotStarted) {
        _editLocation = EditingTimeLocationHour;
    } else {
        // Editing, stop editing and save
        _editLocation = EditingTimeLocationNotStarted;
        _action(this);
    }
    updateCursorForTime(sender->getLCD(), _editLocation);
}



#pragma mark -
#pragma mark -----------------------------

CrbMenu::CrbMenu() {
    
}

void CrbMenu::print() {
    if (_currentItem) {
#if DEBUG_MENU
        Serial.print("at menu item: ");
        Serial.println(_currentItem->getName());
#endif
        if (_lcd) {
            _lcd->clear();
            _lcd->setCursor(0,0);
            _currentItem->printLine1(_lcd);
            _currentItem->printLine2(_lcd);

        } else {
#if DEBUG_MENU
        Serial.println("error - no LCD");
#endif
        }
    } else {
#if DEBUG_MENU
        Serial.println("error - no current item to print");
#endif
    }
}

void CrbMenu::init(Adafruit_RGBLCDShield *lcd, CrbMenuItem *rootItem) {
    _lcd = lcd;
    _rootItem = rootItem;
    _currentItem = rootItem;
    print();
}

// no destructor...this is always alive

void CrbMenu::gotoPriorSibling() {
#if DEBUG_MENU
    Serial.println("goto prior");
#endif
    if (_currentItem->getPrior()) {
        _currentItem = _currentItem->getPrior();
        this->print();
    }
}

void CrbMenu::gotoNextSibling() {
#if DEBUG_MENU
    Serial.println("goto next");
#endif
    if (_currentItem->getNext()) {
        _currentItem = _currentItem->getNext();
        this->print();
    }
}

void CrbMenu::gotoParent() {
#if DEBUG_MENU
    Serial.println("goto parent");
#endif
    if (_currentItem->getParent()) {
        _currentItem = _currentItem->getParent();
        this->print();
    }
}

void CrbMenu::gotoFirstChild() {
#if DEBUG_MENU
    Serial.println("goto first child");
#endif
    if (_currentItem->getChild()) {
        _currentItem = _currentItem->getChild();
        this->print();
    }
}

void CrbMenu::loopOnce() {
    uint8_t buttons = _lcd->readButtons();
    if (buttons && _currentItem) {
#if DEBUG_MENU
        Serial.println("button pressed");
#endif
        if (buttons & BUTTON_UP) {
            _currentItem->handleUpButton(this);
        } else if (buttons & BUTTON_DOWN) {
            _currentItem->handleDownButton(this);
        } else if (buttons & BUTTON_LEFT) {
            _currentItem->handleLeftButton(this);
        } else if (buttons & BUTTON_RIGHT) {
            _currentItem->handleRightButton(this);
        } else if (buttons & BUTTON_SELECT) {
            _currentItem->handleEnterButton(this);
        }

        // Wait until the button is up
        while (_lcd->readButtons()) {
            // nop
        }
    }
}
