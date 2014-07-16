// ============================================================================
// Handler C script for the G-WAN Web Application Server (http://trustleap.ch/)
// ----------------------------------------------------------------------------
// main.c: start worker thread, initialize server wide data pointer.
// ----------------------------------------------------------------------------
// ============================================================================
#pragma debug     // uncomment to get symbols/line numbers in crash reports

#include "gwan.h"   // G-WAN exported functions
#include <hiredis/adapters/libev.h>
#include <hiredis/async.h>
#include <stdio.h>  // printf()
#include <string.h> // strcmp()
#include <malloc.h> // calloc()
#include <pthread.h>
#include <ev.h>

#pragma link "ev"
#pragma link "pthread"
#pragma link "hiredis"

void *worker(void *arg);
void async_cb(EV_P_ struct ev_async *w, int revents);
void periodic_cb(EV_P_ struct ev_periodic *w, int revents);
ev_tstamp reschedule_cb(struct ev_periodic *w, ev_tstamp now);
void connectCallback(const redisAsyncContext *c, int status);
void disconnectCallback(const redisAsyncContext *c, int status);

// ----------------------------------------------------------------------------
// structure holding pointers for persistence
// ----------------------------------------------------------------------------
typedef struct 
{ 
   /*  Custom fields
   kv_t *kv;   // a Key-Value store
   char *blah; // a string
   int   val;  // a counter */
   struct ev_loop *loop;
   ev_async async;
   ev_periodic periodic;
   redisAsyncContext *c;
} data_t;


// ----------------------------------------------------------------------------
// init() will initialize your data structures, load your files, etc.
// ----------------------------------------------------------------------------
// init() should return -1 if failure (to allocate memory for example)
int init(int argc, char *argv[])
{
	// get the Handler persistent pointer to attach anything you need
	//data_t **data = (data_t**)get_env(argv, US_HANDLER_DATA);
	//data_t **data = (data_t**)get_env(argv, US_VHOST_DATA);
	data_t **Data = (data_t**)get_env(argv, US_SERVER_DATA),
			*data;

	// initialize the persistent pointer by allocating our structure
	// attach structures, lists, sockets with a back-end/database server, 
	// file descriptiors for custom log files, etc.
	*Data = (data_t*)calloc(1, sizeof(data_t));
	if(!*Data)
		return -1; // out of memory
	data = *Data;

	// initialize libev loop
	data->loop = ev_loop_new(0);
	ev_async_init(&data->async, async_cb);
	ev_periodic_init(&data->periodic, periodic_cb, 0, 1000, reschedule_cb);

	data->c = redisAsyncConnect("127.0.0.1", 6379);
	if (data->c->err)
	{
		/* Let *c leak for now... */
		printf("Error: %s\n", data->c->errstr);
		abort();
	}

	// connect to redis and set up redisAsyncContext
	redisLibevAttach(data->loop, data->c);
	redisAsyncSetConnectCallback(data->c, connectCallback);
	redisAsyncSetDisconnectCallback(data->c, disconnectCallback);

	// start worker thread
	pthread_t thread;
	pthread_create (&thread, NULL, worker, data);
	return 0;
}

// ----------------------------------------------------------------------------
// clean() will free any allocated memory and possibly log summarized stats
// ----------------------------------------------------------------------------
void clean(int argc, char *argv[])
{
	// free any data attached to your persistence pointer
	data_t **data = (data_t**)get_env(argv, US_SERVER_DATA);

	// we could close a data->log custom file 
	// if the structure had a FILE *log; field
	// fclose(data->log);

	if(data)
		free(*data);
}

void connectCallback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK)
	{
		printf("Error: %s\n", c->errstr);
		return;
	}
	printf("Connected...\n");
}
 
void disconnectCallback(const redisAsyncContext *c, int status)
{
	if (status != REDIS_OK)
	{
		printf("Error: %s\n", c->errstr);
		return;
	}
	printf("Disconnected...\n");
}

void async_cb(EV_P_ struct ev_async *w, int revents)
{
	printf("fn: %s\n", __func__);
}

void periodic_cb(EV_P_ struct ev_periodic *w, int revents)
{
	printf("fn: %s\n", __func__);
}

ev_tstamp reschedule_cb(struct ev_periodic *w, ev_tstamp now)
{
	printf("fn: %s\n", __func__);
	return ev_time() + 1000;
}

void *worker(void *arg)
{
	printf("yan işlem çalışıyor...\n");
	data_t *data = (data_t *)arg;

	ev_async_start(data->loop, &data->async);
	ev_periodic_start(data->loop, &data->periodic);

	ev_run(data->loop, 0);
	return NULL;
}

// ----------------------------------------------------------------------------
// main() does the job for all the connection states below:
// (see 'HTTP_Env' in gwan.h for all the values you can fetch with get_env())
// ----------------------------------------------------------------------------

// those defines summarise the return codes for each state
#define RET_CLOSE_CONN   0 // all states
#define RET_BUILD_REPLY  1 // HDL_AFTER_ACCEPT only
#define RET_READ_MORE    1 // HDL_AFTER_READ only
#define RET_SEND_REPLY   2 // all states
#define RET_CONTINUE   255 // all states

int main(int argc, char *argv[])
{
	return(255); // continue G-WAN's default execution path
}

// ============================================================================
// End of Source Code
// ============================================================================
