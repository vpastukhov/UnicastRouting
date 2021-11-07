#include "monitor_neighbors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include <inttypes.h>

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];
extern char *filename;
extern int DistVector[256];
extern int Neighbor[256];
extern int costTable[256];
extern struct ft_entry forwardTable[256];
extern char dvbuf[1539];

char dvbuf[1539];

unsigned int DV_Buf(char* buf){
	int i;
    sprintf(buf, "DVP");

    for (i = 0; i < 256; i ++) {      
        uint16_t distance = htons((uint16_t)DistVector[i]);
        memcpy(buf + 3 + sizeof(uint16_t)*i, &distance, sizeof(uint16_t));
    }

	return 3 + sizeof(uint16_t)*256;
}

unsigned int getFromSendBuf(unsigned char *buf, uint16_t *destID, char *msg, unsigned int buf_length)
{
    unsigned int msgLen = buf_length - 4 - sizeof(uint16_t);
    buf += 4;
    memcpy(destID, buf, sizeof(uint16_t));
    *destID = ntohs(*destID);
    memcpy(msg, buf + sizeof(uint16_t), msgLen);
    return msgLen;
}

void writeFile(char *buf, unsigned int buf_length)
{
    FILE *fp = fopen(filename, "a");
    fwrite(buf, sizeof(char), buf_length, fp);
    fclose(fp);
}

void writeReceived(char *msg, unsigned int msgLen)
{
    char buf[1000];
    unsigned int count;
    
    memset(buf, '\0', 1000);
    count = (unsigned int)sprintf(buf, "receive packet message ");
    strncpy(buf + count, msg, msgLen);
    buf[count + msgLen] = '\n';
    
    writeFile(buf, count + msgLen + 1);
}

void writeUnreachable(uint16_t dest)
{
    char buf[1000];
    unsigned int count;
    
    memset(buf, '\0', 1000);
    count = (unsigned int)sprintf(buf, "unreachable dest %"PRIu16"\n", dest);
    
    writeFile(buf, count);
}

void writeSend(uint16_t dest, uint16_t nexthop, char *msg, unsigned int msgLen)
{
    char buf[1000];
    unsigned int count;
    
    memset(buf, '\0', 1000);
    count = (unsigned int)sprintf(buf, "sending packet dest %"PRIu16" nexthop %"PRIu16" message ", dest, nexthop);
    strncpy(buf + count, msg, msgLen);
    buf[count + msgLen] = '\n';
    
    writeFile(buf, count + msgLen + 1);
}

void writeForward(uint16_t dest, uint16_t nexthop, char *msg, unsigned int msgLen)
{
    char buf[1000];
    unsigned int count;
    
    memset(buf, '\0', 1000);
    count = (unsigned int)sprintf(buf, "forward packet dest %" PRIu16 " nexthop %" PRIu16 " message ", dest, nexthop);
    strncpy(buf + count, msg, msgLen);
    buf[count + msgLen] = '\n';
    
    writeFile(buf, count + msgLen + 1);
}

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 100 * 1000 * 1000; 
	while(1)
	{
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}      
}

void* propagateDistVector(void* unusedParam)
{
	struct timespec sleepFor;
	int i;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 200 * 1000 * 1000; 
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);

	while(1)
	{
		int length = DV_Buf(dvbuf);
		for(i=0;i<256;i++)
			if ((Neighbor[i] == 1) && (i!=globalMyID))
				sendto(globalSocketUDP, dvbuf, length, 0,
				  		(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));

		nanosleep(&sleepFor, 0);
	}
}

int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
   
    if (x->tv_usec < y->tv_usec) {
    	int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    	y->tv_usec -= 1000000 * nsec;
    	y->tv_sec += nsec;
  	}
  	if (x->tv_usec - y->tv_usec > 1000000) {
    	int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    	y->tv_usec += 1000000 * nsec;
    	y->tv_sec -= nsec;
  	}

  	result->tv_sec = x->tv_sec - y->tv_sec;
  	result->tv_usec = x->tv_usec - y->tv_usec;

  	return x->tv_sec < y->tv_sec;
}

void* checkNeighborAlive(void* unusedParam)
{
    int i, j;

    struct timeval current;
	struct timeval result;
    struct timespec sleepFor;
	
    sleepFor.tv_sec = 0;
    sleepFor.tv_nsec = 700 * 1000 * 1000; 
	nanosleep(&sleepFor, 0);
    nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	nanosleep(&sleepFor, 0);
	

    while (1) {
		
		gettimeofday(&current, 0);

        for (i = 0; i < 256; i ++) {
            if (globalMyID == i) {
                continue;
            }
			
			if (timeval_subtract(&result, &current, &globalLastHeartbeat[i]) == 0) {

            	if ((result.tv_sec > 1) && (Neighbor[i]==1) ) {
				
                	DistVector[i] = INT16_MAX;
					Neighbor[i] = INT16_MAX;
					forwardTable[i].cost = -1;
					forwardTable[i].nextHop = -1;

					for (int j=0; j<256; j++){
						if (globalMyID == j || i == j){
							continue;
						}
						if (forwardTable[j].nextHop == i){
							DistVector[j] = INT16_MAX;
							forwardTable[j].cost = -1;
							forwardTable[j].nextHop = -1;
						}
					}				
				}
            }
        }

		int length = DV_Buf(dvbuf);
		for(j=0;j<256;j++)
			if ((Neighbor[j] == 1) && (j!=globalMyID))
				sendto(globalSocketUDP, dvbuf, length, 0,
				  		(struct sockaddr*)&globalNodeAddrs[j], sizeof(globalNodeAddrs[j]));

        nanosleep(&sleepFor, 0);
    }
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];

	int bytesRecvd;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
		recvBuf[0] = '\0';
		
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
			
			if (heardFrom != globalMyID) {
				DistVector[heardFrom] = costTable[heardFrom];
				Neighbor[heardFrom] = 1;
				forwardTable[heardFrom].cost = DistVector[heardFrom];
				forwardTable[heardFrom].nextHop = heardFrom;
			}

			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp(recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			
            uint16_t destID;
            char msg[1024];
            memset(msg, '\0', 1024);
            unsigned int msgLen = getFromSendBuf(recvBuf, &destID, msg, bytesRecvd);
            
            //TODO send the requested msg to the requested destination node
            // ...
            
            if (globalMyID == (int)destID) {
                writeReceived(msg, msgLen);
            } 
			else {
                int nexthop = forwardTable[destID].nextHop;
                
                if (nexthop == -1) {
                    writeUnreachable(destID);
                } 
				else {
					sendto(globalSocketUDP, recvBuf, bytesRecvd, 0, 
						(struct sockaddr*)&globalNodeAddrs[nexthop], sizeof(globalNodeAddrs[nexthop]));
                    if (strncmp(fromAddr, "10.0.0.10", 9) == 0) {
                        writeSend(destID, (uint16_t)nexthop, msg, msgLen);
                    } else {
                        writeForward(destID, (uint16_t)nexthop, msg, msgLen);
                    }
                }
            }
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
		}
		
		else if(!strncmp(recvBuf, "DVP", 3))
		{
			uint16_t MVect[256];
			int i, j;

			for (i = 0; i < 256; i ++) {
        		memcpy(&MVect[i], recvBuf + 3 + sizeof(uint16_t) * i, sizeof(uint16_t));    		
	        	MVect[i] = ntohs(MVect[i]);
   			}

			int cost = costTable[heardFrom];

			for (i = 0; i < 256; i ++) {
				if (MVect[i] < INT16_MAX) {
					if (DistVector[i] > cost + MVect[i]) {
						DistVector[i] = cost + MVect[i];
						forwardTable[i].cost = DistVector[i];
						forwardTable[i].nextHop = heardFrom;
					}
					//Tie-breaking rule
					if (DistVector[i] == cost + MVect[i]) {
						if (forwardTable[i].nextHop > heardFrom)
							forwardTable[i].nextHop = heardFrom;
						if (forwardTable[i].nextHop == -1 && MVect[i] == 0){
							forwardTable[i].cost = DistVector[i];
							forwardTable[i].nextHop = heardFrom;
						}
					}
				}
		
				if (forwardTable[i].nextHop == heardFrom &&
							forwardTable[i].cost > 0 && MVect[i] == INT16_MAX) {
								DistVector[i] = INT16_MAX;
								forwardTable[i].cost = -1;
								forwardTable[i].nextHop = -1;

								int length = DV_Buf(dvbuf);
								for(j=0;j<256;j++)
									if ((Neighbor[j] == 1) && (j!=globalMyID))
										sendto(globalSocketUDP, dvbuf, length, 0,
				  							(struct sockaddr*)&globalNodeAddrs[j], sizeof(globalNodeAddrs[j]));
				}
			}
		}
	}
	//(should never reach here)
	close(globalSocketUDP);
}
