#ifndef MP_BOARD_H
#define MP_BOARD_H

#include <qwidget.h>

/**
 * The MP_Board class is the base widget from which each individual
 * board should inheritate ; you must implement its virtual methods.
 */
class MPBoard : public QWidget
{
 Q_OBJECT

 public:
    MPBoard(QWidget *parent, const char *name=0)
	: QWidget(parent, name) {}
	virtual ~MPBoard() {}

	/**
	 * This method is called once at the board creation.
	 * @param AI is TRUE if the player is not human.
	 * @param multiplayers is TRUE if the game is not a single player game.
	 * @param first is TRUE if this board is the first local one.
	 */
	virtual void init(bool AI, bool multiplayers, bool server, bool first,
					  const QString &name) = 0;

	/**
	 * Put data on the stream.
	 *
	 * This method is the communication way out. The data given here will
	 * be the only information that will go to the server.
	 */
	virtual void dataOut(QDataStream &) = 0;

	/**
	 * Get data from the stream.
	 *
	 * This method is the communication way in. The data given here will be
	 * the only information that you will receive from the server.
	 */
	virtual void dataIn(QDataStream &) = 0;

 signals:
	/**
	 * Call this signal to enable/disable the keys associated with a board.
	 */
	void enableKeys(bool);
};

#endif // MP_BOARD_H
