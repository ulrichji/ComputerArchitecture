#include "interface.hh"
#include <limits.h>

#define LOGSIZE 256
#define WINDOWSIZE 10
#define BLOCKSIZE 1

typedef struct struct_log_item
{
	Addr pc;
	Addr lastAddr;
	Addr offsetPrediction;
	int loaded;
}LogItem;

LogItem log [LOGSIZE];
int logSize = 0;
int logPointer = 0;

//Function will issue a prefetch if the address is valid and if the adress is not in the cache.
void doPrefetch(Addr pf_addr)
{
	if(pf_addr >= 0 && pf_addr < MAX_PHYS_MEM_ADDR && !in_cache(pf_addr))
	{
		issue_prefetch(pf_addr);
	}
}

void resetLogItem(int index)
{
	log[index].pc = 0;
	log[index].lastAddr = 0;
	log[index].offsetPrediction = 0;
	log[index].loaded = 0;
}

void prefetch_init(void)
{
	for(int i=0;i<LOGSIZE;i++)
		resetLogItem(i);
}

void updateLog(AccessStat stat)
{

	if(logSize >= WINDOWSIZE)
	{
		//Go back in time to update what that address should be
		int updatePointer = (logPointer - WINDOWSIZE) % LOGSIZE;
		log[updatePointer].offsetPrediction = (stat.mem_addr / BLOCKSIZE) - log[updatePointer].lastAddr;
		log[updatePointer].loaded = 1;
	}
	
	//Now update the newest arrival
	resetLogItem(logPointer);
	log[logPointer].pc = stat.pc;
	log[logPointer].lastAddr = stat.mem_addr / BLOCKSIZE;
	
	logPointer = (logPointer + 1) % LOGSIZE;
	logSize += 1;
	if(logSize > LOGSIZE)
		logSize = LOGSIZE;
}

void predictOffset(AccessStat stat)
{
	LogItem lastValid;
	lastValid.loaded = 0;
	lastValid.offsetPrediction = 0;

	for(int i=1;i<=logSize;i++)
	{
		int checkPointer = (logPointer - i) % LOGSIZE;
		if(log[checkPointer].pc == stat.pc && log[checkPointer].loaded != 0)
		{
			lastValid = log[checkPointer];
			break;
		}
	}
	
	if(lastValid.offsetPrediction != 0 && lastValid.loaded != 0)
	{
		Addr pf_addr = ((stat.mem_addr / BLOCKSIZE) + lastValid.offsetPrediction) * BLOCKSIZE;
		doPrefetch(pf_addr);
	}

	/*for(int i=0;i<LOGSIZE;i++)
	{
		if(log[i].pc == stat.pc)
		{
			if(log[i].offsetPrediction != 0)
			{
				Addr pf_addr = ((stat.mem_addr / BLOCKSIZE) + log[i].offsetPrediction) * BLOCKSIZE;
				doPrefetch(pf_addr);
			}
			break;
		}
	}*/
}

void prefetch_access(AccessStat stat)
{
	updateLog(stat);
	
	predictOffset(stat);
}

void prefetch_complete(Addr addr)
{

}
