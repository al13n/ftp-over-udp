/* include rtt1 */
#include	"unprtt.h"

int		rtt_d_flag = 0;	

#define	RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (4 * (ptr)->rtt_rttvar))

static int
rtt_minmax(int rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return(rto);
}

void
rtt_init(struct rtt_info *ptr)
{
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ptr->rtt_base = tv.tv_sec;		
	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 750;
	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
		/* first RTO at (srtt + (4 * rttvar)) = 3 seconds */
}

uint32_t
rtt_ts(struct rtt_info *ptr)
{
	uint32_t		ts;
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000);
	return(ts);
}

void
rtt_newpack(struct rtt_info *ptr)
{
	ptr->rtt_nrexmt = 0;
}

int
rtt_start(struct rtt_info *ptr)
{
	return((ptr->rtt_rto));		
}

void
rtt_stop(struct rtt_info *ptr, uint32_t ms)
{
	
	int		delta;
	
	ptr->rtt_rtt = ms / 1000;		
	delta = ptr->rtt_rtt - ptr->rtt_srtt;
	ptr->rtt_srtt += delta / 8;		
    if (delta < 0)
		delta = -delta;				
	ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4;	

	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
}

int
rtt_timeout(struct rtt_info *ptr)
{
	ptr->rtt_rto *= 2;		/* next RTO */
	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
	if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
		return(-1);			/* time to give up for this packet */
	return(0);
}


void
rtt_debug(struct rtt_info *ptr)
{
	if (rtt_d_flag == 0)
		return;

	fprintf(stderr, "rtt = %d, srtt = %d, rttvar = %d, rto = %d\n, elapsedtime = %d",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stderr);
}
