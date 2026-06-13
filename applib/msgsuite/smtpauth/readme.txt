SmtpAuth V 0.50
===============

SMTP Authenticating Proxy
-------------------------

Author: Marcus von Cube <marcus@mvcsys.de>

This is free software, do what you want with it but don't make a commercial
product from the sources. The software is provided as is. I'm not reliable
for any consequences arising from the usage of it.

This is work in progress at a very early stage!

What is SmtpAuth?
-----------------

As a measure against spam many mail providers recently switched to SMTP
authentication: their mail delivery systems require a user name and a
password to accept mail. (Others, like CompuServe or T-Online, require you to
use their own network to connect to the SMTP server.)

Some older email clients like the mail application on Psion handhelds
(running EPOC as their operating system) do not support the neccessary SMTP
protocol variant. Therfore there is a need to overcome this limitation.
SmtpAuth provides a solution based on the idea that it is enough to intercept
the traffic between the mail program and the server and add some handshaking
during the connection phase.

SmtpAuth is neither a mail client nor a mail server, it is a proxy with only
a limited amount of added functionality. And it has nothing to do with
retrieving mail, POP3 and IMAP4 are totally different from SMTP discussed
here!

How is it used?
---------------

You need to tell the proxy the connection details of your mail provider, that
is the IP name of the server, its port if not the standard value of 25, your
username and your password.

Your mail client now has to be configured to use "localhost" as the SMTP
server and a portnumber that you can freely select. Different port numbers
allow for more than one simultaneous configuration for different servers.
The configuration of SmtpAuth is stored in an ini file. The default name is
"SmtpAuth.ini" but that can be changed on the command line. On EPOC systems,
the file is in C:\System\Apps\SmtpAuth\SmtpAuth.ini. An example configuration
is automatically created:

[Global]
;
;  SMTP authentication
;
;  Change the following line to interface=all or any other ip address
;  if you want SmtpAuth to listen on other ports as well.
;  On EPOC this will cause a dialup request!
;
interface=localhost

[Server 1&1]
active=yes
listen-port=1025
server=smtp.1und1.com
port=25
user=user name
password=password

[Server isp2]
active=no
listen-port=1026
server=smtp.isp2.com
port=25
user=user name 2
password=password 2
;
;  add this only if you want POP before SMTP authentication!
;
;pop-server=pop.smtp2.com
;pop-port=110

As you can see, there is a POP server mentioned in the example: Some providers
allow access to their SMTP server only after the POP server has been contacted
with a valid user and password. You can do it yourself by opening the mailbox
before sending. SmtpAuth just does it for you, now!

Spelling and capitalization are significant. I'll provide a GUI configuration
option in a later release. Passwords will be encoded after the first run,
marked by the sequence "~=" instead of "=". Remove the "~" if you change
passwords!

Implementation
--------------

SmtpAuth is a Java application which can be used without modification on a
number of platforms. To start it from a command line you have to type:

java -cp SmtpAuth.jar de.mvcsys.smtpauth.SmtpAuthGui <inifile>

On a Java 2 system, the following should work as well:

java -jar SmtpAuth.jar <inifile>

<inifile> defaults to SmtpAuth.ini in the current directory. EpocUtil.jar
must be installed somewhere in your classpath (see below).

You can save some memory (and lose some comfort) if you start the class
de.mvcsys.epocutil.SmtpAuth (without "Gui"). Pressing any key terminates this
variant of the program.

On EPOC, things are easier because everything is installed in the right
places by the SIS installer and you'll find a nice icon on the extras bar :).
The command line can be found in a file named SmtpAuth.txt in the
installation directory. If you use the variant without GUI, it's neccessary
to create a dummy file \System\Java\Console on some drive to make the output
visible.

What is EpocUtil?
-----------------

This is a library for Java applications that tries to mimic some of the GUI
aspects of the EPOC operating system found on Psion handhelds. It works on
other platforms as well and is a neccessary part of SmtpAuth. On EPOC, it is
installed in \System\Java\ext\epocutil.jar and \System\libs\EpocUtil.dll. On
other systems, only the jar file is used and should be copied to the lib\ext
directory in the JRE path of the Java installation (Java 2).

If EpocUtil finds a file named "EpocUtil.properties" in the classpath, it
adds the contents to the system properties. This should help in overcoming
some localization problems especially on EPOC where a copy is automatically
installed in \System\Java\classes. Edit the file at your will.
