
#ifndef NETCEIVERCLIENT_H
#define NETCEIVERCLIENT_H

#include "adapter.h"

void find_tsfile_adapter(adapter **a);

typedef struct struct_tsfile
{
  int pwfd;			// file descriptor to writeable end of pipe for TS data
  uint16_t npid[MAX_PIDS];	// active pids
  int lp;				// number of active pids
  char want_tune, want_commit;	// tuning & and PID handling state machine
  pthread_t readThread;
  char threadName[20];
  int pcrPid;
  long long int lastPcr;
  int lastPcrByte;
  long long int lastPcrMs;
} STsfile;

#endif
