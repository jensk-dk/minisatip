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

#include "adapter.h"
#include "tsfile.h"

extern struct struct_opts opts;

STsfile *ts[MAX_ADAPTERS];
#define TS ts[ad->id]

void *tsfile_thread(void *arg) {
  size_t len = 188*5;
  size_t lw=0;
  if (arg)
    thread_name = (char *) arg;

  LOGL(0, "tsfile thread %s created", thread_name);
  unsigned char buffer[len];
  memset(buffer, sizeof(buffer), 0x00);
  buffer[0]=0x47;
  while(1) {
	/* write TS data to DVR pipe */
	lw = write(ts[0]->pwfd, buffer, len);
	if (lw != len) LOGL(0, "netceiver: not all data forwarded (%s)", strerror(errno));
	usleep(10000);
  }
}

int tsfile_open_device(adapter *ad)
{
  pthread_t tid;
  int rv;
  int pipe_fd[2];
  LOGL(0, "tsfile: open_device");
  TS->want_commit = 0;
  
  /* create DVR pipe for TS thread to write to */
  if (pipe2 (pipe_fd, O_NONBLOCK)) LOGL (0, "netceiver: creating pipe failed (%s)", strerror(errno));
  if (-1 == fcntl (pipe_fd[0], F_SETPIPE_SZ, 5 * 188 * 1024))
    LOGL (0, "tsfile pipe buffer size change failed (%s)", strerror(errno));
  ad->dvr = pipe_fd[0]; // read end of pipe
  TS->pwfd = pipe_fd[1]; // write end of pipe
  LOGL(1, "tsfile: created DVR pipe for adapter %d  -> dvr: %d", ad->id, ad->dvr);

  LOGL(1, "tsfile: creating read thread for adapter %d", ad->id);
  char *name="TSFile";
  if ((rv = pthread_create(&tid, NULL, &tsfile_thread, name))) {
    LOG("Failed to create thread: %s, error %d %s", name, rv, strerror(rv));

  }

  return 0;
}

int tsfile_set_pid(adapter *ad, uint16_t pid)
{
	int aid = ad->id;
	LOGL(0, "tsfile: set_pid for adapter %d, pid %d", aid, pid);
	return aid + 100;
}

int tsfile_del_pid(int fd, int pid)
{
	int i, hit = 0;
	adapter *ad;
	fd -= 100;
	ad = get_adapter(fd);
	if (!ad)
		return 0;
	LOGL(3, "tsfile: del_pid for aid %d, pid %d", fd, pid);

	return 0;
}

void tsfile_commit(adapter *ad)
{
  LOGL(0, "tsfile: commit adapter %d", ad->id);
  return;
}

int tsfile_tune(int aid, transponder * tp)
{
  LOGL(0, "tsfile: tune adapter id=%d freq=%d", aid, tp->freq);
  adapter *ad = get_adapter(aid);
  if (!ad)
    return 1;
  if(tp->freq == 538000) { // For now we only play back on 538MHz, which is a UHF channel
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
  return 0;
}

fe_delivery_system_t tsfile_delsys(int aid, int fd, fe_delivery_system_t *sys)
{
	return 0;
}

int tsfile_close(adapter *ad)
{
	LOGL(1, "tsfile: delete receiver instance for adapter %d", ad->id);
	return 0;
}

void find_tsfile_adapter(adapter **a) {
	int i, k, n, na;
	adapter *ad;
	char dbuf[2048];


	LOGL(0, "%s", dbuf);

	// add tsfile "tuners" to the list of adapters
	na = a_count;
	LOGL(0, "tsfile: adding ");
	for (n = 0; n < 1; n++)
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

		ad->sys[0] = SYS_DVBT;

		na++; // increase number of tuner count
		a_count = na;
	}
	LOGL(0, "%s", dbuf);

	for (; na < MAX_ADAPTERS; na++)
		if (a[na])
			a[na]->pa = a[na]->fn = -1;
}

