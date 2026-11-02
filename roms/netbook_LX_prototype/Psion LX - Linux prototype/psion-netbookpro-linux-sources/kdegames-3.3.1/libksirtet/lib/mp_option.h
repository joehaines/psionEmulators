#ifndef MP_OPTION_H
#define MP_OPTION_H

#include <qwidget.h>

/** 
 * The OptionWidget is a base widget for the option widget in the
 * "netmeeting" dialog. This option widget is optional (!). 
 *
 * For example you will have :	 
 * <PRE>
 *  class MyOptionWidget : public OptionWidget { ... };
 *  class MyMultiPlayerInterface : public MultiPlayerInterface { ... };
 *   
 *  OptionWidget *MyMultiPlayerInterface::newOptionWidget(bool server) const
 *  { return new MyOptionWidget(server); };
 * </PRE>
 *
 * The option widget must have two different behaviours for server and
 * clients. The server is able to change the options but the clients are only
 * able to see them. The library will catch the @ref #changed signal from
 * the option widget and will send the changes to the clients. It uses the
 * @ref #dataOut to obtain the data from the
 * server option dialog and then on the client side, it sets the new data with
 * the method @ref #dataIn.
 * You must implement this three methods to have useful option widgets and be
 * careful to emit the signal @ref #changed whenever the server widget is
 * changed. In addition you'll need to implement the method @ref #saveData
 * to save the configuration in the config file ; so that it will be available
 * for the initialisation of @ref LocalBoard.
 * It seems a good idea that the widget have the same layout
 * on both (server and client) sides but with the inner widgets all disabled 
 * on the client side.
 */
class MPOptionWidget : public QWidget
{
 Q_OBJECT
		
 public:
	MPOptionWidget(bool Server, QWidget *parent = 0, const char *name = 0)
	: QWidget(parent, name), server(Server) {}
	virtual ~MPOptionWidget() {}

	bool isServer() const { return server; }
	
	/** 
	 * This method is used on the client side to set the data coming from
	 * the server widget.
	 */
	virtual void dataIn(QDataStream &s) = 0;
	
	/** This method is used on the server side to get the data. */
	virtual void dataOut(QDataStream &s) const = 0;

	/** 
	 * When the game will begin (ie when the "netmeeting" is over and the
	 * server has press the "start game" button) this method will be called
	 * (for clients and server). It must save the settings in the config
	 * file.
	 */
	virtual void saveData() = 0;
	
 signals:
	/** 
	 * This signal must be called each time options are changed 
	 * (by the server).
	 */
	void changed();
	
 private:
	bool server;
};

#endif // MP_OPTION_H
