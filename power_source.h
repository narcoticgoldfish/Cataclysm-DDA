#ifndef _POWER_SOURCE_H_
#define _POWER_SOURCE_H_

class power_source
{
    int xPos;
    int yPos;
    int max_fuel;
    int current_fuel;
public:
    power_source(int maxfuel, int currentfuel, int x, int y);
    ~power_source(){}

    bool getIsPresentAtPoint(int x, int y);
};

#endif
