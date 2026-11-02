#ifndef FE_AI_H
#define FE_AI_H

#include "common/ai.h"


class FEAI : public AI
{
 Q_OBJECT

 public:
    FEAI();

 private:
    static const Data DATA[];

    static double nbPuyos(const Board &, const Board &);
    static double nbChained(const Board &, const Board &);
};

#endif
