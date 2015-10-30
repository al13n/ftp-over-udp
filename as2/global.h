/*
 *
 *  The global.h file is part of the assignment 1 of the Network Programming.
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
#include <stdio.h>
#include <stdlib.h>

#define TRUE	1
#define FALSE	0

#define ECHO_PORT	12445 
#define TIME_PORT	12446

#define MAX_LINE	100

#define MAXDGRAMCONTENT 4096
#define MAXDUPS 3

struct socket_descripts
{
	int sockfd;
	struct in_addr *ip_addr;
	struct in_addr *net_mask;
	struct in_addr *subnet;	
};

struct client_arguments
{
	char *IPserver;
	int server_port;
	char *filename;
	int sliding_window_size;
	int seed;
	float probability_loss;
	int mean_time;
};

struct list_ips
{
	struct sockaddr_in cli_ip;
	struct list_ips *next;
};

typedef struct dgram
{
	int seqNum;
	int ack;
	int eof;
	int windowsize;
	char data[512 - 4*(sizeof(int))];
};
