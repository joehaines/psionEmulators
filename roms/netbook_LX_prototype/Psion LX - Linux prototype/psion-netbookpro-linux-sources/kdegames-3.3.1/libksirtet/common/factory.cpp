#include "factory.h"

#include "ai.h"
#include "settings.h"


CommonFactory::CommonFactory(const MainData &md, const BaseBoardInfo &bbi,
                             const CommonBoardInfo &ci)
    : BaseFactory(md, bbi), cbi(ci)
{}

QWidget *CommonFactory::createAppearanceConfig()
{
    return new AppearanceConfig;
}

QWidget *CommonFactory::createGameConfig()
{
    return new GameConfig;
}

QWidget *CommonFactory::createAIConfig()
{
    AI *ai = createAI();
    QWidget *cw = new AIConfig(ai->elements());
    delete ai;
    return cw;
}
