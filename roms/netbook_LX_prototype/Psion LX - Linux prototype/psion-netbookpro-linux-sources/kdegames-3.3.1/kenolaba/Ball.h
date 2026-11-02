/* Class Ball, BallWidget
 *
 * Online rendered balls with caching + animation widget
 *
 * Supported static effects
 *  - ball color
 *  - ripple texture
 *
 * Supported animation sequences for now:
 *  - Color Blending
 *  - Texture rotate
 *
 * April 1999, Josef Weidendorfer
 */

#ifndef _BALL_H_
#define _BALL_H_

#include <qpixmap.h>
#include <qimage.h>
#include <qcolor.h>
#include <qwidget.h>
#include <qptrlist.h>

/* textures for balls */
#define TEX_FLAT   0
#define TEX_RIPPLE 1

class Ball {
  
 public:
  Ball(const QColor& c, double a = 0.0, int t=TEX_RIPPLE );
  ~Ball();

  QPixmap* pixmap();

  double angle() { return an; }
  QColor ballColor() { return bColor; }
  void setSpecials(double z, double f, double l)
    { zoom = z, flip=f, limit=l; }

  static int w() { return sizeX; }
  static int h() { return sizeY; }
  static void setSize(int x,int y);
  static void setLight(int x=5, int y=3, int z=10, 
		       const QColor& c = QColor(200,230,255) );
  static void setTexture(double c=13., double d=.2);

 private:

  void render();
  static void invalidate();

  //static QImage back;
  static int sizeX, sizeY;
  static double lightX, lightY, lightZ;
  static QColor lightColor;
  static double rippleCount, rippleDepth;

  QPixmap pm;
  QColor bColor;
  double an, sina, cosa;
  double zoom, flip, limit;
  int tex;

  Ball *next;
  static Ball* first;
};


class BallAnimation {
 public:
  BallAnimation(int s, Ball*, Ball*);

  int steps;
  QPtrList<Ball> balls;
};

#define ANIMATION_STOPPED 0
#define ANIMATION_FORWARD 1
#define ANIMATION_BACK    2
#define ANIMATION_LOOP    3
#define ANIMATION_CYCLE   4

class BallPosition {
 public:
  BallPosition(int xp,int yp, Ball* d);

  int x, y, actStep, actDir, actType;
  Ball* def;
  BallAnimation* actAnimation;
};

#define MAX_POSITION  130
#define MAX_ANIMATION  20

class BallWidget : public QWidget
{
  Q_OBJECT

 public:
  BallWidget(int _freq, int bFr, QWidget *parent = 0, const char *name = 0);
  ~BallWidget();

  void createBlending(int, int, Ball* , Ball* );
  void createBallPosition(int, int x, int y, Ball*);
  
  void startAnimation(int pos, int anim, int type=ANIMATION_FORWARD);
  void stopAnimation(int pos);

  void paint(QPaintDevice *);
  
  virtual void resizeEvent(QResizeEvent *);
  virtual void paintEvent(QPaintEvent *);

 signals:
  void animationFinished(int);
  void animationsFinished(void);

 protected:
  void drawBackground();

 private slots:
  void animate();

 protected:
  QMemArray<BallPosition*> positions;
  QMemArray<BallAnimation*> animations;

 private:
  int freq;
  int xStart, yStart, realSize, ballFraction;
  bool isRunning;
  QTimer *timer;
};


/* Ball Test */

class BallTest: public BallWidget
{
  Q_OBJECT
public:
  BallTest(QWidget *parent=0, const char *name=0 );
protected:
  void mousePressEvent( QMouseEvent * );
  void mouseReleaseEvent( QMouseEvent * );


};




#endif // _BALL_H_  
