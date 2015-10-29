/* include udpservselect01 */
#include	"unp.h"
#include	"unprtt.h"
#include	<setjmp.h>
#define MAXDGRAMCONTENT 4096
#define MAXDUPS 3
const int dataMaxLen = 512 - 5*(sizeof(int));

int readFileContents(char *, int );
static void	sig_alrm(int signo);

typedef struct dgram
{
	int seqNum;
	int ack;
	int eof;
	int windowsize;
	int dataLen;
	char data[512 - 5*(sizeof(int))];
};

struct dgram fileContent[MAXDGRAMCONTENT];

static sigjmp_buf	jmpbuf;
static struct rtt_info   rttinfo;
static int	rttinit = 0;

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

	typedef struct  dgram
	{
		int x ;
		int y ;
	}mesg;

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
		FD_ZERO(&rset);
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
			printf("HERE\n");
			len = sizeof(cliaddr);
			struct dgram mesg;
			n = Recvfrom(udpfd, &mesg, MAXLINE, 0, (SA *) &cliaddr, &len);
			//printf("(%d)\n",mesg.x );
			//mesg.x = 3;
			//Sendto(udpfd, (void *)&mesg, n, 0, (SA *) &cliaddr, len);
			char *filename = "testfile";
			int cnt = readFileContents(filename, 10);
			printf("%d\n", cnt);
			//sendFileContents(udpfd, (SA *)cliaddr, len, cnt, windowsize);
			//Close(udpfd);	
			//break;
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

        printf("%s\n", packet.data);
        if(eof){
            packet.eof=1;
            break;
        }
    }
    return seq;
}

int sendFileContents(int sockfd, struct sockaddr_in cliaddr, int len, int totalblocks, int windowsize){

	int first_unacknowledged_pos = 0;
	int pos_sent = -1; 
	struct dgram recv_packet;
	int dups = 0;
	int recv_ack = -1;
	int retransmit = 0;
	int rtt_measured_packet = 0;

	Signal(SIGALRM, sig_alrm);
	if (rttinit == 0) {
		rtt_init(&rttinfo);		/* first time we're called */
		rttinit = 1;
		rtt_d_flag = 1;
	}

	while(1){

		sendAgainFirstUnackPos:
		if(retransmit)
		{	// will serve also as a probe
			Sendto(sockfd, (void *)&fileContent[first_unacknowledged_pos], sizeof(dgram), 0, (SA *) &cliaddr, len);
		}

		if (sigsetjmp(jmpbuf, 1) != 0) {
			if (rtt_timeout(&rttinfo) < 0) {
				err_msg("dg_send_recv: no response from server, giving up");
				rttinit = 0;	/* reinit in case we're called again */
				errno = ETIMEDOUT;
				return(-1);
			}
			#ifdef	RTT_DEBUG
				err_msg("dg_send_recv: timeout, retransmitting");
			#endif
			retransmit = 1;
			goto sendAgainFirstUnackPos;
		}

		int firstPacketSent = 1;
		while(pos_sent < totalblocks && pos_sent + 1 < first_unacknowledged_pos + windowsize){
			if(firstPacketSent){
				firstPacketSent = 0;
				alarm(rtt_start(&rtt_info));
				rtt_measured_packet = pos_sent+1;
			}
			Sendto(sockfd, (void *)&fileContent[pos_sent+1], sizeof(dgram), 0, (SA *) &cliaddr, len);
			pos_sent++;
		}

		if(Recvfrom(sockfd, &recv_packet, sizeof(dgram), 0, (SA *) &cliaddr, &len)){
			windowsize = recv_packet.windowsize;
			if(recv_packet.ack == recv_ack){
				dups++;
			}
			else {
				dups = 0;
			}

				
			recv_ack = recv_packet.ack;
			if(recv_ack > rtt_measured_packet ){
				alarm(0);
				rtt_stop(&rttinfo, rtt_ts(&rttinfo) - ts);
			}

			first_unacknowledged_pos = recv_packet.ack;
			retransmit = 0;
			if(dups >= MAXDUPS){
				// retransmit first unack pos
				retransmit = 1;
				goto sendAgainFirstUnackPos;
			}
			
		}		
	}

	
}	

static void sig_alrm(int signo){
	siglongjmp(jmpbuf, 1);
}
