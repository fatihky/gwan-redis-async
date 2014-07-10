// ============================================================================
// C servlet sample for the G-WAN Web Application Server (http://trustleap.ch/)
// ----------------------------------------------------------------------------
// async-redis.c: Send commands to redis asynchronously.
//                It uses external libev event loop as asynhronous i/o backend.
//                This example also contains how to schedule functions to libev
//                   backend example. 
// ============================================================================

#include "gwan.h" // G-WAN API
#include "light-lock.h"
#include <hiredis/adapters/libev.h>
#include <hiredis/async.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#pragma link "hiredis"
#pragma link "ev"

// ----------------------------------------------------------------------------
// our (minimalist) per-request context
// ----------------------------------------------------------------------------
typedef struct
{
   light_lock_t lock;
   xbuf_t *reply;
} gw_req_async_t;

typedef struct 
{ 
   kv_t *kv;   // a Key-Value store
   char *blah; // a string
   int   val;  // a counter
   struct ev_loop *loop;
   ev_async async;
   ev_periodic periodic;
   redisAsyncContext *c;
} data_t;

void getCallback(redisAsyncContext *c, void *r, void *privdata)
{
   redisReply *reply = r;
      if (reply == NULL) return;

   gw_req_async_t *async = (gw_req_async_t *)privdata;

   xbuf_xcat(async->reply, "%x\r\n%s\r\n", reply->len, reply->str);

   // now unlock to main()
   light_unlock(&async->lock);

   // Disconnect after receiving the reply to GET
   redisAsyncDisconnect(c);
}

int main(int argc, char *argv[])
{
   // get the server 'reply' buffer where to write our answer to the client
   xbuf_t *reply = get_reply(argv);
   data_t **Data = (data_t **)get_env(argv, US_SERVER_DATA),
         *data;
   data = *Data;

   // -------------------------------------------------------------------------
   // step 1: setup a per-request context
   // -------------------------------------------------------------------------
   gw_req_async_t **R_D = (gw_req_async_t **)get_env(argv, US_REQUEST_DATA)
                  , *r_d;

   if(!*R_D) // we did not setup our per-request structure yet
   {
      // create a per-request memory pool
      if(!gc_init(argv, 4070)) // can now use gc_alloc() for up to 4070 bytes
         return 503; // could not allocate memory!

      *R_D = gc_malloc(argv, sizeof(gw_req_async_t)); // allocate a request context
      if(!*R_D) return 500; // not possible here, but a good habit anyway

      r_d = *R_D;

      r_d->is_ok = false;
      r_d->reply = reply;
      r_d->lock.val = 1;

      // ----------------------------------------------------------------------
      // step 2: schedule asynchronous jobs
      // ----------------------------------------------------------------------

      // ev_async_send is not required, only example. 
      ev_async_send(data->loop, &data->async);

      redisAsyncCommand(data->c, NULL, NULL, "SET key %b", argv[argc-1], strlen(argv[argc-1]));
      redisAsyncCommand(data->c, getCallback, r_d, "GET key");
      
      // ----------------------------------------------------------------------
      // tell G-WAN to run the script when reply received from redis
      // ----------------------------------------------------------------------
      // WK_MS:milliseconds, WK_FD:file_descriptor
      wake_up(argv, data->c->c.fd, WK_FD, NULL);
      
      // ----------------------------------------------------------------------
      // send chunked encoding HTTP header and HTTP status code
      // ----------------------------------------------------------------------
      static const char head[] = 
         "HTTP/1.1 200 OK\r\n"
         "Connection: close\r\n" // this will limit the connection rate
         "Content-type: text/html; charset=utf-8\r\n"
         "Transfer-Encoding: chunked\r\n\r\n"
         "5\r\n<pre>\r\n"; // format is: "[length]\r\n[data]\n\n"
      xbuf_ncat(reply, head, sizeof(head) - 1);

      light_lock(&r_d->lock);

      // -------------------------------------------------------------------------
      // return code
      // -------------------------------------------------------------------------
      // RC_NOHEADERS: do not generate HTTP headers
      // RC_STREAMING: call me again after send() is done
      return RC_NOHEADERS + RC_STREAMING; 

   } else {
      r_d = *R_D;

      // We already initialized request wide data pointer
      //  now wait for redis response
      light_lock(&r_d->lock);

      // end chunked encoding; format is: "[length]\r\n[data]\n\n"
      static const char end[] = "6\r\n</pre>\r\n0\r\n\r\n"; 
      xbuf_ncat(reply, end, sizeof(end) - 1);
      
      wake_up(argv, 0, WK_FD, NULL); // 0:disable any further wake-up

      return RC_NOHEADERS; // RC_NOHEADERS: do not generate HTTP headers
   }

   return 200; // never get here
}

// ============================================================================
// End of Source Code
// ============================================================================
