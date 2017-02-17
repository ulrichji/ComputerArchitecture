#include <limits.h>

#include "interface.hh"

#define LOGSIZE 512
#define NEARESTSIZE 5
#define WINDOWSIZE 10

typedef struct struct_log_item
{
	Addr pc;
	Addr mem_addr;
	Tick time;
	Addr predictionAddr;
	int miss;
	int loaded;
}LogItem;

LogItem log [LOGSIZE];
int logPointer = 0;
int logSize = 0;

LogItem nearest [NEARESTSIZE];

void prefetch_init(void)
{
	for(int i=0;i<LOGSIZE;i++)
	{
		log[i].pc = 0;
		log[i].mem_addr = 0;
		log[i].time = 0;
		log[i].predictionAddr = 0;
		log[i].miss = 0;
		log[i].loaded = 0;
	}
	for(int i=0;i<NEARESTSIZE;i++)
	{
		nearest[i].pc = 0;
		nearest[i].mem_addr = 0;
		nearest[i].time = 0;
		nearest[i].predictionAddr = 0;
		nearest[i].miss = 0;
		nearest[i].loaded = 0;
	}
}

void updateLog(AccessStat stat)
{
	if(logSize >= WINDOWSIZE)
	{
		int updatePointer = logPointer - WINDOWSIZE;
		//Make sure the pointer wraps around if the pointer recently have so
		if(updatePointer < 0)
			updatePointer += LOGSIZE;	
			
		//The page is now simulated as loaded and the predictionAddr is the currently accessed address.
		log[updatePointer].loaded = 1;
		log[updatePointer].predictionAddr = stat.mem_addr - log[updatePointer].mem_addr;
	}
}

void addToLog(AccessStat stat)
{
	log[logPointer].pc = stat.pc;
	log[logPointer].mem_addr = stat.mem_addr;
	log[logPointer].time = stat.time;
	log[logPointer].miss = stat.miss;
	log[logPointer].loaded = 0;
	log[logPointer].predictionAddr = 0;
	logPointer++;
	logSize++;
	if(logPointer >= LOGSIZE)
	{
		logPointer = 0;
	}
}

Addr distanceSquared(AccessStat a, LogItem b)
{
	Addr dist = 0;
	Addr diff = 0;
	
	//Calculate the actual distances
	diff = a.pc - b.pc;
	dist += diff * diff;
	
	diff = a.mem_addr - b.mem_addr;
	dist += diff * diff;
	
	diff = a.time - b.time;
	dist += diff * diff;
	
	return dist;
}

void findNearest(AccessStat stat)
{
	Addr largest = MAX_PHYS_MEM_ADDR * MAX_PHYS_MEM_ADDR;
	int largestIndex = 0;
	
	for(int i=0;i<NEARESTSIZE;i++)
		nearest[i].loaded = 0;
	
	for(int i=0;i<LOGSIZE;i++)
	{
		Addr distance = distanceSquared(stat, log[i]);
		if(distance < largest && log[i].loaded != 0)
		{
			nearest[largestIndex] = log[i];
			
			//Search for the largest element in near list
			largest = 0;
			for(int u=0;u<NEARESTSIZE;u++)
			{
				int nearDist = distanceSquared(stat,nearest[u]);
				if(nearest[u].loaded == 0)
				{
					largest = MAX_PHYS_MEM_ADDR * MAX_PHYS_MEM_ADDR;
					largestIndex = u;
				}
				else if(nearDist > largest)
				{
					largest = nearDist;
					largestIndex = u;
				}
			}
		}
	}
}

void doPrefetch(AccessStat stat)
{
	Addr closest = MAX_PHYS_MEM_ADDR * MAX_PHYS_MEM_ADDR;
	LogItem closestItem = nearest[0];
	for(int i=0;i<NEARESTSIZE;i++)
	{
		Addr dist = distanceSquared(stat,nearest[i]);
		if(dist < closest && nearest[i].loaded != 0)
		{
			closest = dist;
			closestItem = nearest[i];
		}
	}
	
	Addr pf_addr = stat.mem_addr + closestItem.predictionAddr;
	
	if(closestItem.loaded != 0 && pf_addr >= 0 && pf_addr < MAX_PHYS_MEM_ADDR && !in_cache(pf_addr))
	{
		issue_prefetch(pf_addr);
	}
}

void prefetch_access(AccessStat stat)
{
	//Update the log to consider the newly added adress.
	updateLog(stat);
	//Add this access to the log
	addToLog(stat);
	//Find the nearest match to the current AccessStat
	findNearest(stat);
	//Now find the page to load
	doPrefetch(stat);
}

void prefetch_complete(Addr addr)
{
	
}
