#ifndef MP_INTERFACE_H
#define MP_INTERFACE_H

#include <qwidget.h>
#include <qvaluelist.h>
#include <qptrlist.h>

#include "mp_board.h"
#include "mp_option.h"

class QHBoxLayout;
class Local;
class ConnectionData;
class RemoteHostData;
class KeyData;
class KeyCollection;
class KKeyDialog;
class KAction;

struct ActionData {
    const char *label, *name;
    const char *slot, *slotRelease; // if slotRelease!=0
                                    // : use keyPress/ReleaseEvent mecanism
};

/**
 * This structure contains information about the game
 * configuration.
 */
typedef struct {
	/** The game version id used for identification (e.g. "4").
	 *  You should change this id when the game is made incompatible
	 *  with previous version. (changes in data for example).
	 */
    const char *gameId;

	/** Maximum number of local players. */
	uint maxNbLocalPlayers;

	/** Interval (in msec.) between data exchange. */
	uint interval;

	/** If there are built-in artificial intelligences that can play. */
	bool AIAllowed;

	/** Slot for player/AI additional configuration. These must be SLOTs which
	 * take an "int" as parameter. It must open a setting
	 * dialog for the corresponding local player/computer and save the
	 * new settings in the config file. It should probably create a group
	 * with the given number in its name.
	 * If such a pointer is set to 0 : it means there is no perticular
	 * setting.
	 */
	 const char *humanSettingSlot, *AISettingSlot;
} MPGameInfo;

/**
 * The MPInterface class is useful for multiplayers game
 * management. Each game is represented by a class you have inherited
 * from the @ref MPBoard class.
 *
 * Multiplayers games can take place with several (humans or eventually
 * AIs) players on the same computer (they use the same keyboard and have
 * each a @ref MPBoard widget on the screen) or/and network players.
 *
 * This class is intended to do all the hard work of sending/receiving data
 * between the players and to send the keyboard events to the right
 * @ref MPBoard. So multiplayers game should be completely transparent
 * from your point of view.
 *
 * Note : The data exchange is done in background with a timer calling at given
 * intervals the read/write methods. Obviously this kind of things can be done
 * easily with threads but I have no experience with thread programming
 * and not all people have thread libraries and a thread-safe system.
 */
class MPInterface : public QWidget
{
 Q_OBJECT

 public:
	/** Constructor which takes a MPGameInfo struct as parameter.
     */
    MPInterface(const MPGameInfo &gameInfo,
                uint nbActions, const ActionData *data,
				QWidget *parent = 0, const char *name = 0);
	virtual ~MPInterface();

 public slots:
	/** Create a single player game for a human being.
	  * Call @ref stop if a game is already created. */
	void singleHuman()     { specialLocalGame(1, 0); }
	/** Create a local game opposing two human beings.
	  * Call @ref stop if a game is already created. */
	void humanVsHuman()    { specialLocalGame(2, 0); }
	/** Create a local game opposing a human with an AI.
	  * Call @ref stop if a game is already created. */
	void humanVsComputer() { specialLocalGame(1, 1); }

	/** Open a dialog to create a multiplayer game.
	  * Call @ref stop if a game is already created. */
	void dialog();

 public:
    virtual void addKeys(KKeyDialog &);
    void saveKeys();

	/** Called when a new game is created. At this point
	 *  the number of players is known. */
	virtual void init() {}

	/** Called just before a new game is created (called by
	  * singleHuman, humanVsHuman, humanVsComputer and dialog). */
	virtual void stop() {}

	/** Called when the start button of the netmeeting is pressed. */
	virtual void start() {}

	/**
     *  Set keys configuration for the given number of human players.
	 *  The size of	the array is the number of defined actions.
	 */
	void setDefaultKeycodes(uint nbHumans, uint human, const int *keycodes);

	/**
      * @return the total number of players.
	  * (If called from client : return the number of local boards).
	  */
	uint nbPlayers() const;

    /**
     * @return true if the interface is the server.
     */
    bool server() const { return _server; }

	/** @return the player name.
		Do not call from client !
	 */
	QString playerName(uint i) const;

	/**
	 * Create a new @ref MPBoard.
	 *
	 * @param i is the game index that goes from 0 to the number of
	 * local players : it can be used to retrieve configuration settings.
	 */
	virtual MPBoard *newBoard(uint i) = 0;

	/**
	 * This method must read data from each client with method
	 * @ref readingStream, do the needed treatement
	 * (for instance which players has lost, which data to be resent, ...) and
	 * then write the useful data to each client with method
	 * @ref writingStream.
	 *
	 * NB: this method is also called for single player games but
	 * you probably only want to check for game over condition (it depends
	 * on game implementation that you really return data to the board).
	 */
	virtual void treatData() = 0;

	/** @return the reading stream for board #i.
	  * Do not call from client !
	  */
	QDataStream &readingStream(uint i) const;

	/** @return the writing stream for board #i.
	  */
	QDataStream &writingStream(uint i) const;

	/**
	 * Read data sent from server to clients "MultiplayersInterface"
	 * (this data is not addressed to boards).
	 * These are meta data that are not directly used in game.
	 * It can be used to display "game over" infos for all
	 * local games.
	 * NB: the use of this method is optional.
	 */
	virtual void dataFromServer(QDataStream &) {}

	/** Used by the server to write meta data to clients.
	 *  NB: the use of this method is optional.
	 *  Do not call from client !
	 */
	QDataStream &dataToClientsStream() const;

	/** Write immediately data to clients and local boards.
	  * It is unlike the normal exchange which is driven
	  * by the timer of the server. Be aware of possible
	  * interactions.
	  */
	void immediateWrite();

	/**
	 * This method should be overload if an option widget is used in the
	 * the "netmeeting" dialog). By default a
	 * null pointer is returned and so no option widget is shown.
	 * The option widget must be inherited from the @ref MPOptionWidget class.
	 */
	virtual MPOptionWidget *newOptionWidget() const { return 0; };

	/** Called when a network error occurred or when a host gets disconnected.
	 *  The default implementation displays a message and calls singleHumans()
	 * 	ie it stops the current game. By overloading this method, it is
	 *  possible to continue the game at this point with the remaining players.
	 */
	virtual void hostDisconnected(uint i, const QString &msg);

 protected:
	void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);

 private slots:
	void enableKeys(bool enable);
	void singleHumanSlot();

 public:
	class Data	{
	public:
		Data() {}
		MPBoard *ptr;
		int      humanIndex;
		QString  name;
	};

  private:
	Local            *internal;
	const MPGameInfo  gameInfo;
	QValueList<Data>  boards;
	uint              nbLocalHumans;
	QHBoxLayout      *hbl;
	bool              _server, disconnected;

    KeyData                    *_keyData;
	QMemArray<KeyCollection *>  _keyCol;

	void createServerGame(const QPtrList<RemoteHostData> &);
	void createClientGame(const RemoteHostData &);
	void createLocalGame(const ConnectionData &);
	void specialLocalGame(uint nbHumans, uint nbComputers);

	void clear();
	void initKeys(uint nbHumans);
};

#endif // MP_INTERFACE_H
