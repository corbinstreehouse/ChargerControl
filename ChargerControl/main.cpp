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

int main(void)
{
    init(); // implemented in ChargerControl.cpp
    setup();

    
    for (;;) loop();
    return 0;
}
