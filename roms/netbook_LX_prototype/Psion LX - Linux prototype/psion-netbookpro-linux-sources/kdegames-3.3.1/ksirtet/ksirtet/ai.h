#ifndef KS_AI_H
#define KS_AI_H

#include "common/ai.h"


class KSAI : public AI
{
    Q_OBJECT
 public:
    KSAI();

 private:
    static const Data DATA[];
};

#endif
