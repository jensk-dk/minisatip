
#ifndef NETCEIVERCLIENT_H
#define NETCEIVERCLIENT_H

#include "adapter.h"

void find_tsfile_adapter(adapter **a);

// One per Tuning Params->Filename in config file
typedef struct struct_tsfile_freq {
  transponder tp;
  char fileName[1024];
} STsFileFreq;

// One per TSFile adapter
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
  int fileFreqIndex; // Index into fileFreqs or -1 if no current freq/file
  int runThread;
} STsfile;

#endif
