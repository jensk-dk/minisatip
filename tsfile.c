/*
 * Copyright (C) 2017 Jens Kristian Jensen <jensk@futarque.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <ifaddrs.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>

#include "adapter.h"
#include "tsfile.h"

extern struct struct_opts opts;

extern const char * gConfFile;

STsFileFreq fileFreqs[MAX_ADAPTERS];

STsfile *ts[MAX_ADAPTERS];
#define TS ts[ad->id]
#define READ_SIZE 188*100*5

char *trimwhitespace(char *str)
{
  char *end;
  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}


// Find file corresponding to tuner parameters
// For now we only compare frequency, and the other parameters are don't cares
// but we could be strict here if we wanted to
int find_tsfile(transponder *tp) {
  for(int i=0;i<MAX_ADAPTERS;i++) {
    printf("%d Comparing %d %d\n", i, fileFreqs[i].tp.freq, tp->freq);
    if(fileFreqs[i].tp.freq == tp->freq) {
      return i;
    }
  }
  return -1;
}

// Returns number of adapters
int parse_config_file(fe_delivery_system_t * oFeds) {
  FILE* fp;
  char buffer[1024];
  int numTuners=1;
 
  // Test code
  //fileFreqs[0].tp.freq = 538000;
  //strncpy(fileFreqs[0].fileName, "m6_w9_arte_fr5_6ter.ts", 1024);
  //return 1;
  
  /// Actual code
  
  fp = fopen(gConfFile, "r");

  if(fp==0) {
      
      LOGL(0, "No Config file - no tsfile adapters created %s",gConfFile);
      return 0;
  }
  int numParams = 0;
  int numFilenames = 0;
  int lineNum=0;
  const char *numTunersKeyword = "NUM_TUNERS";
  const char *tunerCapsKeyword = "TUNER_CAPS";
  while(fgets(buffer, 1024, (FILE*) fp)) {
    lineNum++;
    printf("%s\n", buffer);
    char *realChars = buffer;
    realChars = trimwhitespace(realChars);

    char *savePtr;
    // Comment
    if(realChars[0] == '#' || (!strncmp(realChars, "//", 2)) || strlen(realChars)==0) {
      continue;
    }
    // NUM_TUNERS
    if(!strncmp(realChars, numTunersKeyword, strlen(numTunersKeyword))) {
        char *keyword = strtok_r(realChars, "=", &savePtr);
        char *value = strtok_r(NULL, "=", &savePtr);
        if(value != NULL) {
            numTuners = atoi(value);
        }
        LOGL(0, "numTuners set to %d from config file", numTuners);
        continue;
    }
    // TUNER_CAPS
    if(!strncmp(realChars, tunerCapsKeyword, strlen(tunerCapsKeyword))) {
        char *keyword = strtok_r(realChars, "=", &savePtr);
        char *value = strtok_r(NULL, "=", &savePtr);
        value = trimwhitespace(value);
        if(value != NULL) {
            printf("CHECKing '%s'\n",value);
            if(strcmp("DVBT",value)==0){
                oFeds[0] = SYS_DVBT;
            } else if(strcmp("DVBT2",value)==0){
                oFeds[0] = SYS_DVBT2;
            } else if(strcmp("DVBC",value)==0){
                oFeds[0] = SYS_DVBC_ANNEX_AC;
            } else if(strcmp("DVBC2",value)==0){
                oFeds[0] = SYS_DVBC_ANNEX_AC;
            } else if(strcmp("DVBS",value)==0){
                oFeds[0] = SYS_DVBS;
            } else if(strcmp("DVBS2",value)==0){
                oFeds[0] = SYS_DVBS2;
            } else if(strcmp("ATSC",value)==0){
                oFeds[0] = SYS_ATSC;
            } else if(strcmp("ISDBT",value)==0){
                oFeds[0] = SYS_ISDBT;
            }
            
        }
        LOGL(0, "tunerCaps set to %d from config file", oFeds[0]);
        continue;
    }
    // Tuner params
    if(realChars[0] == '?') {
      if(numParams != numFilenames) {
	LOGL(0, "cfgfile line %d: Params and filenames must alternate (%d/%d)", lineNum, numParams, numFilenames);
      }
      int retVal = detect_dvb_parameters(realChars,  &fileFreqs[numParams].tp);
      if(retVal != 0) {
	LOGL(0, "cfgfile - Error parsing params:", realChars);
      }
      numParams++;
      continue;
    }
    // Filename
    if(numParams-1 != numFilenames) {
      LOGL(0, "cfgfile line %d: Params and filenames must alternate (%d/%d)", lineNum, numParams, numFilenames);
    } else {
      FILE *fp2= fopen(realChars, "r");
      if(fp2==0) {
	LOGL(0, "cfgfile line %d: Unable to open file %s for frequency %d", lineNum, realChars, fileFreqs[numFilenames].tp.freq);
      } else {
	strncpy(fileFreqs[numFilenames].fileName, realChars, 1024);
	printf("fileFreqs[%d].fileName is now %s\n", numFilenames, fileFreqs[numFilenames].fileName);
	fclose(fp2);
      }
    }
    numFilenames++;
  }
  
  fclose(fp);
  return numTuners;
}

long long int get_time_ms() {
  struct timespec tm;
  clock_gettime(CLOCK_REALTIME, &tm);
  return tm.tv_sec*1000 + tm.tv_nsec / 1000000;
}

long long int get_pcr_base(unsigned char *buffer, size_t len, int *pcrPid, int *pcrIndex) {
  int i=0;
  do {
    if(buffer[i] == 0x47) {
      long long int pcrBase=0;
      int errorIndicator = buffer[i+1]>>7;
      int pid = ((buffer[i+1] & 0x1F) << 8) | buffer[i+2];
      int j=0;
      //      LOGL(0, "tsfile: pid %X - pcrPid=%X", pid, *pcrPid);
      if((*pcrPid != -1 && pid != *pcrPid) || errorIndicator==1) {
	//LOGL(0, "tsfile: skipping pid %X - pcrPid=%d, tei=%d", pid, *pcrPid, errorIndicator);
	i+=188;
	continue;
      }      int adaption_field_control = (buffer[i+3] & 0x30) >> 4;
      if(adaption_field_control == 0x02 || adaption_field_control == 0x03) {
	unsigned char adaption_field_length = buffer[i+4];
	int hasPcr = buffer[i+5] & 0x10; // Is PCR flag set in adaptation field flags?
	//LOGL(0, "pcrBase hasPcr=%d afc=%d i=%d", hasPcr, adaption_field_control, i);
	if(hasPcr) {
	  if(*pcrPid == -1) {
	    *pcrPid = pid;
	  }
	  for(j=0;j<4;j++) {
	    //LOGL(0, "pcrBase=%X", pcrBase);
	    pcrBase = pcrBase << 8;
	    pcrBase += buffer[i+6+j];
	    //LOGL(0, "pcrBase=%X", pcrBase);
	  }
	  
	  pcrBase = pcrBase << 1;
	  pcrBase += ((buffer[i+6+j] & 0x80) >> 7);
	  //LOGL(0, "pcrBase=%X", pcrBase);
	  *pcrIndex = i;
	  return pcrBase;
	}
      }
    }
    i+=188;
  } while(i<len);
  return -1; 
}

void *tsfile_thread(void *arg) {
  size_t lw=0;
  size_t lr=0;
  FILE *fp;
  int i=0;
  STsfile *ts;
  int pcrByteIndex=0;
  long long int usleepDelay=10*1000;
  if (arg)
    ts = (STsfile *) arg;
  char *fileName = fileFreqs[ts->fileFreqIndex].fileName;
  printf("fileFreqIndex = %d\n", ts->fileFreqIndex);
  LOGL(0, "tsfile thread %s created for file %s", ts->threadName, fileName);
  unsigned char buffer[READ_SIZE];
  memset(buffer, sizeof(buffer), 0x00);
  ts->pcrPid=-1;
  ts->lastPcr=-1;
  //fp = fopen("dvr62.ts", "r");
  fp = fopen(fileName, "r");
  if(fp==0) {
    LOGL(0, "failed to open ts file: %s", fileName);
  }
  while(ts->runThread) {
    long long int bufPos = 0;
    do {
    /* Read from ts file */
      //LOGL(0, "tsfile: reading %d bytes", READ_SIZE);
      bufPos = ftell(fp); // Index of buffer into file
      lr = fread(buffer, 1, READ_SIZE, fp);
      //LOGL(0, "tsfile: read %d wanted %d", lr, READ_SIZE);
      if(lr != READ_SIZE) { // Yeah, we loose the last chunk - so what!
	LOGL(0, "tsfile: rewinding file - read %d wanted %d", lr, READ_SIZE);
	ts->pcrPid=-1;
	ts->lastPcr=-1;
	fseek(fp, 0, SEEK_SET);
	break;
      }
      if(buffer[0] != 0x47) {
	LOGL(0, "tsfile: No sync on TS file - %d.\n", i);
	fseek(fp, -(READ_SIZE-1), SEEK_CUR);
      }
    } while(buffer[0] != 0x47);
    
    if(i==188) {
      LOGL(0, "tsfile: No sync on TS file. Exiting TS read thread :-/\n");
      break;
    }
    long long int pcr = get_pcr_base(buffer, READ_SIZE, &ts->pcrPid, &pcrByteIndex);
    long long int now = get_time_ms();
    long long int bytesPerSec = 0;
    long long int pcrDiff = -100000;
    if(pcr != -1) {
      pcrByteIndex = bufPos + pcrByteIndex;
      //LOGL(0, "tsfile: pcrBase pcrPid=%X pcr=%lld index=%d (diff=%lld tdiff=%lld bdiff=%d)", ts->pcrPid, pcr, pcrByteIndex, pcr - ts->lastPcr, now - ts->lastPcrMs, pcrByteIndex - ts->lastPcrByte);
      if(ts->lastPcr != -1) {
	long long int pcrDiff = pcr - ts->lastPcr;
	long long int byteDiff = pcrByteIndex - ts->lastPcrByte;
	bytesPerSec = byteDiff * 90000 / pcrDiff;
	if(pcrDiff > 0 && pcrDiff < 90000) {
	  //LOGL(0, "tsfile: pcrBase Simple PCR diff says %lld bytes/sec = %lld bits/s", bytesPerSec, bytesPerSec*8);
	  if(bytesPerSec != 0) {
	    usleepDelay = READ_SIZE * 1000ull *1000 / bytesPerSec;
	    //LOGL(0, "tsfile: pcrBase Computed sleep delay of %lld usec", usleepDelay);
	  }	  
	} else {
	  //LOGL(0, "tsfile: pcrBase PCR jump - disregard");
	}
      }
      ts->lastPcr = pcr;
      ts->lastPcrMs = now;
      ts->lastPcrByte = pcrByteIndex;
    }
    if(ts->pcrPid != -1 && ((now - ts->lastPcrMs) > 1000) ) {
      LOGL(0, "tsfile: pcrBase No PCR for pid %d for one second - selecting a new", ts->pcrPid);
      ts->pcrPid = -1;
      ts->lastPcr = -1;
      ts->lastPcrMs = now;
    }
    /* write TS data to DVR pipe */
    //LOGL(0, "tsfile: writing %d bytes to fd=%d", READ_SIZE, ts->pwfd);
    lw = write(ts->pwfd, buffer, READ_SIZE);
    if (lw != READ_SIZE) LOGL(0, "tsfile: not all data forwarded (%s)", strerror(errno));
    LOGL(0, "tsfile: delaying %d uSecs", usleepDelay);
    usleep(usleepDelay);
  }
  fclose(fp);
  LOGL(0, "tsfile thread %s exiting", ts->threadName);
}

int tsfile_open_device(adapter *ad)
{
  int pipe_fd[2];
  LOGL(0, "tsfile: open_device");
  TS->want_commit = 0;
  
  /* create DVR pipe for TS thread to write to */
  if (pipe2 (pipe_fd, O_NONBLOCK)) LOGL (0, "tsfile: creating pipe failed (%s)", strerror(errno));
  if (-1 == fcntl (pipe_fd[0], F_SETPIPE_SZ, 5 * READ_SIZE))
    LOGL (0, "tsfile pipe buffer size change failed (%s)", strerror(errno));
  ad->dvr = pipe_fd[0]; // read end of pipe
  TS->pwfd = pipe_fd[1]; // write end of pipe
  TS->fileFreqIndex = -1;
  LOGL(1, "tsfile: created DVR pipe for adapter %d  -> dvr: %d", ad->id, ad->dvr);
  LOGL(1, "tsfile: TS->pwfd = %d", TS->pwfd);

  for(int i=0;i<MAX_PIDS;i++) {
    TS->npid[i] = 0xFFFF;
  }
  return 0;
}

int tsfile_set_pid(adapter *ad, uint16_t pid)
{
	int aid = ad->id;
	LOGL(0, "tsfile: set_pid for adapter %d, pid %d", aid, pid);
	int foundPid=0;
	for(int i=0;i<MAX_PIDS;i++) {
	    if(TS->npid[i] == pid) {
	      printf("Already found at %d\n", i);
	      foundPid=1;
	      break;
	    }
	}
	if(foundPid==0) {
	  for(int i=0;i<MAX_PIDS;i++) {
	    if(TS->npid[i] == 0xFFFF) {
	      printf("Set at %d\n", i);
	      TS->npid[i] = pid;
	      break;
	    }
	  }
	}
	return aid + 100; // This is really a DMX fd!?!?!
}

int tsfile_del_pid(int fd, int pid)
{
	int i, hit = 0;
	adapter *ad;
	fd -= 100;
	ad = get_adapter(fd);
	if (!ad)
		return 0;
	LOGL(0, "tsfile: del_pid for aid %d, pid %d", fd, pid);
	for(int i=0;i<MAX_PIDS;i++) {
	  if(TS->npid[i] == pid) {
	    printf("Deleted at %d\n", i);
	    TS->npid[i] = 0xFFFF;
	    break;
	  }
	}

	return 0;
}

void tsfile_commit(adapter *ad)
{
  

  int numPids=0;
  for(int i=0;i<MAX_PIDS;i++) {
    if(TS->npid[i] != 0xFFFF) {
      numPids++;
    }
  }
  LOGL(0, "tsfile: commit adapter %d - numPids=%d", ad->id, numPids);

  void *retVal;
  int rv;

#if 0
  if(numPids == 0) {
    if(TS->readThread) {
      LOGL(0, "tsfile: Stopping thread: %s", TS->threadName);
      TS->runThread = 0;
      pthread_join(TS->readThread, &retVal);
      LOGL(0, "tsfile: Stopped thread: %s", TS->threadName);
    }
  } else {
    if(!TS->readThread) {
      if(TS->fileFreqIndex != -1) {
	LOGL(0, "tsfile: creating read thread for adapter %d", ad->id);
	TS->runThread = 1;
	snprintf(TS->threadName, 20, "TSFileThread%d", ad->id);
	if ((rv = pthread_create(&tid, NULL, &tsfile_thread, TS))) {
	  LOG("Failed to create thread: %s, error %d %s", TS->threadName, rv, strerror(rv));    
	}
	TS->readThread = tid;
	ad->status = FE_HAS_SIGNAL;
	ad->strength = 100;
	ad->snr = 64;
	ad->ber = 1000000;
      } else {
	ad->strength = 0;
	ad->status = 0;
	ad->snr = 0;
	ad->ber = 0;
      }
    }
  }
#endif

  
  return;
}

int tsfile_tune(int aid, transponder * tp)
{
  pthread_t tid;
  int rv;
  void *retVal;
  LOGL(0, "tsfile: tune adapter id=%d freq=%d", aid, tp->freq);
  adapter *ad = get_adapter(aid);
  if (!ad)
    return 1;

#if 1
  if(TS->readThread) {
    LOGL(0, "tsfile: Stopping thread: %s", TS->threadName);
    TS->runThread = 0;
    pthread_join(TS->readThread, &retVal);
    LOGL(0, "tsfile: Stopped thread: %s", TS->threadName);
  }
#endif
  TS->fileFreqIndex = find_tsfile(tp);
  LOGL(0, "Found fileFreqIndex = %d\n", TS->fileFreqIndex);
  #if 1
  if(TS->fileFreqIndex != -1) {
    LOGL(0, "tsfile: creating read thread for adapter %d", ad->id);
    TS->runThread = 1;
    snprintf(TS->threadName, 20, "TSFileThread%d", ad->id);
    if ((rv = pthread_create(&tid, NULL, &tsfile_thread, TS))) {
      LOG("Failed to create thread: %s, error %d %s", TS->threadName, rv, strerror(rv));    
    }
    TS->readThread = tid;
    ad->status = FE_HAS_SIGNAL;
    ad->strength = 100;
    ad->snr = 64;
    ad->ber = 1000000;
  } else {
    LOGL(0, "tsfile: No matching frequency for adapter %d at freq=%d - not creating read thread", ad->id, tp->freq);
    ad->strength = 0;
    ad->status = 0;
    ad->snr = 0;
    ad->ber = 0;
  }
#endif
  return 0;
}

fe_delivery_system_t tsfile_delsys(int aid, int fd, fe_delivery_system_t *sys)
{
	return 0;
}

int tsfile_close(adapter *ad)
{
    void *retVal;
    
    LOGL(0, "tsfile: delete receiver instance for adapter %d", ad->id);
    if(TS->readThread) {
      LOGL(0, "tsfile: Stopping thread: %s", TS->threadName);
      TS->runThread = 0;
      pthread_join(TS->readThread, &retVal);
      LOGL(0, "tsfile: Stopped thread: %s", TS->threadName);
    }
    
    /* close DVR pipe */
    close(TS->pwfd);
    close(ad->dvr);
    
    ad->strength = 0;
    ad->status = 0;
    ad->snr = 0;
    ad->ber = 0;
    return 0;
}

void find_tsfile_adapter(adapter **a) {
	int i, k, n, na;
	adapter *ad;

    fe_delivery_system_t feds[1];
    feds[0] = SYS_DVBT;
	int numTuners = parse_config_file(feds);
    
	LOGL(0, "tsfile: %i tuners caps %i", numTuners,feds[0]);
	
	// add tsfile "tuners" to the list of adapters
	na = a_count;
	LOGL(0, "tsfile: adding ");
	for (n = 0; n < numTuners; n++)
	{

		if (na >= MAX_ADAPTERS)
			break;
		if (!a[na])
			a[na] = adapter_alloc();
		if (!ts[na])
			ts[na] = malloc1(sizeof(STsfile));

		ad = a[na];
		ad->pa = 0;
		ad->fn = 0;
		ts[na]->want_tune = 0;
		ts[na]->want_commit = 0;
		ts[na]->readThread = 0;
		//		ts[na]->adapter = ad;
		
		/* initialize signal status info */
		ad->strength = 0;
		ad->max_strength = 0xff;
		ad->status = 0;
		ad->snr = 0;
		ad->max_snr = 0xff;
		ad->ber = 0;

		/* register callback functions in adapter structure */
		ad->open = (Open_device) tsfile_open_device;
		ad->set_pid = (Set_pid) tsfile_set_pid;
		ad->del_filters = (Del_filters) tsfile_del_pid;
		ad->commit = (Adapter_commit) tsfile_commit;
		ad->tune = (Tune) tsfile_tune;
		ad->delsys = (Dvb_delsys) tsfile_delsys;
		ad->post_init = (Adapter_commit) NULL;
		ad->close = (Adapter_commit) tsfile_close;
		ad->type = ADAPTER_TSFILE;

		ad->sys[0] = feds[0];

		na++; // increase number of tuner count
		a_count = na;
	}

	for (; na < MAX_ADAPTERS; na++)
		if (a[na])
			a[na]->pa = a[na]->fn = -1;
}

