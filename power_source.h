#ifndef _POWER_SOURCE_H_
#define _POWER_SOURCE_H_

class game;

class cPowerComponent
{
protected:
    int xPos;
    int yPos;
public:
    cPowerComponent(){}
    ~cPowerComponent(){}

    virtual void update() = 0;
    bool getIsPresentAtPoint(int x, int y)
    {
        return ( xPos == x && yPos == y);
    };
};

class cPowerSource : public cPowerComponent
{
    bool bActive;
    float maxFuel;
    float currentFuel;
    float consumptionRate;
    float PowerUnitsPerFuel;
    game* pGame;
public:
    cPowerSource(game *g, int x, int y, float maxfuel, float currentfuel, float consumption, float pu_per_fuel);
    ~cPowerSource(){}

    void update();
    void activate();
};

#endif
