/*
 *
 *  The tcpechotimesrv.c file is part of the assignment 1 of the Network Programming.
 *  It is a simple TCP implementation, basically, it is
 *  an academic project of CSE533 of Stony Brook University in fall
 *  2015. For more details, please refer to Readme.
 *
 *  Copyright (C) 2015 Dongju Ok   <dongju@stonybrook.edu,
 *                                  yardbirds79@gmail.com>
 *
 *  It is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  It is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "unpifiplus.h"
#include "global.h"
#include "unpthread.h"
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <string.h>
/*
 * External Functions
 */
extern struct ifi_info 
*Get_ifi_info_plus(int family, int doaliases);
extern void 
free_ifi_info_plus(struct ifi_info *ifihead);

static void	sig_alrm(int);
void * consumerThread(void *);
int receive_file(int );
static sigjmp_buf	jmpbuf;
int seed, mean;

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

/*
 * Local Functions
 */

pthread_mutex_t recv_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
int seq_stdout = -1;
int maxWindowSize;
int seed, mean_time;

struct dgram* recv_window[MAXDGRAMCONTENT];


int 
main(int argc, char **argv)
{
	FILE *fp;
	struct client_arguments client_info;
	//char *IPserver;
        int server_port;
        char filename[MAXLINE];
        int sliding_window_size;
        float probability_loss;
	struct socket_descripts sock_desc[10];
	char buf[MAXLINE];
	int count = 0;
	struct ifi_info *ifi, *ifihead;
	struct sockaddr_in *sa;
	int i;
	char IPserver[MAXLINE], IPclient[MAXLINE];
	int sockfd;
	int sock_count = 0;
	int loopback = FALSE;
	int window_size;
	int local = FALSE;
	uint64_t netopt = 0;

	if( (fp = fopen("client.in", "r")) == NULL)
	{
		printf("ERROR MSG: client open error\n");
		exit(1);
	}

	while (fgets(buf, MAX_LINE -1, fp) != NULL)
        {
		switch(count){
			case 0:
				memset(IPserver, 0, MAXLINE);
				strncpy(IPserver, buf, strlen(buf)-2);
				break;
			case 1:
				server_port = atoi(buf);
				break;
			case 2:
				memset(filename, 0, MAXLINE);
				strncpy(filename, buf, strlen(buf)-1);
				
				break;
			case 3:
				maxWindowSize = atoi(buf);
				break;
			case 4: 
				seed = atoi(buf);
				break;
			case 5:
				probability_loss = atof(buf);
				break;
			case 6:
				mean_time = atof(buf);
				break;
			default:
				printf("ERROR MSG: client client.in input error\n");
				exit(1);
				break;
		}
		count++;
	}
	fclose(fp);

	client_info.IPserver = IPserver;
	client_info.server_port = server_port;
	client_info.filename = filename;
	client_info.sliding_window_size = maxWindowSize;
	client_info.seed = seed;
	client_info.probability_loss = probability_loss;
	client_info.mean_time = mean_time;
#if(DEBUG)
	/* Debug Info */
	printf("CLIENT: client_info.IPserver: %s\n", client_info.IPserver);
	printf("CLIENT: client_info.filename: %s\n", client_info.filename);
#endif


	/* stdout */
        printf("\nCLIENT:\t\tsock descripts information\n");

	for (ifihead = ifi = Get_ifi_info_plus( AF_INET, 1);
                ifi != NULL;
                ifi = ifi->ifi_next) {

                sa = (struct sockaddr_in *) ifi->ifi_addr;
                sa->sin_family = AF_INET;
                sa->sin_port = htonl(server_port);

                sock_desc[sock_count].sockfd = -1;
                sock_desc[sock_count].ip_addr = sa;
                sock_desc[sock_count].net_mask = (struct sockaddr_in *)ifi->ifi_ntmaddr;

		sock_desc[sock_count].subnet = (struct sockaddr_in *)malloc( sizeof(struct sockaddr_in));
		sock_desc[sock_count].subnet->s_addr =  sock_desc[sock_count].ip_addr->s_addr & sock_desc[sock_count].net_mask->s_addr;
		
		/* stdout */
                printf("CLIENT: sock_desc[%d] IP: %s\n", sock_count, sock_ntop_host((SA *)sock_desc[sock_count].ip_addr, sizeof( struct in_addr)));
                printf("CLIENT: sock_desc[%d] NetMask: %s\n",sock_count, Sock_ntop_host(sock_desc[sock_count].net_mask, sizeof(struct in_addr)));
		printf("CLIENT: sock_desc[%d] SubNet Mask: %s\n", sock_count,  Sock_ntop_host(sock_desc[sock_count].subnet, sizeof(struct in_addr)));
		printf("\n");

		sock_count++;
	}


	/* Identify Loopback */	
	for(i=0; i < sock_count; i++)
	{
		char IPbuf[MAXLINE];
		char *p;
		char *p1;
#if(DEBUG)
		printf("CLIENT: 1st %s\n", Sock_ntop_host( (SA *)sock_desc[i].ip_addr, sizeof( struct in_addr *)));
		printf("CLIENT: 2nd %s\n", client_info.IPserver);
#endif
		p = sock_ntop( (SA *)(sock_desc[i].ip_addr), sizeof( struct in_addr *));
		
		p1 = strtok(p, ":");
#if(DEBUG)
		//printf("CLIENT: sock_desc.ip_addr = %s\n",p1 );
		printf("CLIENT: p1 = %s\n", p1);
		printf("CLIENT: client_ino.IPserver = %s\n", client_info.IPserver);
#endif

		if( strcmp(p1, client_info.IPserver) == 0)
		{
			printf("CLIENT: Loopback case\n");
			loopback = TRUE;
			strcpy(IPserver, "127.0.0.1\n");
			strcpy(sock_desc[i].ip_addr, IPserver);
			strcpy(IPclient, IPserver);
			break;
		}
		else
		{
			struct in_addr ip, netmask, subnet1, subnet2, serv_ip, longest_prefix_ip, longest_prefix_netmask;
        		char prefix1[MAXLINE], prefix2[MAXLINE], str_longest_prefix_ip[MAXLINE], str_longest_prefix_netmask[MAXLINE];
			
			ip =(struct in_addr)((struct sockaddr_in *)(sock_desc[i].ip_addr))->sin_addr;
			netmask = (struct in_addr) ((struct sockaddr_in *)(sock_desc[i].net_mask))->sin_addr;
			
			subnet1.s_addr = ip.s_addr & netmask.s_addr;
			//subnet1 = (struct in_addr)((struct sockaddr_in *)(sock_desc[i].subnet))->sin_addr;
			
                        inet_ntop(AF_INET, &subnet1, prefix1, MAXLINE);
			

			inet_pton(AF_INET, client_info.IPserver, &serv_ip);
			
                        subnet2.s_addr = serv_ip.s_addr & netmask.s_addr;

                        inet_ntop(AF_INET, &subnet2, prefix2, MAXLINE);


                        if (strncmp(prefix1, prefix2, strlen(prefix1)) == 0){
                                if (netmask.s_addr > longest_prefix_netmask.s_addr){
                                        longest_prefix_ip = serv_ip;
                                        longest_prefix_netmask = netmask;
				
					inet_ntop( AF_INET, &longest_prefix_ip, IPserver, MAXLINE);
					local = TRUE;
					/* stdout */
                                        inet_ntop(AF_INET, &longest_prefix_ip, str_longest_prefix_ip, MAXLINE);
                                        printf("CLIENT: longest prefix ip = %s\n",str_longest_prefix_ip);
                                        inet_ntop(AF_INET, &longest_prefix_netmask, str_longest_prefix_netmask, MAXLINE);
                                        printf("CLIENT: longest prefix netmask = %s\n",str_longest_prefix_netmask);
                               		printf("\n");
				 }
                        }
		}
		free( sock_desc[sock_count].subnet );
	}

	if( (loopback == TRUE) || (local == TRUE))
	{
		netopt = SO_DONTROUTE;
		printf("CLIENT: Server IP is \"local\" network\n");
	}

#if(DEBUG)	
	/* Debug info */
	printf("CLIENT: IPserver : %s\n",IPserver);

#endif

	char sendline[MAXLINE], recvline[MAXLINE];
	struct 	sockaddr_in servaddr1;
	socklen_t servaddr1_len = sizeof(servaddr1);
	struct sockaddr_in cliaddr, servaddr;
	struct sockaddr_in serv_addr;
	socklen_t serv_addr_len = sizeof(serv_addr);
	int on=1;
	int n;	

	inet_pton( AF_INET, IPclient, &cliaddr.sin_addr );
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = htons(0);

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR & netopt, &on, sizeof(on));
	Bind( sockfd, (SA *)&cliaddr, sizeof(cliaddr));
#if(DEBUG)
	printf("CLIENT: passing Bind()\n");
#endif
	if(getsockname (sockfd, (SA *) &servaddr1, &servaddr1_len) < 0)
	{
		perror("Client information could not be obtained");
		exit(0);
	}
#if(DEBUG)
	printf("CLIENT: passing getsockname()\n");
#endif
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(client_info.server_port);

	inet_pton(AF_INET, client_info.IPserver, &(servaddr.sin_addr));
	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
#if(DEBUG)	
	printf("CLIENT: passing Connect()\n");
#endif
	bzero(&servaddr1, sizeof(servaddr1));
	if( getpeername(sockfd, (SA *) &servaddr1, &servaddr1_len) < 0)
	{ 
		printf("CLIENT: passing getpeername()\n");
		exit(1);
	}
	
	strcpy(sendline, client_info.filename);
	sprintf(sendline, "%s %d", client_info.filename, client_info.sliding_window_size);
	Sendto(sockfd, sendline, strlen(sendline), MSG_DONTROUTE, 0, 0);
#if(DEBUG)
	printf("CLIENT: sending message %s\n", sendline);
	printf("CLIENT: passing Sendto()\n");
#endif
#if(DEBUG)
	/* Test tracking client IP */
	sleep(1);
	Sendto(sockfd, sendline, strlen(sendline), MSG_DONTROUTE, 0, 0);
        printf("CLIENT: passing Sendto()\n");

#endif	

	
	/* receving PORT */
	Recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);
#if(DEBUG)
	printf("ClIENT: recevied port %s \n", recvline);
#endif	
	servaddr1.sin_port = (uint16_t)atoi(recvline);
#if(DEBUG)
	/* For Debug */
	char IPbuf[MAXLINE];
	printf("CLIENT: get port num %d \n",(uint16_t)servaddr1.sin_port);
	inet_ntop( AF_INET, &(servaddr1.sin_addr), IPbuf, MAXLINE);
	printf("CLIENT: sock_store IP: %s, port: %d\n", IPbuf, (uint16_t)servaddr1.sin_port);
#endif	
	if(connect(sockfd, &servaddr1, servaddr1_len) < 0)
	{
		printf("CLIENT: connect() error after receving port num\n");
		exit(1);	
	}	
	
#if(DEBUG)
	/* For Debug */
	struct sockaddr_in sa1;
        socklen_t sa1_len = sizeof(sa1);
	struct  sockaddr_in sa2;
        socklen_t sa2_len = sizeof(sa2);
	
	if(getsockname (sockfd, (SA *) &sa1, &sa1_len) < 0)
        {
                perror("Client information could not be obtained");
                exit(0);
        }
	//printf("CLIENT: client  port: %d\n", (uint16_t)(sa1.sin_port));  

	if( getpeername(sockfd, (SA *)&sa2, &sa2_len) < 0)
        {
                printf("CLIENT: getpeeername() error\n");
                exit(1);
        }
	printf("CLIENT: client  access to port: %d\n", (uint16_t)(sa2.sin_port));
#endif
	/* ACK */
	char *a = "I am ack";
	strncpy(sendline, a, strlen(a) );
	Sendto(sockfd, sendline, strlen(sendline), MSG_DONTROUTE, 0, 0);
#if(DEBUG)	
	printf("CLIENT: sending ACK\n");
#endif
	/* File transfer */
	receive_file(sockfd);
}

int receive_file(int sockfd){
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
				Sendto(sockfd, (void *)&emptyDgram, sizeof(emptyDgram), 0, NULL, NULL);
				alarm(200);
				
		}
		
		struct dgram recvDgram;
		if(Recvfrom(sockfd, &recvDgram, sizeof(recvDgram), 0, NULL, NULL)){
			//printf("%d:\t%s\n",recvDgram.seqNum, recvDgram.data);
			if(recvDgram.seqNum >= seq_stdout + 1 && recvDgram.seqNum < seq_stdout + 1 + maxWindowSize){
				//printf("PACKET Receving?\n");
				Pthread_mutex_lock(&recv_buffer_lock);
				alarm(200);
				struct dgram* emptyDgram = (struct dgram *)malloc(sizeof(struct dgram));
				strcpy(emptyDgram->data, recvDgram.data);
				//printf("PACKET Receving?\n");
				emptyDgram->seqNum = recvDgram.seqNum;
				emptyDgram->ack = recvDgram.ack;
				emptyDgram->eof = recvDgram.eof;
				emptyDgram->windowsize = recvDgram.windowsize;
				recv_window[recvDgram.seqNum % maxWindowSize] = emptyDgram;
				windowsize--;
				Pthread_mutex_unlock(&recv_buffer_lock);
			}
		}


	}
}


void * consumerThread(void *arg){
	int eof = 0;
	srand(seed);
	pthread_detach(pthread_self());
	while(1){
		double sleep_time = -1*mean_time*log((double)(rand()*1.0/RAND_MAX));
		//printf("THREAD: %f\n", sleep_time);

		usleep(sleep_time);
		Pthread_mutex_lock(&recv_buffer_lock);
		while(recv_window[(seq_stdout+1)%maxWindowSize] != NULL){
			//printf("%d\n", seq_stdout+1);
			//printf("%s\n", recv_window[(seq_stdout+1)%maxWindowSize]->data);
			Fputs(recv_window[(seq_stdout+1)%maxWindowSize]->data, stdout);
			if(recv_window[(seq_stdout+1)%maxWindowSize]->eof == 1)
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
