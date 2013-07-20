#include "power_source.h"

power_source::power_source(int maxfuel, int currentfuel, int x, int y)
{
    max_fuel = maxfuel;
    current_fuel = currentfuel;
    xPos = x;
    yPos = y;
}

bool power_source::getIsPresentAtPoint(int x, int y)
{
    if (xPos == x && yPos == y)
        return true;
    return false;
}
