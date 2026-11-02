#ifndef COMMON_FACTORY_H
#define COMMON_FACTORY_H

#include "base/factory.h"


struct CommonBoardInfo {
    uint baseTime, dropDownTime, beforeGlueTime, afterGlueTime;
    uint afterGiftTime, nbBumpStages;
    uint nbRemovedToLevel;
    uint nbGiftLeds, maxGiftsToSend, giftShowerTimeout, giftPoolTimeout;
};

class BaseField;
class AI;

#define cfactory static_cast<CommonFactory *>(BaseFactory::self())

class CommonFactory : public BaseFactory
{
 public:
    CommonFactory(const MainData &, const BaseBoardInfo &,
                  const CommonBoardInfo &);

    const CommonBoardInfo &cbi;

    virtual BaseField *createField(QWidget *parent) = 0;
    virtual AI *createAI() = 0;

    QWidget *createAIConfig();
    virtual QWidget *createAppearanceConfig();
    virtual QWidget *createGameConfig();
};

#endif
