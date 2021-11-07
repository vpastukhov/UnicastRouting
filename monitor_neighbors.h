#define INT16_MAX 32676

#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>

struct ft_entry {
    int cost;
    int nextHop;
};

void* propagateDistVector(void* unusedParam);
void* announceToNeighbors(void* unusedParam);
void* checkNeighborAlive(void* unusedParam);
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);
void hackyBroadcast(const char* buffer, int length);
void listenForNeighbors();
unsigned int DV_Buf(char* buf);
unsigned int getFromSendBuf(unsigned char *buf, uint16_t *destID, char *msg, unsigned int buf_length);
void writeFile(char *buf, unsigned int buf_length);
void writeReceived(char *msg, unsigned int msgLen);
void writeUnreachable(uint16_t dest);
void writeForward(uint16_t dest, uint16_t nexthop, char *msg, unsigned int msgLen);


