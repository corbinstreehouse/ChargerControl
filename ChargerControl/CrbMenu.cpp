//
//  CrbMenu.cpp
//  ChargerControl
//
//  Created by corbin dunn on 5/28/12 .
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "CrbMenu.h"
#include "Time.h"
#include <string.h>

#define LCD_COLUMNS 16
#define SCROLL_WAIT 500

CrbMenuItem::CrbMenuItem(const char *name) : _name(name) {
    _parent = _priorSibling = _nextSibling = _child = 0;
    _options = 0;
    _secondLineMessage = NULL;
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
    if (_name) lcd->print(_name);

}

void CrbMenuItem::printLine2(Adafruit_RGBLCDShield *lcd) {
    lcd->setCursor(0,1);
    // Prefer whatever message we have stored
    if (_secondLineMessage) {
        lcd->print(_secondLineMessage);
    }
    // Otherwise, say our navigation status (if we are in the menu system)
    else if (this->getPrior() && this->getNext()) {
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

CrbTimeSetMenuItem::CrbTimeSetMenuItem(const char *name, CrbMenuItemAction action, time_t *timePointer) : CrbMenuItem(name) {
    _action = action;
    _time = timePointer ? *timePointer : now();
    _originalTime = timePointer;
    _editLocation = EditingTimeLocationNotStarted;
}

const char zeroCh = '0';

static inline void _CrbPrintValue(int timeHour, Adafruit_RGBLCDShield *lcd) {
    if (timeHour < 10) {
        lcd->print(zeroCh);
    }
    lcd->print(timeHour);
}

static void _CrbPrintTime(time_t time, Adafruit_RGBLCDShield *lcd, bool includeSecond, bool includeAMPM) {
    lcd->setCursor(0,1);

    int printHour = includeAMPM ? hourFormat12(time) : hour(time);
    _CrbPrintValue(printHour, lcd);
    
    lcd->print(":");
    
    _CrbPrintValue(minute(time), lcd);
    
    if (includeSecond) {
        lcd->print(":");
        _CrbPrintValue(second(time), lcd);
    }
    
    if (includeAMPM) {
        if (hour(time) > 12) {
            lcd->print(" PM");
        } else {
            lcd->print(" AM");
        }
    }
}

void CrbTimeSetMenuItem::printLine2(Adafruit_RGBLCDShield *lcd) {
    _CrbPrintTime(_time, lcd, false, !this->isDuration());
    lcd->print(" [Enter]");
}

void CrbTimeSetMenuItem::updateCursorForLine2(Adafruit_RGBLCDShield *lcd) {
    
    if (_editLocation == EditingTimeLocationNotStarted) {
        lcd->noBlink();
    } else {
        // move to the right area, and then blink
        switch (_editLocation) {
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
        // Keep the AM PM part right..
        bool add12 = false;
        if (!this->isDuration() && tm.Hour > 12) {
            tm.Hour -= 12;
            add12 = true;
        }
        tm.Hour++;
        if (tm.Hour > 12) {
            if (this->isDuration()) {
                tm.Hour = 0; //Wrap to 0
            } else {
                tm.Hour = 1; // Wrap to 1
            }
        }
        if (add12) tm.Hour += 12;
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
    this->updateCursorForLine2(sender->getLCD());
}

void CrbTimeSetMenuItem::handleDownButton(CrbMenu *sender) {
    tmElements_t tm;
    breakTime(_time, tm);
    
    if (_editLocation == EditingTimeLocationHour) {
        // Let it go to 0 if it is a duration edit; else, wrap to 12
        if (tm.Hour > 1 || (this->isDuration() && tm.Hour >= 1)) {
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
    this->updateCursorForLine2(sender->getLCD());
}

void CrbTimeSetMenuItem::handleLeftButton(CrbMenu *sender) {
    // Cancel editing
    if (_editLocation > EditingTimeLocationNotStarted) {
        _editLocation--;
        if (_editLocation < EditingTimeLocationHour) {
            _editLocation = this->isDuration() ? EditingTimeLocationMinute : EditingTimeLocationAMPM; // wrap
        }
        this->updateCursorForLine2(sender->getLCD());
    } else {
        sender->gotoParent();
    }
}

void CrbTimeSetMenuItem::handleRightButton(CrbMenu *sender) {
    // Implicitly starts editing if we aren't
    if (_editLocation > EditingTimeLocationNotStarted) {
        _editLocation++;
        if (_editLocation > (this->isDuration() ? EditingTimeLocationMinute : EditingTimeLocationAMPM)) {
            _editLocation = EditingTimeLocationHour; // wrap
        }
        this->updateCursorForLine2(sender->getLCD());
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
    this->updateCursorForLine2(sender->getLCD());
}

#pragma mark - CrbDurationMenuItem


CrbDurationMenuItem::CrbDurationMenuItem(const char *name, CrbMenuItemAction action, time_t *time) : CrbTimeSetMenuItem(name, action, time) {
    
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

void CrbMenu::printItem(CrbMenuItem *item) {
    if (_currentItem == item) {
        this->print();
        // Scroll if we need to
    }
}

void CrbMenu::printItemLine2(CrbMenuItem *item) {
    if (_currentItem == item) {
        _currentItem->printLine2(_lcd);
    }
}


void CrbMenu::showCurrentItem() {
    _currentItem->willBeShown(this);
    print();
    _characterScrolled = 0;
    _showStartTime = millis();
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
        this->showCurrentItem();
    } else {
        // Wrap by walking to the end
        while (_currentItem->getNext()) {
            _currentItem = _currentItem->getNext();
        }
        this->showCurrentItem();
    }
}

void CrbMenu::gotoNextSibling() {
#if DEBUG_MENU
    Serial.println("goto next");
#endif
    if (_currentItem->getNext()) {
        _currentItem = _currentItem->getNext();
        this->showCurrentItem();
    } else {
        // wrap
        while (_currentItem->getPrior()) {
            _currentItem = _currentItem->getPrior();
        }
        this->showCurrentItem();
    }
}

void CrbMenu::gotoParent() {
#if DEBUG_MENU
    Serial.println("goto parent");
#endif
    if (_currentItem->getParent()) {
        _currentItem = _currentItem->getParent();
        this->showCurrentItem();
    }
}

void CrbMenu::gotoFirstChild() {
#if DEBUG_MENU
    Serial.println("goto first child");
#endif
    if (_currentItem->getChild()) {
        _currentItem = _currentItem->getChild();
        this->showCurrentItem();
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
            // should we process timers here? Alarm.delay(0)?
        }
    }
    _currentItem->tick(this);
    // Scroll the name, if necessary
    int totalCount = _currentItem->totalColumnCount();
    int amountToScroll = totalCount - LCD_COLUMNS;
    if (amountToScroll > 0) {
        unsigned long newNow = millis();
        if ((newNow - _showStartTime) > SCROLL_WAIT) {
            _showStartTime = newNow;
            if (_characterScrolled >= 0) {
                _characterScrolled++;
                if (_characterScrolled > amountToScroll) {
                    _characterScrolled *= -1; // Wait... 50ms before going back
                } else {
                    _lcd->scrollDisplayLeft();
                }
            } else {
                // Going back to 0
                _characterScrolled++;
                if (_characterScrolled == 0) {
                    // Wait
                } else {
                    _lcd->scrollDisplayRight();
                }
            }
        }
    }
}
        

int CrbMenuItem::totalColumnCount() {
    if (_name) {
        return strlen(_name);
    } else {
        return 0;
    }
}

CrbClockMenuItem::CrbClockMenuItem(const char *name) : CrbMenuItem(name) {
    
}

void CrbClockMenuItem::printLine2(Adafruit_RGBLCDShield *lcd) {
    lcd->setCursor(0,1);    
    _CrbPrintTime(now(), lcd, true, true);
}

// Constantly update the time
void CrbClockMenuItem::tick(CrbMenu *sender) { 
    this->printLine2(sender->getLCD());
}

CrbNumberEditMenuItem::CrbNumberEditMenuItem(const char *name, CrbMenuItemAction action, uint8_t *originalValue) : CrbActionMenuItem(name, action, 0) {
    _originalValue = originalValue;
    _currentValue = *originalValue;
    _isEditing = false;
}

void CrbNumberEditMenuItem::printLine2(Adafruit_RGBLCDShield *lcd) {
    lcd->setCursor(0,1);    
    if (_currentValue < 100) {
        lcd->print("0");
    }
    if (_currentValue < 10) {
        lcd->print("0");
    }
    lcd->print(_currentValue);
    
    lcd->print(" mins [Enter]");
}


void CrbNumberEditMenuItem::handleUpButton(CrbMenu *sender) {
    if (_isEditing) {
        _currentValue++; // Use the tag as our temporary editing value. This wraps after 255 via overflow (how nice)
        this->printLine2(sender->getLCD());
    }
}

void CrbNumberEditMenuItem::handleDownButton(CrbMenu *sender) {
    if (_isEditing) {
        _currentValue--;
        this->printLine2(sender->getLCD());
    }
}

void CrbNumberEditMenuItem::handleEnterButton(CrbMenu *sender) {
    _isEditing = !_isEditing;
    Adafruit_RGBLCDShield *lcd = sender->getLCD();
    if (_isEditing) {
        lcd->setCursor(2, 1); 
        lcd->blink();
    } else {
        lcd->noBlink();
        // Update the value
        *_originalValue = _currentValue;
    }
    CrbActionMenuItem::handleEnterButton(sender);
}
