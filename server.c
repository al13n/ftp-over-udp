/* include udpservselect01 */
#include	"unp.h"
#define MAXDGRAMCONTENT 4096
const int dataMaxLen = 512 - 5*(sizeof(int));

typedef struct dgram
{
	int seqNum;
	int ack;
	int eof;
	int windowSize;
	int dataLen;
	char data[512 - 5*(sizeof(int))];
};

struct dgram fileContent[MAXDGRAMCONTENT];

main(int argc, char **argv)
{
	int					listenfd, connfd, udpfd, nready, maxfdp1;
	//char				mesg[MAXLINE];
	pid_t				childpid;
	fd_set				rset;
	ssize_t				n;
	socklen_t			len;
	const int			on = 1;
	struct sockaddr_in	cliaddr, servaddr;
	void				sig_chld(int);

		/* 4create listening TCP socket */
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(SERV_PORT);

	Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	Bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

	Listen(listenfd, LISTENQ);

		/* 4create UDP socket */
	udpfd = Socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(SERV_PORT);

	Bind(udpfd, (SA *) &servaddr, sizeof(servaddr));
/* end udpservselect01 */

/* include udpservselect02 */
	//Signal(SIGCHLD, sig_chld);	/* must call waitpid() */

	FD_ZERO(&rset);
	maxfdp1 = max(listenfd, udpfd) + 1;
	for ( ; ; ) {
		FD_SET(listenfd, &rset);
		FD_SET(udpfd, &rset);
		if ( (nready = select(maxfdp1, &rset, NULL, NULL, NULL)) < 0) {
			if (errno == EINTR)
				continue;		/* back to for() */
			else
				err_sys("select error");
		}

		if (FD_ISSET(listenfd, &rset)) {
			len = sizeof(cliaddr);
			connfd = Accept(listenfd, (SA *) &cliaddr, &len);
	
			if ( (childpid = Fork()) == 0) {	/* child process */
				Close(listenfd);	/* close listening socket */
				str_echo(connfd);	/* process the request */
				exit(0);
			}
			Close(connfd);			/* parent closes connected socket */
		}

		if (FD_ISSET(udpfd, &rset)) {
			len = sizeof(cliaddr);
			//struct dgram mesg;
			//n = Recvfrom(udpfd, &mesg, MAXLINE, 0, (SA *) &cliaddr, &len);
			//printf("(%d)\n",mesg.x );
			//mesg.x = 3;
			//Sendto(udpfd, (void *)&mesg, n, 0, (SA *) &cliaddr, len);
		}
	}
}
/* end udpservselect02 */



int readFileContents(char *fileName, int windowSize){
	FILE *fp = fopen(fileName, "r");
    int seq=0, i;

    if(fp == NULL){
    	// file does not exist
    	return 0;
    }

    while(1)
    {	
    	struct dgram packet;
        char buff[dataMaxLen];
        int eof = 0;

        for(i = 0; i < dataMaxLen; i++)
        {
            int c = fgetc(fp);
            if(c == EOF)
            {
                eof = 1;
                break;
            }
            buff[i] = c;
        }
        buff[i] = '\0';
        
        // Assign contents for a particular packet
        
        packet.seqNum = seq++;
        packet.windowSize = windowSize;
        strcpy(packet.data,buff);
        packet.dataLen = i;
        packet.eof = 0;
        fileContent[seq-1] = packet;

        if(eof){
            packet.eof=1;
            break;
        }
    }
    return seq;
}


