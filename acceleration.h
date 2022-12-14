#ifndef __ACCELERATION_H__
#define __ACCELERATION_H__
#include "mvmt.h"

class Acceleration : public Mvmt {
    int ddY, ddX;
  public:
    Acceleration(GameObj *obj, int ddY, int ddX);
    void affect() override;
};

#endif

