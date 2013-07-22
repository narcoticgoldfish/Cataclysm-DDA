#include "power_source.h"
#include "game.h"

cPowerSource::cPowerSource(game *g, int x, int y, float maxfuel, float currentfuel, float consumption, float pu_per_fuel)
{
    pGame = g;
    maxFuel = maxfuel;
    currentFuel = currentfuel;
    consumptionRate = consumption;
    PowerUnitsPerFuel = pu_per_fuel;
    xPos = x;
    yPos = y;
    bActive = false;
}

void cPowerSource::update()
{
    //if the power source is turned on
    if(bActive)
    {
        //consume a fuel unit
        currentFuel -= consumptionRate;

        if(currentFuel <= 0.0f)
        {
            currentFuel = 0.0f;
            pGame->add_msg("The generator sputters out!");
            bActive = false;
        }
    }

}

void cPowerSource::activate()
{
    if(bActive)
    {
        pGame->add_msg("You turn the generator off.");
        bActive = false;
    }
    else
    {
        if(currentFuel <= 0.0f)
            pGame->add_msg("The generator has no fuel.");
        else
        {
            pGame->add_msg("You turn the generator on.");
            bActive = true;
        }
    }
}
