#ifndef BASE_INTER_H
#define BASE_INTER_H

class QWidget;


class BaseInterface
{
public:
    BaseInterface() {}
    virtual ~BaseInterface() {}

    virtual void _start() = 0;
    virtual void _pause() = 0;
    virtual bool _isPaused() const = 0;

    void showHighscores(QWidget *parent);

protected:
    virtual void _showHighscores(QWidget *parent);
};

#endif
