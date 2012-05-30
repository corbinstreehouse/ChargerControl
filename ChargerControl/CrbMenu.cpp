//
//  CrbMenu.cpp
//  ChargerControl
//
//  Created by corbin dunn on 5/28/12 .
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "CrbMenu.h"
#include "Arduino.h"
#include <Wire.h>


CrbMenuItem::CrbMenuItem(const char *name, CrbMenuItemAction action) : _name(name) {
    _parent = _priorSibling = _nextSibling = _child = 0;
    _options = 0;
    _tag = 0;
    _action = action;
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
            // Print the parent item that we are on
            if (_currentItem->hasOption(CrbMenuItemOptionSelected)) {
                _lcd->print("*");
            }
            _lcd->print(_currentItem->getName());
            
            _lcd->setCursor(0,1);
            if (_currentItem->getPrior() && _currentItem->getNext()) {
                if (_currentItem->getChild()) {
                    _lcd->print("       [Up/Down]");
                } else {
                    // This is a menu item to choose
                    _lcd->print(" [Up/Down|Enter]");
                }
            } else if (_currentItem->getPrior()) {
                if (_currentItem->getChild()) {
                    _lcd->print("            [Up]");
                } else {
                    _lcd->print("      [Up|Enter]");
                }
            } else if (_currentItem->getNext()) {
                if (_currentItem->getChild()) {
                    _lcd->print("          [Down]");
                } else {
                    _lcd->print("    [Down|Enter]");
                }
            }
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

void CrbMenu::handleButton(uint8_t buttons) {
#if DEBUG_MENU
    Serial.println("button pressed");
#endif
    
    if (buttons & BUTTON_UP) {
        this->gotoPriorSibling();
    } else if (buttons & BUTTON_DOWN) {
        this->gotoNextSibling();
    } else if (buttons & BUTTON_LEFT) {
        this->gotoParent();
    } else if (buttons & BUTTON_RIGHT) {
        this->gotoFirstChild();
    } else if (buttons & BUTTON_SELECT) {
        // Prefer to fire the action
        if (_currentItem->getAction()) {
            _currentItem->getAction()(_currentItem);
        } else {
            // Use it as an alias to goto the right
            this->gotoFirstChild();
        }
    }
}

void CrbMenu::loopOnce() {
    uint8_t buttons = _lcd->readButtons();
    if (buttons) {
        this->handleButton(buttons);
        // Wait until the button is up
        while (_lcd->readButtons()) {
            // nop
        }
    }
}
