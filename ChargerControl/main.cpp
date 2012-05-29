//
//  main.cpp
//  Main file
//  ----------------------------------
//  Developed with embedXcode
//
//  Project ChargerControl
//  Created by corbin dunn on 5/18/12 
//  Copyright (c) 2012 __MyCompanyName__
//

// Core library
#include  "Arduino.h" // — for Arduino 1.0


// Sketch
#include "ChargerControl.pde"

int main(void)
{
    init();
    setup();

    
    for (;;) loop();
    return 0;
}
