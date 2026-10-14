#pragma once

#include "Core/Globals/_types.h"

struct OBJECT;
void RegisterBuff(eBuffState buff, OBJECT* object, int remainingSeconds);
void UnRegisterBuff(eBuffState buff, OBJECT* object);
