#include "unp.h"
#include "unpthread.h"
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_WINDOW_SIZE 4096

unsigned int alarm (unsigned int useconds)
{
    struct itimerval old, new;
    new.it_interval.tv_usec = 0;
    new.it_interval.tv_sec = 0;
    new.it_value.tv_usec = (long int) useconds;
    new.it_value.tv_sec = 0;
    if (setitimer (ITIMER_REAL, &new, &old) < 0)
        return 0;
    else
        return old.it_value.tv_sec;
}

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


pthread_mutex_t recv_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
int seq_stdout = -1;

struct dgram* recv_window[MAX_WINDOW_SIZE];

int
main(int argc, char **argv)
{
	int					sockfd;
	struct sockaddr_in	servaddr;

	if (argc != 2)
		err_quit("usage: udpcli <IPaddress>");

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT);
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

	//dg_cli(stdin, sockfd, (SA *) &servaddr, sizeof(servaddr));
	struct dgram mesg;
	mesg.x = 10;
	int len;
	Sendto(sockfd, (void *)&mesg, sizeof(mesg), 0, (SA *) &servaddr, sizeof(servaddr));
	//Recvfrom(sockfd, &mesg, sizeof(mesg), 0, (SA *) &servaddr, &len);
	printf("%d\n", mesg.x);
	

	exit(0);
}

int receive_file(int sockfd, struct sockaddr_in servaddr, int len){
	int prev_ack = 0;
	int windowsize = maxWindowSize;
	pthread_t	tid;
	Pthread_create(&tid, NULL, consumerThread, NULL);

	while(1){

		if (sigsetjmp(jmpbuf, 1) != 0) {
			//send ACK - seq_stdout+1
				struct dgram emptyDgram;
				emptyDgram.ack = seq_stdout + 1;
				windowsize = windowsize + ( emptyDgram.ack - prev_ack);
				prev_ack = emptyDgram.ack;
				emptyDgram.windowsize = windowsize;
				Sendto(sockfd, (void *)&emptyDgram, sizeof(emptyDgram), 0, (SA *) &servaddr, len);
				alarm();
		}
		
		struct dgram recvDgram;
		if(Recvfrom(sockfd, (void *)&recvDgram, sizeof(struct dgram), 0, (SA*)&servaddr, sizeof(servaddr))){
			if(recvDgram.seqNum >= seq_stdout + 1 && recvDgram.seqNum < seq_stdout + 1 + maxWindowSize){
				alarm();
				recv_window[recvDgram.seqNum % maxWindowSize] = recvDgram;
				windowsize--;
			}
		}


	}
}

(void *) consumerThread(void *arg){
	int eof = 0;
	srand(seed);
	while(1){
		double sleep_time = -1*mean*log((double)rand());
		usleep(sleep_time);
		Pthread_mutex_lock(&recv_buffer_lock);
		while(recv_window[(seq_stdout+1)%maxWindowSize] != NULL){
			Fputs(recv_window[(seq_stdout+1)%maxWindowSize].data, stdout);
			if(recv_window[(seq_stdout+1)%maxWindowSize].eof == 1)
				eof = 1;
			recv_window[(seq_stdout+1)%maxWindowSize] = NULL;
			seq_stdout++;
		}
		Pthread_mutex_unlock(&recv_buffer_lock);
		if(eof)
			break;
	}
	exit(0);
}

static void sig_alrm(int signo){
	
	siglongjmp(jmpbuf, 1);
}
