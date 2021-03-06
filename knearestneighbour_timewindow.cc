#include <limits.h>

#include "interface.hh"

#define LOGSIZE 512
#define NEARESTSIZE 5
#define WINDOWSIZE 5000

#define PCWEIGHT 10
#define MEMWEIGHT 0
#define TIMEWEIGHT 0
#define WRONGPENALTY 0//10000000000
#define GUESSPENALTY 0//100
#define GUESSCORRECTNESS 10

#define MAXWEIGHT ((PCWEIGHT * PCWEIGHT) + (MEMWEIGHT * MEMWEIGHT) + (TIMEWEIGHT * TIMEWEIGHT))

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
		log[i].miss = -1;
		log[i].loaded = 0;
	}
	for(int i=0;i<NEARESTSIZE;i++)
	{
		nearest[i].pc = 0;
		nearest[i].mem_addr = 0;
		nearest[i].time = 0;
		nearest[i].predictionAddr = 0;
		nearest[i].miss = -1;
		nearest[i].loaded = 0;
	}
}

void updateLog(AccessStat stat)
{
	int tempLogSize = logSize;
	int updatePointer = logPointer - 1;
	
	if(updatePointer < 0)
		updatePointer += LOGSIZE;
	
	while(tempLogSize >= 0)
	{
		if(log[updatePointer].time <= stat.time - WINDOWSIZE)
			break;
		else
		{
			updatePointer--;
			if(updatePointer < 0)
				updatePointer += LOGSIZE;
			tempLogSize--;
		}
	}
	
	if(tempLogSize >= 0)
	{
		log[updatePointer].loaded = 1;
		log[updatePointer].predictionAddr = stat.mem_addr - log[updatePointer].mem_addr;
	}
}

void addToLog(AccessStat stat)
{
	log[logPointer].pc = stat.pc;
	log[logPointer].mem_addr = stat.mem_addr;
	log[logPointer].time = stat.time;
	log[logPointer].miss = -1/*stat.miss*/;
	log[logPointer].loaded = 0;
	log[logPointer].predictionAddr = 0;
}

void logPrefetch(Addr predictionAddr)
{
	log[logPointer].predictionAddr = predictionAddr;
	logPointer++;
	logSize++;
	if(logPointer >= LOGSIZE)
	{
		logPointer = 0;
	}
	if(logSize >= LOGSIZE - 1)
	{
		logSize = LOGSIZE - 1;
	}
}

Addr distanceSquared(AccessStat a, LogItem b)
{
	Addr dist = 0;
	Addr diff = 0;
	
	//Calculate the actual distances
	diff = (a.pc - b.pc) * PCWEIGHT;
	dist += diff * diff;
	
	diff = (a.mem_addr - b.mem_addr) * MEMWEIGHT;
	dist += diff * diff;
	
	diff = (a.time - b.time) * TIMEWEIGHT;
	dist += diff * diff;
	
	if(b.miss == 1)
		dist += WRONGPENALTY;
	else if(b.miss == -1)
		dist += GUESSPENALTY;
	
	return dist;
}

void findNearest(AccessStat stat)
{
	Addr largest = MAX_PHYS_MEM_ADDR * MAX_PHYS_MEM_ADDR * MAXWEIGHT;
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

LogItem offsetWeightedAverage(AccessStat stat)
{
	LogItem averageItem;
	averageItem.predictionAddr = 0;
	averageItem.loaded = 0;
	
	double totalWeight = 0;
	double weightedSum = 0;
	
	for(int i=0;i<NEARESTSIZE;i++)
	{
		if(nearest[i].loaded != 0)
		{
			Addr dist = distanceSquared(stat,nearest[i]);
			double weight = 1 / (double)dist;
			totalWeight += weight;
			double weightedValue = weight * (double)nearest[i].predictionAddr;
			weightedSum += weightedValue;
			
			averageItem.loaded = 1;
		}
	}
	
	if(averageItem.loaded != 0)
		weightedSum /= totalWeight;
	
	averageItem.predictionAddr = (Addr)weightedSum;
	
	return averageItem;
}

LogItem offsetAverage(AccessStat stat)
{
	LogItem averageItem;
	averageItem.predictionAddr = 0;
	averageItem.loaded = 0;
	Addr loadedCount = 0;
	for(int i=0;i<NEARESTSIZE;i++)
	{
		if(nearest[i].loaded != 0)
		{
			averageItem.predictionAddr += nearest[i].predictionAddr;
			averageItem.loaded = 1;
			loadedCount ++;
		}
	}
	
	averageItem.predictionAddr /= loadedCount;
	
	return averageItem;
}

LogItem offsetNearest(AccessStat stat)
{
	Addr closest = MAX_PHYS_MEM_ADDR * MAX_PHYS_MEM_ADDR * MAXWEIGHT;
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
	
	return closestItem;
}

Addr doPrefetch(AccessStat stat)
{
	LogItem guessItem = offsetNearest(stat);
	
	Addr pf_addr = stat.mem_addr + guessItem.predictionAddr;
	
	if(guessItem.loaded != 0 && pf_addr >= 0 && pf_addr < MAX_PHYS_MEM_ADDR && !in_cache(pf_addr))
	{
		issue_prefetch(pf_addr);
	}
	
	return pf_addr;
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
	Addr fetchAddr = doPrefetch(stat);
	logPrefetch(fetchAddr);
}

void prefetch_complete(Addr addr)
{
	
}
