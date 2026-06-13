

#include "NWInterface.h"






void CNWInterface::Send(TDesC& aPacket)
	{
	/* function	 Send sends the descriptor aPacket over the network implementation
	 *			 or in debug mode writes it to the file
	 */


	Write(aPacket);
	}//end function send(TDesC&)
	
void CNWInterface::Send()
	{
	/* function	 Send sends the descriptor aPacket over the network implementation
	 *			 or in debug mode writes it to the file
	 */

	// Never send empty packet
	if (iBuf.Length() > 0)
		{
		Write(iBuf);
		}
	}//end function Send()


void CNWInterface::Receive()
	{
	/* function  Receive, receives a packet from the network
	 *			 or in debug mode reads it from the file
	 */

	Read();
	}// end function Receive


TDesC& CNWInterface::GetBuffer()
	{
	/* function  GetBuffer returns a pointer to the locally stored buffer
	 */
	return iBuf;
	}// end function GetBuffer

void CNWInterface::SetBuffer(const TDesC& aStream)
	{
	/* function  SetBuffer sets the local buffer
	 * parameter aStream the stream to be stored into the buffer
	 */

	iBuf.Copy(aStream);
	}// end function SetBuffer

void CNWInterface::AppendBuffer(const TDesC& aStream)
	{
	/* function  AppendBuffer appends data to the local buffer
	 * parameter aStream the descriptor which is appended
	 */

	iBuf.Append(aStream);
	}// end function AppendBuffer