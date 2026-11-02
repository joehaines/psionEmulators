/*
 * LX Sync Server
 *
 * Provides synchronization service for the Psion LX.
 *
 * The LX side is the client, listening and servicing requests as received.
 * The application specific logic is all placed on the server side,
 * minimizing the complexity and footprint of the client.
 *
 * The support commands are as follows:
 * T<file>                 to send named file
 *		 <size><file contents is sent>
 *
 * S<file><sql statement> for sql commands
 *     fields in responses are string delimited
 *
 * R<file><size>          for receiving named file
 *     file of <size> is sent over the pipe
 *
 * Q								terminate the session
 *									send from Windows when finished
 *
 * D<path>                 recursive listing of files from path down
 *     <filename><modify time><md5 hash>
 *
 * C<command>              a command line request
 *     <shell output>
 *
 * fields are string delimited (terminated with '\0')
 * transmissions are terminated with an empty field (two NULLs)
 * size (in bytes) is used to determine length of read for binary data
 *
 * All PIM information is synchronized using SQL commands
 * File sync is done using commands above
 * Email sync is built on file sync using maildir format
 * Backups use file sync as well
 *		backup file will be pushed to the server after it is created in
 *		backup dir on the lx - the server will trigger deletion by copying
 *		the file to the backups directory
 *		files will be recovered by placing them back in the server backup 
 *		dir when required this will trigger transport to the lx and deletion
 *		from that dir after they have been used
 *	Software request use file sync as well
 *		request file is place in lx software queue
 *		sync moves this file to the server
 *		server processes the request file and deletes it, adding the software
 *		package from the web
 *		package is sent to lx on next sync
 *
 *
 * History:
 *
 */
#define DEBUG 1

#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>

char buffer[5001];

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, clilen;
	struct sockaddr_in serv_addr, cli_addr;
	int n;

	// check for port to use
	if (argc < 2) 
		portno = 1101;
	else
		portno = atoi(argv[1]);

	// obtaint the requested socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	  error("ERROR opening socket");

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		  error("ERROR on binding");

	// fullfill requests forever
	while(1)
	{
		// prepare to accept connections
		listen(sockfd,5);
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0 && DEBUG) 
			 fprintf(stderr, "ERROR on accept");

		// service a connection as long as it is open
		while( lx_parse( newsockfd ) )
			;

		close(newsockfd);
	}
}

int fd;

int	lx_parse( int lfd )
{
	int	sop = 1; // socket open flag
	int	n;

	fd = lfd;

	bzero(buffer,sizeof(buffer));

	n = read(fd,buffer,sizeof(buffer)-1);
	if (n < 0)
	{
		perror( "read in lx_parse" );
		fprintf( stderr, "ERROR reading from socket");
		return(0);
	}
	if (n == 0)
	{
		// fprintf( stderr, "need to sleep\n");
		// sleep(1);
		return(0);
	}
	buffer[n] = '\0';

	if( DEBUG )
		printf("Here is the message: %s\n",buffer);

	// parse the request
	switch( buffer[0] )
	{
	case 'T': // a file transmission
		sop = send_file(fd, &buffer[1]);
		break;

	case 'S': // an sql query
		sop = sql_query(fd, &buffer[1]);
		break;

	case 'R': // a file reception
		sop = recv_file(fd, &buffer[1]);
		break;

	case 'Q': // all done
		exit(0);
		break;

	case 'D': // a directory listing
		sop = scan_path(fd, &buffer[1]);
		break;

	case 'C': // a command line request
		sop = cmd_line(fd, &buffer[1]);
		break;

	default:
		sop = cmd_line(fd, buffer);
		break;
	}
	return( sop );
}

int cmd_line(int fd, char *cmd)
{
	FILE	*f_p;
	int	sop = 1;
	int	n, p;
	int	req_len = sizeof(buffer)-2;

	f_p = popen( cmd, "r" );
	if( f_p != NULL )
	{
		while( 1 )
		{
			n = fread( buffer, 1, req_len, f_p);

			if( DEBUG )
				printf("Writing: %s\n",buffer);

			if( write(fd,buffer,n) < 0 )
			{
				fprintf( stderr, "error on write");
				sop = 0;
				break;
			}
		
			// finish writing when less than full buffer
			if( n != req_len )
			{
				n = 0;
				buffer[n++] = '\0';
				buffer[n++] = '\0';
				write(fd,buffer,n);
				break;
			}
		}
		fclose( f_p );
	}
	else
	{	
		perror("can't open");
		sop = 0;
	}
	return(sop);
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
	char	buff[10];
	char	*t;
	int	n;

	memset( buff, 0, sizeof(buff) );

	// return data through the socket in the form of each record as a string
	for(i=0; i<argc; i++)
	{
	  if( argv[i] != NULL )
		  t = argv[i];
		else
			t = "NULL";

		// push the record including the '\0'
		n = write(fd, t, strlen(t)+1);
		if( DEBUG )
			printf("%s\n", t );
	}
	
	// do line delimiting
	// n = 0;
	// buff[n++] = '\n';
	// buff[n++] = '\0';
	// n = write( fd, buff, n);

	return 0;
}

int sql_query(int fd, char *cmd)
{
	FILE	*f_p;
	int	sop = 1;
	int	n;
	int	i;
	char	*pDb, *pSql;

	sqlite *db;
	char *zErrMsg = 0;
	int rc;

	// parse the input
	pDb = cmd;
	pSql = cmd;
	pSql += strlen(cmd);
	pSql++;

	if( DEBUG )
		printf("Opening %s\n%s\n", pDb, pSql );

	db = sqlite_open( pDb, 1, &zErrMsg );
	if( db == NULL )
	{
		fprintf(stderr, "Can't open database: %s\n", pDb );
		sqlite_close(db);
		return(0);
	}

	rc = sqlite_exec(db, pSql, callback, 0, &zErrMsg);
	
	// signify the end of transmission
	write( fd, "\0", 1 );
	// write( fd, "\0", 1 );

	if( rc!=SQLITE_OK )
	{
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sop = 0;
	}
	sqlite_close(db);

	return(sop);
}

int send_file(int fd, char *cmd)
{
	long	i, s;
	int	n;
	char	*pName, *pSize;
	FILE	*f_p;
	static char buffer[200];
	int sop = 1;

	// parse the input
	pName = cmd;

	if( DEBUG )
		printf("Tx File:%s\n", pName );
	
	if( (f_p = fopen( pName, "r" )) == NULL )
	{
		perror("Tx File");
		return(0);
	}

	while( 1 )
	{
		n = fread( buffer, 1, n, f_p);

		if( DEBUG )
			printf("got:%d\n", n );

		if( n > 0 )
		{
			write( fd, buffer, n );
			i += n;
		}
		else if( n == 0 )
		{
			break;
		}
		else
		{
			perror("Tx File Read");
			sop = 0;
			break;
		}
	}
	if( DEBUG )
		printf("Transmission Complete\n");
	fclose( f_p );

	// signify the end of transmission
	write( fd, "\0", 1 );
	write( fd, "\0", 1 );

	return(sop);
}

int recv_file(int fd, char *cmd)
{
	long	i, s;
	int	n;
	char	*pName, *pSize;
	FILE	*f_p;
	static char buffer[200];
	int sop = 1;

	// parse the input
	pName = cmd;
	pSize = cmd;
	pSize += strlen(cmd);
	pSize++;
	s = atol(pSize);

	if( DEBUG )
		printf("Rx File;%s of %ld bytes\n", pName, s );
	
	if( (f_p = fopen( pName, "w" )) == NULL )
	{
		perror("Rx File");
		return(0);
	}

	for( i=0; i<s; )
	{
		if( s - i > sizeof(buffer) )
			n = sizeof(buffer);
		else
			n = s - i;

		n = read( fd, buffer, n );

		if( DEBUG )
			printf("got:%d\n", n );

		if( n > 0 )
		{
			fwrite( buffer, 1, n, f_p);
			i += n;
		}
		else
		{
			perror("Rx File Data");
			sop = 0;
			break;
		}
	}
	if( DEBUG )
		printf("Transmission Complete\n");
	fclose( f_p );

	// signify the end of transmission
	write( fd, "\0", 1 );
	write( fd, "\0", 1 );

	return(sop);
}


int scan_path(int fd, char *cmd)
{
	if( DEBUG )
		printf("Directory Listing of %s\n", cmd );

	file_scan( cmd );
	
	// signify the end of transmission
	write( fd, "\0", 1 );

	return(0);
}

int file_scan( char *path )
{
	struct dirent *entry;
	DIR *dir;
	struct stat dstat;
	char	fn[200];
	char	t[50];

	if( (dir = opendir(path)) == NULL) 
	{
		fprintf(stderr, "Could not open:%s\n", path);
		return(0);	/* could not open the dir */
	}

	// loop through the directory
	while ((entry = readdir(dir)) != NULL) 
	{
		if( (strcmp(entry->d_name, ".") != 0 ) &&
		 (strcmp(entry->d_name, "..") != 0 ) &&
		 (strlen(path)+strlen(entry->d_name)+3 < sizeof(fn)) )
		{
			strcpy(fn, path );
			if( fn[strlen(fn)-1] != '/' )
				strcat( fn, "/" );
			strcat(fn, entry->d_name );

			if( stat( fn, &dstat) == 0 )
			{
				if( S_ISREG( dstat.st_mode) )
				{
					write( fd, fn, strlen(fn)+1 );

					sprintf( t, "%ld", dstat.st_mtime );
					write( fd, t, strlen(t)+1 );

					sprintf( t, "%ld", dstat.st_size );
					write( fd, t, strlen(t)+1 );
				}
				else if( S_ISDIR( dstat.st_mode) )
				{
					strcat( fn, "/" );
					file_scan( fn );
				}
			}
		}
	}
	closedir(dir);

	return(0);
}
