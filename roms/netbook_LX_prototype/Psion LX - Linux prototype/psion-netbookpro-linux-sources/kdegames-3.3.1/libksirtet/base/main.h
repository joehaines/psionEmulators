#ifndef BASE_MAIN_H
#define BASE_MAIN_H

#include "kzoommainwindow.h"

class BaseInterface;
class KToggleAction;
class KKeyDialog;
class KConfigDialog;

class BaseMainWindow : public KZoomMainWindow
{
    Q_OBJECT
public:
    BaseMainWindow();
    virtual ~BaseMainWindow();

signals:
    void settingsChanged();

private slots:
    void start();
    void pause();
    void showHighscores();
    void configureSettings();
    void configureKeys();
    void configureHighscores();
    void configureNotifications();

protected:
    BaseInterface     *_inter;

    void buildGUI(QWidget *widget);
    virtual void addConfig(KConfigDialog *) {}
    virtual void addKeys(KKeyDialog &) {}
    virtual void saveKeys() {}
    
    virtual void writeZoomSetting(uint zoom);
    virtual uint readZoomSetting() const;
    virtual void writeMenubarVisibleSetting(bool visible);
    virtual bool menubarVisibleSetting() const;

private:
    KToggleAction *_pause;
};

#endif
