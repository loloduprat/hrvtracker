// Copyright 2009 `date +paul@ant%m%y.sbrk.co.uk`
// Released under GPLv3
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>

#include "libant.h"

#define DFLTSPD 115200
#if defined linux
		#define DFLTDEV "/dev/ttyUSB0";
#elif defined WIN32
		#define DFLTDEV "COM3";
#elif defined __APPLE__
		#define DFLTDEV "/dev/cu.SLAB_USBtoUART";
#else
#error Unknown OS
#endif

struct anth
{
	int cookie;
	int maywrite;
	pthread_mutex_t maywriteM;

	pthread_mutex_t writerM;
	pthread_cond_t  writerC;

	pthread_mutex_t dataM;
	pthread_cond_t  dataC;

	struct rcvq *firstrcv;
	struct rcvq *lastrcv;

	pthread_mutex_t nextrcvM;
	struct rcvq *nextrcv;

	struct msgq *firstmsg;
	struct msgq *lastmsg;

	pthread_mutex_t nextsendM;
	struct msgq *nextsend;
	struct msgq *nexttx[MAXCHAN];
	struct msgq *lasttx[MAXCHAN];
	u8 txready[MAXCHAN];

	FDTYPE fd;

	pthread_t commthread;
};


static u16 antdbg = 0;
static int errlevel = 0;

#define DBG_WARN	(1 << 0)
#define DBG_FLOW	(1 << 1)
#define DBG_MSG		(1 << 2)
#define DBG_ACK		(1 << 3)
#define DBG_GET		(1 << 4)
#define DBG_RQ		(1 << 5)
#define DBG_WR		(1 << 6)
#define DBG_WQ		(1 << 7)
#define DBG_DATA	(1 << 8)
#define DBG_EVENT	(1 << 9)
#define DBG_TX		(1 << 10)


struct msgq {
	struct msgq *prev;
	struct msgq *next;
	struct antmsg msg;
};

struct rcvq {
	struct rcvq *prev;
	struct rcvq *next;
	struct rcvmsg msg;
};

static struct {
	u8 maxchan;
	u8 maxnet;
	u8 stdopt;
	u8 advopt;
	u8 adv2;
	u8 rsvd;
	u8 valid;
} cap;

static struct {
	u8 chan;
	u16 devno;
	u8 devtype;
	u8 manid;
} chid;

#ifdef WIN32
int
READ(HANDLE fd, char *b, DWORD l)
{
	DWORD wnr;
	A(ReadFile(fd, b, l, &wnr, NULL))
	return wnr;
}
int
WRITE(HANDLE fd, char *b, DWORD l)
{
	DWORD wnr;
	A(WriteFile(fd, b, l, &wnr, NULL))
	return wnr;
}
#else
#define READ read
#define WRITE write
#endif

/* send message over serial port */
static void
data_send(struct anth *h, u8 mesg, u8 *inbuf, u8 len)
{
	u8 buf[MAXMSG];
	ssize_t nw;
	int i;
	u8 chk = MESG_TX_SYNC;

	if (len > MAXMSG-3) {
		fprintf(stderr, "msg too long msg %x %d > %d-3 buf:", mesg, len, MAXMSG);
		for (i = 0; i < len; i++)
			fprintf(stderr, "%02x", inbuf[i]);
		fprintf(stderr, "\n");
		exit(1);
	}
		
	buf[0] = MESG_TX_SYNC;
	buf[1] = len; chk ^= len;
	buf[2] = mesg; chk ^= mesg;
	for (i = 0; i < len; i++) {
		buf[3+i] = inbuf[i];
		chk ^= inbuf[i];
	}
	buf[3+i] = chk;
	if (4+i != (nw=WRITE(h->fd, buf, 4+i))) {
		perror("failed write in data_send");
		exit(1);
	}
}

// sanity check the msg list structures
static void
chkq(struct anth *h, int line)
{
	struct msgq *q;
	char *crash = 0;
	int i, sz;

	for (q = h->firstmsg, sz = 0; q; q=q->next, sz++)
		if (q->msg.cookie != COOKIE) {
			fprintf(stderr, "bad cookie (%d) %x\n", line, q->msg.cookie);
			*crash = 0;
		}
	if ((sz > 10) && (antdbg & DBG_WARN)) fprintf(stderr, "large q size %d\n", sz);
	for (i = 0; i < MAXCHAN; i++) {
		for (q = h->nexttx[i], sz = 0; q; q=q->next, sz++)
			;
		if ((sz > 10) && (antdbg & DBG_WARN)) fprintf(stderr, "large txq[%d] size %d\n", i, sz);
	}
}

// debug function to print the send queue
static void
printq(struct anth *h)
{
	int got = 0;
	struct msgq *q;

	chkq(h, __LINE__);
	for (q = h->firstmsg; q; q=q->next)
		if (!((q->msg.flags & AM_REPLIED) || (q->msg.flags & AM_NOREPLY))) {
			fprintf(stderr, "q wait %x\n", q->msg.msg_id);
			got = 1;
		}
	if (!got)
		fprintf(stderr, "q empty\n");
}

// A (hopefully) previously sent message has received a response
// find the last message of that type and update it's receipt time
// This is for statistics/debugging. Not critical if race with lastmsg
// TODO: store the code too. Handle overlapping messages? ignore broadcasts
static void
ack(struct anth *h, u8 msgid, u8 code)
{
	struct msgq *q;

	q = h->nextsend;
	if (!q)
		q = h->lastmsg;
	for (; q; q=q->prev) {
		if ((q->msg.msg_id == msgid) || ((MESG_REQUEST_ID == q->msg.msg_id) &&
				(q->msg.REQUEST.message == msgid))) {
			q->msg.flags |= AM_REPLIED;
			q->msg.code = code;
			S(gettimeofday(&q->msg.replytime, 0));
			if (antdbg & DBG_ACK) fprintf(stderr,
				"Acking %x from %ld.%ld at %ld.%ld\n", msgid,
				q->msg.sendtime.tv_sec, (long)q->msg.sendtime.tv_usec,
				q->msg.replytime.tv_sec, (long)q->msg.replytime.tv_usec);
			if (errlevel != 0 && code != 0) {
				fprintf(stderr, "no exit on error code %x\n", code);
				//exit(1);
			}
			break;
		} else {
			if (antdbg  & DBG_ACK) fprintf(stderr, "q msg %x ! %x\n",
				q->msg.msg_id, msgid);
		}
	}
	if (antdbg & DBG_ACK) printq(h);
}

// read from serial port and consume valid messages
// discard anything prior to a valid message
static void
data_get(struct anth *h)
{
	static u8 buf[500];
	static int bufc = 0;
	int nr;
	int dlen;
	int sync;
	int j;
	unsigned char chk = 0;
	int found;
	int srch;
	int next;
	u8 msgid;
	u8 msglen;
	u8 chan;
	u8 origid;
	u8 code;
	struct rcvq *rq;
	fd_set readfds;
	int ready;
	struct timeval to;

	FD_ZERO(&readfds);
	FD_SET((FDCAST)h->fd, &readfds);
	to.tv_sec = 10;
	to.tv_usec = 0;
	ready = select((FDCAST)h->fd+1, &readfds, 0, 0, &to);
	if (!ready) {
		if (antdbg & DBG_GET) fprintf(stderr, "data_get timeout\n");
		return;
	}
	// read and append to buffer
	nr = READ(h->fd, buf+bufc, 20);
	if (nr > 0)
		bufc += nr;
	else {
		fprintf(stderr, "null read\n");
		exit(1);
	}
	if (bufc > 30) {
		if (antdbg & DBG_GET)
			fprintf(stderr, "bufc %d\n", bufc);
	}
	if (bufc > 300) {
		fprintf(stderr, "buf too long %d\n", bufc);
		for (j = 0; j < bufc; j++)
			fprintf(stderr, "%02x", buf[j]);
		fprintf(stderr, "\n");
		exit(EXIT_FAILURE);
	}

	/* some data in buf */
	/* search for possible valid messages */
  srch = 0;
	found = 0;
	while (srch < bufc) {
		/*printf("srch %d bufc %d\n", srch, bufc);*/
		for (sync = srch; sync < bufc; sync++) {
			if (buf[sync] == MESG_TX_SYNC) {
				/*fprintf(stderr, "bufc %d sync %d\n", bufc, sync);*/
				if (sync+1 < bufc && buf[sync+1] >= 1 && buf[sync+1] <= 13) {
					dlen = buf[sync+1];
					if (sync+3+dlen < bufc) {
						chk = 0;
						for (j = sync; j <= sync+3+dlen; j++)
							chk ^= buf[j];
						if (0 == chk) {
							found = 1; /* got a valid message*/
							break;
						} else {
							fprintf(stderr, "bad chk %02x\n", chk);
							for (j = sync; j <= sync+3+dlen; j++)
								fprintf(stderr, "%02x", buf[j]);
							fprintf(stderr, "\n");
						}
					}
				}
			}
		}
		if (found) {
			next = j;
			if (antdbg & DBG_GET) fprintf(stderr, "next %d %02x\n", next, buf[j-1]);
			/* got a valid message, see if any data needs to be discarded*/
			if (sync > srch) {
				fprintf(stderr, "\nDiscarding: ");
				for (j = 0; j < sync; j++)
					fprintf(stderr, "%02x", buf[j]);
				fprintf(stderr, "\n");
			}

			if (antdbg & DBG_GET) {
				fprintf(stderr, "got data: ");
				for(j = sync; j < sync+dlen+4; j++) {
					fprintf(stderr, "%02x", buf[j]);
				}
				fprintf(stderr, "\n");
			}
			// handle the message. TODO put this into its own function
			msgid = buf[sync+2];
			msglen = buf[sync+1];
			switch (msgid) {
				case MESG_CAPABILITIES_ID:
					A(sizeof cap >= msglen);
					memcpy(&cap, buf+sync+3, msglen);
					if (antdbg & DBG_FLOW) fprintf(stderr,
						"Capabilities maxchan: %d, maxnet: %d stdopt: %x advopt: %x"
						"adv2: %x, rsvd: %x\n",
						cap.maxchan, cap.maxnet, cap.stdopt, cap.advopt,
						cap.adv2, cap.rsvd);
					cap.valid = 1;
					ack(h, msgid, 0);
					break;
				case MESG_CHANNEL_ID_ID:
					A(sizeof chid >= msglen);
					memcpy(&chid, buf+sync+3, msglen);
					if (antdbg & DBG_FLOW) fprintf(stderr,
						"channel id ch#%d devno: %x devtype: %x manid: %x\n",
						chid.chan, chid.devno, chid.devtype, chid.manid);
					break;
				case MESG_RESPONSE_EVENT_ID:
					A(3 == msglen);
					chan = buf[sync+3];
					origid = buf[sync+4];
					code = buf[sync+5];
					if (1 == origid) {
						if (antdbg & DBG_FLOW) fprintf(stderr,
							"channel event ch#%d code %x\n", chan, code);
						if (EVENT_RX_FAIL == code) {
							if (antdbg & DBG_FLOW) fprintf(stderr, "RX FAIL, incorrect message period?\n");
						} else if (EVENT_TX == code || EVENT_TRANSFER_TX_COMPLETED == code) {
							// ready for next broadcast
							h->txready[chan] = 1;
							pthread_cond_signal(&h->writerC);
						}
					} else {
						if (antdbg & DBG_EVENT) fprintf(stderr,
							"response event ch#%d id %x code %x\n", chan, origid, code);
						ack(h, origid, code);
						// if OPEN_CHANNEL ACKed then allow transmission on that channel
						if (MESG_OPEN_CHANNEL_ID == origid && 0 == code) {
							if (antdbg & DBG_EVENT) fprintf(stderr, "enable xmit ch#%d\n", chan);
							h->txready[chan] = 1;
						}
					}
					break;
				case MESG_BROADCAST_DATA_ID:
					A(9 == msglen);
					chan = buf[sync+3];
					A(rq = (struct rcvq *)malloc(sizeof(struct rcvq)));
					rq->next = 0;
					rq->msg.rtype = msgid;
					rq->msg.chan = chan;
					S(gettimeofday(&rq->msg.rcvtime, 0));
					memcpy(rq->msg.data, buf+sync+4, 8);
					if (h->lastrcv) {
						rq->prev = h->lastrcv;
						rq->prev->next = h->lastrcv = rq;
						pthread_mutex_lock(&h->nextrcvM);
						if (!h->nextrcv)
							h->nextrcv = rq;
						pthread_mutex_unlock(&h->nextrcvM);
						if (antdbg & DBG_RQ) fprintf(stderr, "add rq %p ch#%d f %p l %p\n",
							rq, rq->msg.chan, h->firstrcv, h->lastrcv);
					} else {
						rq->prev = 0;
						pthread_mutex_lock(&h->nextrcvM);
						h->nextrcv = h->firstrcv = h->lastrcv = rq;
						pthread_mutex_unlock(&h->nextrcvM);
						if (antdbg & DBG_RQ) fprintf(stderr,
							"init rq %p msg %x f %p l %p\n",
							rq, rq->msg.chan, h->firstrcv, h->lastrcv);
					}
					if (antdbg & DBG_RQ) {
						int i;
						fprintf(stderr, "BCAST ch#%d d:", chan);
						for (i = 0; i < 8; i++)
							fprintf(stderr, "%02x", rq->msg.data[i]);
						fprintf(stderr, "\n");
					}
					if (h->nextrcv) {
						pthread_cond_signal(&h->dataC);
					}
					break;
				default:
					fprintf(stderr, "unhandled message %x\n", buf[sync+2]);
			}
			srch = next;
		} else {
			if (antdbg & DBG_GET) fprintf(stderr, "No valid message found\n");
			return;
		}
	}
	if (found) {
		if (antdbg & DBG_RQ) fprintf(stderr, "allowing write %d -> %d\n", h->maywrite, 1);
		pthread_mutex_lock(&h->maywriteM);
		h->maywrite = 1;
		pthread_mutex_unlock(&h->maywriteM);
		if (h->nextsend) {
			if (antdbg & DBG_RQ) fprintf(stderr, "nextsend %p signal writer\n",
				h->nextsend);
			pthread_cond_signal(&h->writerC);
		} else {
			if (antdbg & DBG_RQ) fprintf(stderr,
				"nextsend %p not signalling writer\n", h->nextsend);
		}
	}
	if (next < bufc) {
		if (antdbg & DBG_GET) fprintf(stderr, "shifting bufc %d next %d\n",
			bufc, next);
		memmove(buf, buf+next, bufc-next);
		bufc -= next;
	} else {
		if (antdbg & DBG_GET) fprintf(stderr, "clearing bufc %d next %d\n",
			bufc, next);
		bufc = 0;
	}
}

// serial port reader thread
static void *
reader(struct anth* h)
{
	for(;;) {
		data_get(h);
	}
	return 0;
}

// message writer thread
static void *
writer(struct anth *h)
{
	struct antmsg msg;
	struct msgq *ntx;
	int i;

	for(;;) {
		// deal with outstanding xmits first
		pthread_mutex_lock(&h->maywriteM);
		for (i=0; i < MAXCHAN; i++) {
			if (h->maywrite && h->txready[i] && h->nexttx[i]) {
				ntx = h->nexttx[i];
				pthread_mutex_unlock(&h->maywriteM); // unlock in case write blocks
				data_send(h, ntx->msg.msg_id, ntx->msg.d, ntx->msg.len);
				h->txready[i] = 0;
				pthread_mutex_lock(&h->maywriteM);
				h->nexttx[i] = ntx->next;
				free(ntx);
			}
		}
		pthread_mutex_unlock(&h->maywriteM);

		// then command messages
		if (h->maywrite && h->nextsend) {
			pthread_mutex_unlock(&h->maywriteM);
			memcpy(&msg, &h->nextsend->msg, sizeof msg);
			if (msg.flags & AM_PREDELAY) {
				if (antdbg & DBG_WR) fprintf(stderr, "predelay %d\n", msg.predelay);
				usleep(msg.postdelay*1000);
			}
			S(gettimeofday(&h->nextsend->msg.sendtime, 0));
			data_send(h, msg.msg_id, msg.d, msg.len);
			if (msg.flags & AM_POSTDELAY) {
				if (antdbg & DBG_WR) fprintf(stderr, "postdelay %d\n", msg.postdelay);
				usleep(msg.postdelay*1000);
			}
			if (!(msg.flags & AM_NOREPLY)) {
				// this message needs a reply, disable maywrite
				if (antdbg & DBG_WR) fprintf(stderr,
					"disable maywrite %d -> 0 for msg %x\n", h->maywrite, msg.msg_id);
				pthread_mutex_lock(&h->maywriteM);
				h->maywrite = 0;
				pthread_mutex_unlock(&h->maywriteM);
			} else {
			}
			h->nextsend = h->nextsend->next;
		} else {
			pthread_mutex_unlock(&h->maywriteM);
			// can't write, so wait for signal
			pthread_mutex_lock(&h->writerM);
			if (antdbg & DBG_WR) fprintf(stderr, "waiting for writer signal\n");
				pthread_cond_wait(&h->writerC, &h->writerM);
			if (antdbg & DBG_WR) fprintf(stderr, "\nwriter signalled\n");
			pthread_mutex_unlock(&h->writerM);
		}
	}
	return 0;
}

static void
posttx(struct anth *h, u8 chan, struct antmsg *msg)
{
	int i;
	struct msgq *q;

	A(q = (struct msgq *)malloc(sizeof(struct msgq)));
	q->next = 0;
	memcpy(&(q->msg), msg, sizeof(struct antmsg));
	chkq(h, __LINE__);
	if (h->nexttx[chan]) {
		q->prev = h->lasttx[chan];
		q->prev->next = q;
		h->lasttx[chan] = q;
		if (antdbg & DBG_TX) {
			fprintf(stderr, "add tx %p msg %x n %p l %pd:",
				q, q->msg.msg_id, h->nexttx[chan], h->lasttx[chan]);
			for (i = 0; i < 9; i++)
				fprintf(stderr, "%02x", q->msg.d[i]);
			fprintf(stderr, "\n");
		}
	} else {
		// empty xmit queue
		q->prev = 0;
		h->nexttx[chan] = h->lasttx[chan] = q;
		if (antdbg & DBG_TX) fprintf(stderr, "init tx %p msg %x f %p l %p\n",
			q, q->msg.msg_id, h->nexttx[chan], h->lasttx[chan]);
		// start writer on first message
		//pthread_mutex_lock(&maywriteM);
		//maywrite++;
		//pthread_mutex_unlock(&maywriteM);
	}
	if (antdbg & DBG_TX) {
		fprintf(stderr, "POSTMSG: %x flags: %x len: %d time: %ld.%ld data: ",
			msg->msg_id, msg->flags, msg->len, msg->reqtime.tv_sec,
			(long)msg->reqtime.tv_usec);
		for (i = 0; i < msg->len; i++)
			fprintf(stderr, "%02x ", msg->d[i]);
		fprintf(stderr, "\n");
	}
	if (antdbg & DBG_TX) {
		fprintf(stderr, "Queue on ch#%d\n", chan);
		for (q = h->nexttx[chan]; q; q = q->next)
			fprintf(stderr, "%p msg %x flags %x\n", q, q->msg.msg_id, q->msg.flags);
	}
	pthread_cond_signal(&h->writerC);
}

static void
postmsg(struct anth *h, struct antmsg *msg)
{
	int i;
	struct msgq *q;

	A(q = (struct msgq *)malloc(sizeof(struct msgq)));
	q->next = 0;
	memcpy(&(q->msg), msg, sizeof(struct antmsg));
	if (h->lastmsg) {
		q->prev = h->lastmsg;
		q->prev->next = h->lastmsg = q;
		if (!h->nextsend) {
			h->nextsend = q;
		}
		if (antdbg & DBG_WQ) {
			fprintf(stderr, "add q %p msg %x f %p l %p d:",
				q, q->msg.msg_id, h->firstmsg, h->lastmsg);
			for (i = 0; i < 9; i++)
				fprintf(stderr, "%02x", q->msg.d[i]);
			fprintf(stderr, "\n");
		}
	} else {
		// first ever message
		q->prev = 0;
		h->nextsend = h->firstmsg = h->lastmsg = q;
		if (antdbg & DBG_WQ) fprintf(stderr, "init q %p msg %x f %p l %p\n",
			q, q->msg.msg_id, h->firstmsg, h->lastmsg);
		// start writer on first message
		pthread_mutex_lock(&h->maywriteM);
		h->maywrite++;
		pthread_mutex_unlock(&h->maywriteM);
	}
	if (antdbg & DBG_WQ) {
		fprintf(stderr, "POSTMSG: %x flags: %x len: %d time: %ld.%ld data: ",
			msg->msg_id, msg->flags, msg->len, msg->reqtime.tv_sec,
			(long)msg->reqtime.tv_usec);
		for (i = 0; i < msg->len; i++)
			fprintf(stderr, "%02x ", msg->d[i]);
		fprintf(stderr, "\n");
	}
	if (antdbg & DBG_WQ) {
		fprintf(stderr, "Queue\n");
		for (q = h->firstmsg; q; q = q->next)
			fprintf(stderr, "%p msg %x flags %x\n", q, q->msg.msg_id, q->msg.flags);
	}
	if (h->nextsend) {
		if (antdbg & DBG_WQ) fprintf(stderr, "\nnextsend %p maywrite %d\n",
			h->nextsend, h->maywrite);
		pthread_cond_signal(&h->writerC);
		if (h->maywrite) {
			if (antdbg & DBG_WQ) fprintf(stderr, "\n!waking writer nextsend %p\n",
				h->nextsend);
			//pthread_cond_signal(&h->writerC);
		}
	}
}

void
msg_send(struct anth *h, const int msg_id, ...)
{
	va_list argp;
	struct antmsg msg;
	int i;
	char *nks; // ascii network key
	u8 *msgp;
	int msglen;

	memset(&msg, 0, sizeof msg);
	S(gettimeofday(&msg.reqtime, 0));
	msg.msg_id = msg_id;
	msg.cookie = COOKIE;
	va_start(argp, msg_id);
	switch (msg_id) {
	// this is mostly auto generated from a message parameter file
	case MESG_UNASSIGN_CHANNEL_ID:
		msg.len = 1;
		msg.UNASSIGN_CHANNEL.chan = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr, "UNASSIGN_CHANNEL 	chan %x\n"
			, msg.UNASSIGN_CHANNEL.chan);
	break;
	case MESG_ASSIGN_CHANNEL_ID:
		msg.len = 3;
		msg.ASSIGN_CHANNEL.chan = va_arg(argp, int);
		msg.ASSIGN_CHANNEL.chtype = va_arg(argp, int);
		msg.ASSIGN_CHANNEL.net = va_arg(argp, int);
		msg.ASSIGN_CHANNEL.extass = 0; // TODO
		//msg.ASSIGN_CHANNEL.extass = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"ASSIGN_CHANNEL 	chan %x	chtype %x	net %x	extass %x\n",
			msg.ASSIGN_CHANNEL.chan, msg.ASSIGN_CHANNEL.chtype,
			msg.ASSIGN_CHANNEL.net, msg.ASSIGN_CHANNEL.extass);
	break;
	case MESG_CHANNEL_MESG_PERIOD_ID:
		msg.len = 3;
		msg.CHANNEL_MESG_PERIOD.chan = va_arg(argp, int);
		msg.CHANNEL_MESG_PERIOD.period = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"CHANNEL_MESG_PERIOD 	chan %x	period %x\n",
			msg.CHANNEL_MESG_PERIOD.chan, msg.CHANNEL_MESG_PERIOD.period);
	break;
	case MESG_CHANNEL_SEARCH_TIMEOUT_ID:
		msg.len = 2;
		msg.CHANNEL_SEARCH_TIMEOUT.chan = va_arg(argp, int);
		msg.CHANNEL_SEARCH_TIMEOUT.timeout = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"CHANNEL_SEARCH_TIMEOUT 	chan %x	timeout %x\n",
			msg.CHANNEL_SEARCH_TIMEOUT.chan, msg.CHANNEL_SEARCH_TIMEOUT.timeout);
	break;
	case MESG_CHANNEL_RADIO_FREQ_ID:
		msg.len = 2;
		msg.CHANNEL_RADIO_FREQ.chan = va_arg(argp, int);
		msg.CHANNEL_RADIO_FREQ.freq = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"CHANNEL_RADIO_FREQ 	chan %x	freq %x\n",
			msg.CHANNEL_RADIO_FREQ.chan, msg.CHANNEL_RADIO_FREQ.freq);
	break;
	case MESG_BURST_DATA_ID:
		msg.len = 9;
		msg.flags = AM_XMIT;
		msg.DATA.chan = va_arg(argp, int);
		memcpy(msg.DATA.data, (char *)va_arg(argp, char *), sizeof(msg.DATA.data));
		if (antdbg & DBG_MSG) fprintf(stderr, "BURST_DATA chan %x\n",	msg.DATA.chan);
	break;
	case MESG_BROADCAST_DATA_ID:
		msg.len = 9;
		msg.flags = AM_XMIT;
		msg.DATA.chan = va_arg(argp, int);
		memcpy(msg.DATA.data, (char *)va_arg(argp, char *), sizeof(msg.DATA.data));
		if (antdbg & DBG_MSG) fprintf(stderr, "BROADCAST_DATA chan %x\n",	msg.DATA.chan);
	break;
	case MESG_ACKNOWLEDGED_DATA_ID:
		msg.len = 9;
		msg.flags = AM_XMIT;
		msg.DATA.chan = va_arg(argp, int);
		memcpy(msg.DATA.data, (char *)va_arg(argp, char *), sizeof(msg.DATA.data));
		if (antdbg & DBG_MSG) fprintf(stderr, "ACKNOWLEDGED_DATA chan %x\n",	msg.DATA.chan);
	break;
	case MESG_NETWORK_KEY_ID:
		msg.len = 9;
		msg.NETWORK_KEY.net = va_arg(argp, int);
		nks = va_arg(argp, char *);
		if (16 != strlen(nks)) {
			fprintf(stderr,  "Bad key length %s\n", nks);
			exit(1);
		}
		for(i=0; i < 8; i++)
			msg.NETWORK_KEY.key[i] = hexval(nks[i*2])*16+hexval(nks[i*2+1]);
		if (antdbg & DBG_MSG) fprintf(stderr, "NETWORK_KEY 	net %x	key %s"
			, msg.NETWORK_KEY.net, nks);
	break;
	case MESG_RADIO_TX_POWER_ID:
		msg.len = 2;
		msg.RADIO_TX_POWER.zero = va_arg(argp, int);
		msg.RADIO_TX_POWER.power = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr, "RADIO_TX_POWER 	zero %x	power %x\n"
			, msg.RADIO_TX_POWER.zero, msg.RADIO_TX_POWER.power);
	break;
	case MESG_RADIO_CW_MODE_ID:
		msg.len = 3;
		msg.RADIO_CW_MODE.zero = va_arg(argp, int);
		msg.RADIO_CW_MODE.power = va_arg(argp, int);
		msg.RADIO_CW_MODE.freq = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"RADIO_CW_MODE 	zero %x	power %x	freq %x\n",
			msg.RADIO_CW_MODE.zero, msg.RADIO_CW_MODE.power, msg.RADIO_CW_MODE.freq);
	break;
	case MESG_SYSTEM_RESET_ID:
		msg.flags = AM_NOREPLY|AM_POSTDELAY;
		msg.postdelay = 500;
		msg.len = 1;
		msg.SYSTEM_RESET.zero = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr, "SYSTEM_RESET 	zero %x\n",
			msg.SYSTEM_RESET.zero);
	break;
	case MESG_OPEN_CHANNEL_ID:
		msg.len = 1;
		msg.OPEN_CHANNEL.chan = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr, "OPEN_CHANNEL 	chan %x\n"
			, msg.OPEN_CHANNEL.chan);
	break;
	case MESG_CLOSE_CHANNEL_ID:
		msg.len = 1;
		msg.CLOSE_CHANNEL.chan = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr, "CLOSE_CHANNEL 	chan %x\n"
			, msg.CLOSE_CHANNEL.chan);
	break;
	case MESG_REQUEST_ID:
		msg.len = 2;
		msg.REQUEST.chan = va_arg(argp, int);
		msg.REQUEST.message = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr, "REQUEST 	chan %x	message %x\n"
			, msg.REQUEST.chan, msg.REQUEST.message);
	break;
	case MESG_CHANNEL_ID_ID:
		msg.len = 5;
		msg.CHANNEL_ID.chan = va_arg(argp, int);
		msg.CHANNEL_ID.devno = va_arg(argp, int);
		msg.CHANNEL_ID.devtype = va_arg(argp, int);
		msg.CHANNEL_ID.trans = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"CHANNEL_ID 	chan %x	devno %x	devtype %x	trans %x\n",
			msg.CHANNEL_ID.chan, msg.CHANNEL_ID.devno, msg.CHANNEL_ID.devtype,
			msg.CHANNEL_ID.trans);
	break;
	case MESG_RADIO_CW_INIT_ID:
		msg.len = 1;
		msg.RADIO_CW_INIT.zero = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr, "RADIO_CW_INIT 	zero %x\n"
			, msg.RADIO_CW_INIT.zero);
	break;
	case MESG_ID_LIST_ADD_ID:
		msg.len = 6;
		msg.ID_LIST_ADD.chan = va_arg(argp, int);
		msg.ID_LIST_ADD.devno = va_arg(argp, int);
		msg.ID_LIST_ADD.devtype = va_arg(argp, int);
		msg.ID_LIST_ADD.trans = va_arg(argp, int);
		msg.ID_LIST_ADD.listidx = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"ID_LIST_ADD 	chan %x	devno %x	devtype %x	trans %x	listidx %x\n",
			msg.ID_LIST_ADD.chan, msg.ID_LIST_ADD.devno, msg.ID_LIST_ADD.devtype,
			msg.ID_LIST_ADD.trans, msg.ID_LIST_ADD.listidx);
	break;
	case MESG_ID_LIST_CONFIG_ID:
		msg.len = 3;
		msg.ID_LIST_CONFIG.chan = va_arg(argp, int);
		msg.ID_LIST_CONFIG.size = va_arg(argp, int);
		msg.ID_LIST_CONFIG.exclude = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"ID_LIST_CONFIG 	chan %x	size %x	exclude %x\n",
			msg.ID_LIST_CONFIG.chan, msg.ID_LIST_CONFIG.size,
			msg.ID_LIST_CONFIG.exclude);
	break;
	case MESG_OPEN_RX_SCAN_ID:
		msg.len = 1;
		msg.OPEN_RX_SCAN.zero = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr, "OPEN_RX_SCAN 	zero %x\n"
			, msg.OPEN_RX_SCAN.zero);
	break;
	case MESG_SET_LP_SEARCH_TIMEOUT_ID:
		msg.len = 2;
		msg.SET_LP_SEARCH_TIMEOUT.chan = va_arg(argp, int);
		msg.SET_LP_SEARCH_TIMEOUT.timeout = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"SET_LP_SEARCH_TIMEOUT 	chan %x	timeout %x\n",
			msg.SET_LP_SEARCH_TIMEOUT.chan, msg.SET_LP_SEARCH_TIMEOUT.timeout);
	break;
	case MESG_SERIAL_NUM_SET_CHANNEL_ID_ID:
		msg.len = 3;
		msg.SERIAL_NUM_SET_CHANNEL_ID.chan = va_arg(argp, int);
		msg.SERIAL_NUM_SET_CHANNEL_ID.devtype = va_arg(argp, int);
		msg.SERIAL_NUM_SET_CHANNEL_ID.trans = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"SERIAL_NUM_SET_CHANNEL_ID 	chan %x	devtype %x	trans %x\n",
			msg.SERIAL_NUM_SET_CHANNEL_ID.chan, msg.SERIAL_NUM_SET_CHANNEL_ID.devtype,
			msg.SERIAL_NUM_SET_CHANNEL_ID.trans);
	break;
	case MESG_RX_EXT_MESGS_ENABLE_ID:
		msg.len = 2;
		msg.RX_EXT_MESGS_ENABLE.zero = va_arg(argp, int);
		msg.RX_EXT_MESGS_ENABLE.enable = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"RX_EXT_MESGS_ENABLE 	zero %x	enable %x\n",
			msg.RX_EXT_MESGS_ENABLE.zero, msg.RX_EXT_MESGS_ENABLE.enable);
	break;
	case MESG_ENABLE_LED_FLASH_ID:
		msg.len = 2;
		msg.ENABLE_LED_FLASH.zero = va_arg(argp, int);
		msg.ENABLE_LED_FLASH.enable = va_arg(argp, int);
		if (antdbg & DBG_MSG) fprintf(stderr,
			"ENABLE_LED_FLASH 	zero %x	enable %x\n",
			msg.ENABLE_LED_FLASH.zero, msg.ENABLE_LED_FLASH.enable);
		break;
	case MESG_MSG:
		// send raw data
		msgp =  va_arg(argp, u8 *);
		msglen = va_arg(argp, int);
		if (msglen < 4 || msglen > 17 || *(msgp+1) != msglen-4) {
			fprintf(stderr, "Invalid message len %d (%d)\n", msglen, *(msgp+1));
			exit(1);
		}
		msg.len = *(msgp+1);
		msg.msg_id = *(msgp+2);
		memcpy(msg.d, msgp+3, msg.len);
		switch (msg.msg_id) {
			case MESG_BURST_DATA_ID:
			case MESG_BROADCAST_DATA_ID:
			case MESG_ACKNOWLEDGED_DATA_ID:
			case MESG_EXT_ACKNOWLEDGED_DATA_ID:
			case MESG_EXT_BROADCAST_DATA_ID:
			case MESG_EXT_BURST_DATA_ID:
				msg.flags = AM_XMIT;
				break;
		}
		fprintf(stderr, "msglen %d\n", msg.len);
		for (i = 0; i < msg.len; i++)
			fprintf(stderr, "%02x", msg.d[i]);
		fprintf(stderr, "\n");
		break;
	default:
		fprintf(stderr, "Unknown msg_id %x\n", msg_id);
		exit(1);
	}
	if (antdbg & DBG_MSG) {
		fprintf(stderr, "MSG: %x flags: %x len: %d time: %ld.%ld data: ",
			msg.msg_id, msg.flags, msg.len,
			msg.reqtime.tv_sec, (long)msg.reqtime.tv_usec);
		for (i = 0; i < msg.len; i++)
			fprintf(stderr, "%02x ", msg.d[i]);
		fprintf(stderr, "\n");
	}
	if (msg.flags & AM_XMIT)
		posttx(h, msg.DATA.chan, &msg);
	else
		postmsg(h, &msg);
}

void
ant_debug(unsigned dbg)
{
	antdbg = dbg;
}

void
ant_errors(unsigned err_level)
{
	errlevel = err_level;
}

void
ant_messages(unsigned msg_level)
{
	(void)msg_level; // TODO
}

void
ant_sync(unsigned sync)
{
	(void)sync; // TODO
}

struct anth *
ant_open(char *devfile, unsigned speed, int rtscts)
{
#ifdef WIN32
	struct anth *h = (struct anth *)malloc(sizeof(struct anth));
	memset(h, 0, sizeof (struct anth));
	DCB dcb={0};
	COMMTIMEOUTS timeouts = {0};

	if (!devfile)
		devfile = DFLTDEV;
	A(INVALID_HANDLE_VALUE != (h->fd = CreateFile(devfile,
		GENERIC_READ|GENERIC_WRITE, 0, 0,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)))
	A(GetCommState(h->fd, &dcb))
	dcb.DCBlength = sizeof(dcb);
	if (0 == speed)
		speed = DFLTSPD;
	if (115200 == speed) {
		dcb.BaudRate = CBR_115200;
	} else {
		fprintf(stderr, "unhandled speed %s:%d %d\n", __FILE__, __LINE__, speed);
	}
	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity = NOPARITY;
	A(SetCommState(h->fd, &dcb))
	timeouts.ReadIntervalTimeout = 100;
	timeouts.ReadTotalTimeoutConstant = 100;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	A(SetCommTimeouts(h->fd, &timeouts))
#else
	struct anth *h = (struct anth *)malloc(sizeof(struct anth));
	memset(h, 0, sizeof (struct anth));
	struct termios tp;
	speed_t baud;

	if (!devfile)
		devfile = DFLTDEV;
	h->fd = open(devfile, O_RDWR);
	if (h->fd < 0) {
		perror(devfile);
		exit(1);
		//return 0;
	}

	if (0 == speed)
		speed = DFLTSPD;
	switch (speed) {
		case 115200:
			baud = B115200;
			break;
		case 4800:
			baud = B4800;
			break;
		default:
			fprintf(stderr, "unhandled speed %d\n", speed);
			exit(1);
	}
	S(tcgetattr(h->fd, &tp));
#if 0
	tp.c_iflag &=
	~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF|IXANY|INPCK
#ifdef IUCLC
	|IUCLC
#endif
	);
	tp.c_oflag &= ~OPOST;
	tp.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN|ECHOE);
	tp.c_cflag &= ~(CSIZE|PARENB);
	tp.c_cflag |= CS8 | CLOCAL | CREAD;
	if (rtscts)
		tp.c_cflag |= CRTSCTS;
#endif
#if 1
	tp.c_iflag = 0;
	tp.c_oflag = 0;
	tp.c_lflag = 0;
	tp.c_cflag = CLOCAL|CS8|CREAD;
	if (rtscts)
		tp.c_cflag |= CRTSCTS;
	S(cfsetispeed(&tp, baud));
	S(cfsetospeed(&tp, baud));
	tp.c_cc[VMIN] = 1;
	tp.c_cc[VTIME] = 0;
	S(tcsetattr(h->fd, TCSANOW, &tp));
#endif

	S(cfsetispeed(&tp, baud));
	S(cfsetospeed(&tp, baud));
	tp.c_cc[VMIN] = 1;
	tp.c_cc[VTIME] = 0;
	S(tcsetattr(h->fd, TCSANOW, &tp));

#endif

	h->nextsend = h->firstmsg = h->lastmsg = 0;
	cap.valid = 0;

	A(1 == sizeof(u8));
	A(2 == sizeof(u16));
	A(4 == sizeof(u32));
	if (antdbg & DBG_FLOW) fprintf(stderr,
		"antmsgsz %zd\n", sizeof (struct antmsg));
	//memset(h->txready, 1, sizeof(h->txready));
	S(pthread_create(&(h->commthread), NULL, (void *)writer, h));
	S(pthread_create(&(h->commthread), NULL, (void *)reader, h));

	h->cookie = 0xdeadbeef;
	h->maywrite = 1;
	//h->maywriteM = PTHREAD_MUTEX_INITIALIZER;
	//h->writerM = PTHREAD_MUTEX_INITIALIZER;
	//h->writerC  = PTHREAD_COND_INITIALIZER;
	//h->dataM = PTHREAD_MUTEX_INITIALIZER;
	//h-> dataC  = PTHREAD_COND_INITIALIZER;
	//h->nextrcvM = PTHREAD_MUTEX_INITIALIZER;
	//h->nextsendM = PTHREAD_MUTEX_INITIALIZER;
	return h;
}

void
ant_reset(struct anth *h)
{
	msg_send(h, MESG_SYSTEM_RESET_ID, 0); // 0 == filler
}

void // should return capabilities list if in sync mode
ant_req_cap(struct anth *h)
{
	msg_send(h, MESG_REQUEST_ID, 0, MESG_CAPABILITIES_ID);
}

struct rcvmsg *
ant_getdata(struct anth *h, int *keep_running)
{
	struct rcvmsg *rm;

	while(*keep_running) {
		if (antdbg & DBG_DATA) fprintf(stderr,"firstrcv %p lastrcv %p nextrcv %p\n", h->firstrcv, h->lastrcv, h->nextrcv);
		pthread_mutex_lock(&h->nextrcvM);
		if (h->nextrcv) {
			rm = &h->nextrcv->msg;
			h->nextrcv = h->nextrcv->next;
			pthread_mutex_unlock(&h->nextrcvM);
			return rm;
		} else {
			pthread_mutex_unlock(&h->nextrcvM);
			pthread_mutex_lock(&h->dataM);
			if (antdbg & DBG_DATA) fprintf(stderr, "waiting for data signal\n");
				pthread_cond_wait(&h->dataC, &h->dataM);
			if (antdbg & DBG_DATA) fprintf(stderr, "\ndata signalled\n");
			pthread_mutex_unlock(&h->dataM);
		}
	}
	return rm;
}

void
ant_bcdata(struct anth *h, const u8 len, const u8 *data)
{
	(void)len; (void)data; (void)h; // TODO
}

// vim: se ts=2 sw=2:
