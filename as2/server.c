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
#include "unprtt.h"
#include <setjmp.h>

/*
 * External Functions
 */
extern struct ifi_info 
*Get_ifi_info_plus(int family, int doaliases);
extern void 
free_ifi_info_plus(struct ifi_info *ifihead);

/*
 * Local Functions
 */
void
handshake(int sockfd, SA *serv_addr, socklen_t serv_len, SA *cli_addr, socklen_t cli_len, char *filename, int max_sending_window_size);

struct list_ips* 
create_list(struct sockaddr_in ip);

struct list_ips* 
add_list(struct sockaddr_in ip);

struct list_ips* 
search_list(struct sockaddr_in ip, struct sockaddr_in **prev);

int del_list(struct sockaddr_in ip);

int readFileContents(char *, int );
static void	sig_alrm(int signo);

/*
 * External Variables
 */
struct list_ips *head = NULL;
struct list_ips *cur = NULL;

const int dataMaxLen = 512 - 4*(sizeof(int));
struct dgram fileContent[MAXDGRAMCONTENT];
static sigjmp_buf	jmpbuf;
static struct rtt_info   rttinfo;
static int	rttinit = 0;

int 
main(int argc, char **argv)
{
	FILE *fp;
	char buf[MAX_LINE];
	int count=0;
	int serv_port;
	int max_sending_window_size;
	const int on=1;
	struct socket_descripts sock_desc[10];
	int sock_count = 0;
	struct ifi_info *ifi, *ifihead;
	int sockfd;
	struct sockaddr_in *sa;
	int i = 0;
	fp=fopen("server.in","r");

	if(fp == NULL)
	{
		printf("ERROR MSG: No server.in file!\n");
		exit(1);
	}

	while(fgets(buf, MAX_LINE -1, fp) != NULL)
	{
		if(count == 0)
			serv_port = atoi(buf);
		else if(count == 1)
			max_sending_window_size = atoi(buf);
		count++;
	}
	fclose(fp);

	for (ifihead = ifi = Get_ifi_info_plus( AF_INET, 1);
                ifi != NULL; 
		ifi = ifi->ifi_next) {

		/* bind unicast adress */
		sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
		Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,&on, sizeof(on));

		sa = (struct sockaddr_in *) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = htons(serv_port);

		Bind(sockfd, (SA *) sa, sizeof(*sa));

		sock_desc[sock_count].sockfd = sockfd;
		sock_desc[sock_count].ip_addr = sa;
		sock_desc[sock_count].net_mask = (struct sockaddr_in *)ifi->ifi_ntmaddr;

		//printf("SERVER: sockdesc[%d]\n", sock_count);
		printf("SERVER: IP: %s\n", Sock_ntop(sock_desc[sock_count].ip_addr, sizeof(struct in_addr)));
		printf("SERVER: Network mask: %s\n", Sock_ntop(sock_desc[sock_count].net_mask, sizeof(struct in_addr)));
		//printf("SERVER: sockdesc[%d]\n", sock_count); 
		sock_count++;
	}
	
	int maxfd;
	fd_set rset;
	int nready;
	int len;
	char recvline[MAXLINE];	
	struct sockaddr_in cliaddr, child_server_addr, server_addr, sock_store;
	
	int n;
	int server_len;
	char IPserver[MAXLINE];
	int pid;
	socklen_t serv_len, store_len;
	struct list_ips* r = NULL;

	maxfd=sock_desc[0].sockfd;
	for(i=0;i < sock_count; i++)
	{
		if(maxfd < sock_desc[i].sockfd)
			maxfd = sock_desc[i].sockfd;
	}

	for( ; ; )
	{
		FD_ZERO(&rset);
		for(i = 0; i < sock_count; i++)
			FD_SET(sock_desc[i].sockfd, &rset);

		//printf("SERVER: waiting using select()\n");
		
		if( (nready=select(maxfd+1, &rset, NULL, NULL, NULL)) < 0 )
		{
			if (errno == EINTR){
				continue;
			}else{
				printf("ERROR MSG: server select error\n");
				exit(0);
			}
		}


		
		for(i=0; i < sock_count; i++)
		{
			if(FD_ISSET(sock_desc[i].sockfd, &rset))
			{
				char filename[MAXLINE];
				int client_window_size;
				len = sizeof(cliaddr);
				
				//printf("SERVER: before revefrom func()\n");	
				n = recvfrom(sock_desc[i].sockfd, recvline, MAXLINE, 0, (SA *) &cliaddr, &len);
				sscanf(recvline, "%s %d", filename, &client_window_size);
				//printf("%s %d\n", filename, client_window_size);
				//printf("SERVER: first message is recevied: %s\n", recvline);
				
				r = search_list(cliaddr, NULL);
				if( r != NULL)
					continue;

				server_len = sizeof(server_addr);	
				if( getsockname( sock_desc[ i ].sockfd, (SA *)&server_addr, &server_len ) < 0 )
				{
					printf( "SERVER: getsockname error\n" );
					exit(1);
				}      
				inet_ntop( AF_INET, &(server_addr.sin_addr), IPserver, MAXLINE);
				//printf("SERVER: IPserver after receving filename: %s\n", IPserver);

				
				inet_pton( AF_INET, IPserver, &child_server_addr.sin_addr );	
				child_server_addr.sin_family = AF_INET;
				child_server_addr.sin_port = htonl(0);
			
				add_list(cliaddr);

				if ( (pid = fork() ) == 0)
				{
					int j;
					for(j=0; j < sock_count; j++)
					{
						if(i!=j)
							close(sock_desc[j].sockfd);
					}
					//printf("%d %d\n", max_sending_window_size, client_window_size);
					max_sending_window_size = min(max_sending_window_size, client_window_size);
					handshake(sock_desc[i].sockfd,&child_server_addr, sizeof(child_server_addr), &cliaddr, sizeof(cliaddr),  filename, max_sending_window_size );
					del_list(cliaddr);
					exit(0);
				}
			}
		}
	}
}

void
handshake(int sockfd, SA *serv_addr, socklen_t serv_len, SA *cli_addr, socklen_t cli_len, char *filename, int max_sending_window_size)
{
	FILE *fd;
	char sendline[MAXLINE];
	char recvline[MAXLINE];
	int listen_sockfd = sockfd;
	int new_sockfd;
	int on = 1;
	struct sockaddr_in ss;
	int ss_len = sizeof(ss);
	int n;

	fd = fopen( filename, "r");
	
	getsockopt(listen_sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));

	/* establishing new_sock */
	if((new_sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == NULL)
        {
                printf("SERVER: socket error in handshake\n" );
                exit(1);
        }
	setsockopt(new_sockfd, SOL_SOCKET, SO_DONTROUTE | SO_REUSEADDR, &on, sizeof(on));
	Bind(new_sockfd, (SA *)serv_addr, sizeof(struct sockaddr_in ));
	
	/* getting ephemeral PORT */
	if( getsockname(new_sockfd, (SA *)&ss, &ss_len) < 0)
	{
		printf("SERVER: getsockname error \n");
		exit(1);
	}
#if(1)
	/* For Debug */
	int IPbuf[MAXLINE];
	inet_ntop( AF_INET, &(ss.sin_addr), IPbuf, MAXLINE);
	printf("SERVER: new_sockfd IP %s, port %d\n",IPbuf, ss.sin_port); 

#endif
	if(connect(new_sockfd, (SA *)cli_addr, cli_len) < 0)
        {
                printf("SERVER: connect error in handshake\n" );
                exit(1);
        }

	sprintf(sendline, "%d", ss.sin_port);
#if(0)

	ssize_t
	dg_send_recv(int fd, const void *outbuff, size_t outbytes,
                         void *inbuff, size_t inbytes,
                         const SA *destaddr, socklen_t destlen)


#else
	Sendto(listen_sockfd, sendline, 10, 0, cli_addr, cli_len);
	printf("SERVER: sending port num %d\n", ss.sin_port);

	n = recvfrom(new_sockfd, recvline, MAXLINE, 0, (SA *)cli_addr, &cli_len);
	recvline[15] = 0;
	printf("SERVER: receving ACK %s \n", recvline);
#endif
	close(listen_sockfd);	/* close listening socket */

	/* File data transfer */

	//fread(sendline, 1, 10, fd);
	
	//Sendto(new_sockfd, sendline, 10, 0, NULL, NULL);
	//printf("SERVER: sending file 10 bytes to client\n");
	//recvfrom(new_sockfd, recvline, MAXLINE, 0, (SA *)cli_addr, &cli_len);
	//printf("SERVER: Ack from client about PACKET 1\n");
	
	//while(1);
	int totalblocks = readFileContents(filename, max_sending_window_size);
	//printf("%d\n", totalblocks);
	sendFileContents(new_sockfd, totalblocks, max_sending_window_size);
}

struct list_ips* create_list(struct sockaddr_in ip)
{
	printf("SERVER: starting create_list() \n");
	
	struct list_ips *ptr = (struct list_ips *)malloc( sizeof(struct list_ips));

	if(ptr == NULL){
		printf("SERVER: create_list() fail\n");
		return NULL;
		}
	ptr->cli_ip.sin_addr = ip.sin_addr;
	ptr->cli_ip.sin_port = ip.sin_port;
	ptr->next = NULL;
	
	head = cur = ptr;
	return ptr;
}

struct list_ips* add_list(struct sockaddr_in ip)
{
	printf("SERVER: starting add_list()\n");

	if(head == NULL)
		return(create_list(ip));
	struct list_ips *ptr = (struct list_ips *)malloc( sizeof(struct list_ips));
	if(ptr == NULL){
                printf("SERVER: add_list() fail\n");
                return NULL;
                }
	
	ptr->cli_ip.sin_addr.s_addr = ip.sin_addr.s_addr;
        ptr->cli_ip.sin_port = ip.sin_port;

	ptr->next = NULL;

	cur->next = ptr;
	cur = ptr;

	printf("SERVER: end of ad_list()\n");
	return ptr;
}



struct list_ips* search_list(struct sockaddr_in ip, struct sockaddr_in **prev)
{
	struct list_ips *ptr = head;

	while(ptr != NULL){
		if( (ptr->cli_ip.sin_addr.s_addr == ip.sin_addr.s_addr)
		   && (ptr->cli_ip.sin_port == ip.sin_port) )
		{
			return ptr;
		}
		else
		{
			prev = ptr;
			ptr = ptr->next;
		}
	}

	return NULL;
}

int del_list(struct sockaddr_in ip)
{
	struct list_ips *prev = NULL;
	struct list_ips *del = NULL;

	printf("SERVER: starting del_list()\n");

	del = search_list(ip, &prev);

	if(del == NULL)
		return -1;
	else{
		if(prev != NULL)
			prev->next = del->next;

		if(del == cur)
			cur = prev;
		else if(del == head)
			head = del->next;
	}

	free(del);
	return 0;
}



int readFileContents(char *fileName, int windowsize){
	FILE *fp = fopen(fileName, "r");
    int seq=0, i;

    if(fp == NULL){
    	// TODO: file does not exist
    	return 0;
    }

    while(1)
    {	
    	struct dgram packet;
        char buff[dataMaxLen];
        int eof = 0;
        //printf("\n\n\n readFileContents %d \n\n\n", seq);
        for(i = 0; i < dataMaxLen-1; i++)
        {
            int c = fgetc(fp);
            //printf("%c\n", );
            if(c == EOF)
            {
                eof = 1;
                break;
            }
            buff[i] = c;
        }
        buff[i] = '\0';

        //printf("SEQNUM \n");
        //printf("%s\n", buff);
        // Assign contents for a particular packet
        
        packet.seqNum = seq++;
        packet.windowsize = windowsize;
        //printf("WTF\n");
        strcpy(packet.data,buff);
        //printf("WTF\n");
        packet.eof = 0;
        packet.ack = 1;
        fileContent[seq-1] = packet;
        //printf("%d : %s\n", fileContent[seq-1].seqNum, fileContent[seq-1].data);
        //printf("%d : %s\n", seq-1, buff);
        if(eof){
            packet.eof=1;
            break;
        }
    }

    printf("---Read %d blocks from file----\n", seq);
    return seq;
}

int sendFileContents(int sockfd,int totalblocks, int windowsize){
	//printf("%d\n", totalblocks);
	int first_unacknowledged_pos = 0;
	int pos_sent = -1; 
	struct dgram recv_packet;
	int dups = 0;
	int recv_ack = -1;
	int retransmit = 0;
	int rtt_measured_packet = 0;
	uint32_t	ts;	

	Signal(SIGALRM, sig_alrm);
	if (rttinit == 0) {
		rtt_init(&rttinfo);		/* first time we're called */
		rttinit = 1;
		rtt_d_flag = 1;
	}

	while(1){
		//printf("HERE-retransmit\n");
		sendAgainFirstUnackPos:
		if(retransmit)
		{	// will serve also as a probe
			printf("Retransmit Packet\n");
			//rtt_debug(&rttinfo);
			Sendto(sockfd, (void *)&fileContent[first_unacknowledged_pos], sizeof(struct dgram), 0, NULL, NULL);
		}

		if (sigsetjmp(jmpbuf, 1) != 0) {
			if (rtt_timeout(&rttinfo) < 0) {
				err_msg("no response from server, giving up");
				rttinit = 0;	/* reinit in case we're called again */
				errno = ETIMEDOUT;
				return(-1);
			}
			retransmit = 1;
			goto sendAgainFirstUnackPos;
		}

		int firstPacketSent = 1;
		//printf("%d\n", totalblocks);
		printf("%d %d %d %d\n", pos_sent, totalblocks-1, first_unacknowledged_pos, windowsize);
		while( (pos_sent < totalblocks-1) && (pos_sent + 1 < first_unacknowledged_pos + windowsize)){
			if(firstPacketSent){
				firstPacketSent = 0;
				alarm(rtt_start(&rttinfo));
				ts = rtt_ts(&rttinfo);
				rtt_measured_packet = pos_sent+1;
			}
			printf("SEND MESSAGE PACKET: %d\n", fileContent[pos_sent+1].seqNum);
			//printf("%d\n", rtt_ts(&rttinfo));
			//rtt_debug(&rttinfo);
			printf("%d %d %d %d\n", pos_sent, totalblocks-1, first_unacknowledged_pos, windowsize);
			//printf("%s\n", fileContent[pos_sent+1].data);
			Sendto(sockfd, (void *)&fileContent[pos_sent+1], sizeof(struct dgram), 0, NULL, NULL);
			pos_sent++;
		}
		//sleep(100);

		if(Recvfrom(sockfd, &recv_packet, sizeof(struct dgram), 0, NULL, NULL)){

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

		if(fileContent[first_unacknowledged_pos-1].eof){
			break;
		}		
	}

	return totalblocks;	
}

static void sig_alrm(int signo){
	siglongjmp(jmpbuf, 1);
}
