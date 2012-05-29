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


CrbMenuItem::CrbMenuItem(const char *name) : _name(name) {
    _parent = _prior = _next = _firstChild = 0;
}

CrbMenuItem *CrbMenuItem::addNextWithName(const char *name) {
    CrbMenuItem *result = new CrbMenuItem(name);
    this->addNext(result);
    return result;
}

CrbMenuItem *CrbMenuItem::addChildWithName(const char *name) {
    CrbMenuItem *result = new CrbMenuItem(name);
    this->addChild(result);
    return result;
}

void CrbMenuItem::addNext(CrbMenuItem *next) {
    CrbMenuItem *walker = this;
    // Fine the first item without a next
    while (walker->_next) {
        walker = walker->_next;
    }
    // doubly linked list forwards/backwards from that last child
    walker->_next = next;
    next->_prior = walker;
    next->_parent = _parent; // we have the same parent
}

void CrbMenuItem::addChild(CrbMenuItem *child) {
    if (_firstChild == NULL) {
        child->_parent = this;
        _firstChild = child;
    } else {
        // Otherwise, add a next to the child to put it at the end of the children list for this item
        _firstChild->addNext(child);
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
            _lcd->print("< ");
            _lcd->print(_currentItem->getName());
            _lcd->print("> ");
            
            if (_currentItem->getNext()) {
                _lcd->setCursor(0,1);
                _lcd->print("  ");
                _lcd->print(_currentItem->getNext()->getName());
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
    if (_currentItem->getFirstChild()) {
        _currentItem = _currentItem->getFirstChild();
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
        //            menu.use();
        //            lcd.print("SELECT ");            
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
