// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache_rsm.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <sys/time.h>

#include "rsm_client.h"

static void *
releasethread(void *x)
{
  lock_client_cache_rsm *cc = (lock_client_cache_rsm *) x;
  cc->releaser();
  return 0;
}
int lock_client_cache_rsm::last_port = 0;

lock_client_cache_rsm::lock_client_cache_rsm(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  printf("%s\n", id.c_str());
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache_rsm::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache_rsm::retry_handler);
  xid = 0;
  // You fill this in Step Two, Lab 7
  // - Create rsmc, and use the object to do RPC 
  //   calls instead of the rpcc object of lock_client
  VERIFY(pthread_mutex_init(&client_mutex, NULL) == 0);
  rsmc = new rsm_client(xdst);

  pthread_t th;
  int r = pthread_create(&th, NULL, &releasethread, (void *) this);
  VERIFY (r == 0);
}


void
lock_client_cache_rsm::releaser()
{

  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.
	while(true) {
		release_entry e;
		release_queue.deq(&e);

		if (lu) {
			lu->dorelease(e.lid);
		}
		int r;
		rsmc->call(lock_protocol::release, e.lid, id, e.xid, r);
        printf("release thread front %s, \n", id.c_str());
       	VERIFY(pthread_mutex_lock(&client_mutex) == 0);
		std::map<lock_protocol::lockid_t, lock_entry>::iterator iter;
		iter = lockmap.find(e.lid);
		VERIFY(iter != lockmap.end());
		iter->second.state = NONE;
		pthread_cond_broadcast(&iter->second.releasequeue);
		pthread_cond_broadcast(&iter->second.waitqueue);
		VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
	}
}


lock_protocol::status
lock_client_cache_rsm::acquire(lock_protocol::lockid_t lid)
{
  	int ret = lock_protocol::OK;
	int r; 
	std::map<lock_protocol::lockid_t, lock_entry>::iterator iter;
	VERIFY(pthread_mutex_lock(&client_mutex) == 0);
	iter = lockmap.find(lid);
	if(iter == lockmap.end()) {
		iter = lockmap.insert(std::make_pair(lid, lock_entry())).first;
	}

	while (true) { 
        printf("acquire state %d, %d, %s\n", iter->second.state, lid, id.c_str());
		switch(iter->second.state) {
			case NONE:
				iter->second.xid = xid;
				xid++;
				iter->second.state = ACQUIRING;
				iter->second.retry = false;
	
				VERIFY(pthread_mutex_unlock(&client_mutex) ==0);
				ret = rsmc->call(lock_protocol::acquire, lid, id, iter->second.xid, r);
				VERIFY(pthread_mutex_lock(&client_mutex) == 0);

				if (ret == lock_protocol::OK) {
					iter->second.state = LOCKED;
					VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
                    printf("in the end of requeire, xid = %d\n", xid);
					return ret;
				} else if (ret == lock_protocol::RETRY) {
					if(!iter->second.retry) {
						struct timeval now;
						struct timespec next_timeout;
						gettimeofday(&now, NULL);
						next_timeout.tv_sec = now.tv_sec + 3;
						next_timeout.tv_nsec = 0;
						int r = pthread_cond_timedwait(&iter->second.retryqueue, 
								&client_mutex,&next_timeout);				
				   		if (r == ETIMEDOUT)
							iter->second.retry = true;
					//	pthread_cond_wait(&iter->second.retryqueue, &client_mutex);
					}
				}
				break;
			case FREE:
				iter->second.state = LOCKED;
				pthread_mutex_unlock(&client_mutex);
                printf("in the end of requeire, xid = %d\n", xid);
				return lock_protocol::OK;
				break;
			case LOCKED:
				pthread_cond_wait(&iter->second.waitqueue, &client_mutex);
				break;
			case ACQUIRING:
				if(!iter->second.retry) { 
					pthread_cond_wait(&iter->second.waitqueue, &client_mutex);
				} else {
					iter->second.retry = false;
					iter->second.xid = xid;
					xid++;
					VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
					ret = rsmc->call(lock_protocol::acquire, lid, id, iter->second.xid, r);
                    printf("rpc call in acquiring %d, %s \n", lid, id.c_str());
					VERIFY(pthread_mutex_lock(&client_mutex) == 0);
                    printf("lock successfully %d, %s, %d %d \n", lid, id.c_str(), ret, iter->second.retry);
					if (ret == lock_protocol::OK) {
						iter->second.state = LOCKED;
						VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
                        printf("in the end of requeire, xid = %d\n", xid);
						return ret;
					} else if (ret == lock_protocol::RETRY) {
						if(!iter->second.retry) {
							struct timeval now;
							struct timespec next_timeout;
							gettimeofday(&now, NULL);
							next_timeout.tv_sec = now.tv_sec + 3;
							next_timeout.tv_nsec = 0;
							int r = pthread_cond_timedwait(&iter->second.retryqueue, 
									&client_mutex,&next_timeout);
							if (r == ETIMEDOUT)
									iter->second.retry = true;
					//		pthread_cond_wait(&iter->second.retryqueue, &client_mutex);
						}
					}
				}
				break;	
			case RELEASING:
				pthread_cond_wait(&iter->second.releasequeue, &client_mutex);
				break;
		}
	}
  	return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache_rsm::release(lock_protocol::lockid_t lid)
{
 	int r;
	lock_protocol::status ret = lock_protocol::OK;
	std::map<lock_protocol::lockid_t, lock_entry>::iterator iter;
	VERIFY(pthread_mutex_lock(&client_mutex) == 0);
	iter = lockmap.find(lid);
	if (iter == lockmap.end()) {
		printf("ERROR: can't find lock with lockid = %d\n", lid);
		VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
		return lock_protocol::NOENT;
	}
	if (iter->second.revoked) {
		iter->second.state = RELEASING;
		iter->second.revoked = false;
		lock_protocol::xid_t cur_xid = iter->second.xid;
		VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
		//for lab5, flush file extent from extent_client to extent_server 
		if (lu)
			lu->dorelease(lid);
		
		ret = rsmc->call(lock_protocol::release, lid, id, cur_xid, r);
        printf("release function front %d, %s, \n", lid, id.c_str());
		
		VERIFY(pthread_mutex_lock(&client_mutex) == 0);
		iter->second.state = NONE;
		pthread_cond_broadcast(&iter->second.releasequeue);
		pthread_cond_broadcast(&iter->second.waitqueue);
		VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
		return ret;
	} else {
		iter->second.state = FREE;
		pthread_cond_signal(&iter->second.waitqueue);
		VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
		return lock_protocol::OK;
	}	
}


rlock_protocol::status
lock_client_cache_rsm::revoke_handler(lock_protocol::lockid_t lid, 
			          lock_protocol::xid_t xid, int &)
{
    printf("revoke handler front %d, %s, \n", lid, id.c_str());
	int r;
	int ret = rlock_protocol::OK;
	std::map<lock_protocol::lockid_t, lock_entry>::iterator iter;
	VERIFY(pthread_mutex_lock(&client_mutex) == 0);
	iter = lockmap.find(lid);
	if (iter == lockmap.end()) {
		printf("ERROR: can't find lock with lockid = %d\n", lid);
		VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
		return lock_protocol::NOENT;
	}
    printf("revoke handler second %d, %s, %d, %d, %d\n", lid, id.c_str(), iter->second.xid, xid, iter->second.state);
	if (iter->second.xid == xid) {
		if (iter->second.state == FREE) {
			iter->second.state = RELEASING;
			iter->second.revoked = false;
			release_queue.enq(release_entry(lid, xid));
			VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
		} else {
			//handle duplicated revoke for lab7 step four 
			// when lock state is NONE or RELEASING, the lock is released or in releasing.
			// eg. the lock has been revoked;
			if (iter->second.state != NONE || iter->second.state != RELEASING)
			iter->second.revoked = true;
			VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
		}
	}
    else{
        VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
    }
  	return ret;
}

rlock_protocol::status
lock_client_cache_rsm::retry_handler(lock_protocol::lockid_t lid, 
			         lock_protocol::xid_t xid, int &)
{
    printf("retry handler front %d, %s, \n", lid, id.c_str());
 	int ret = rlock_protocol::OK;
	std::map<lock_protocol::lockid_t, lock_entry>::iterator iter;
	pthread_mutex_lock(&client_mutex);
    printf("retry handler second %d, %s, \n", lid, id.c_str());
	iter = lockmap.find(lid);
	if (iter == lockmap.end()) {
		printf("ERROR: can't find lock with lockid = %d\n", lid);
		VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
		return lock_protocol::NOENT;
	}
	if (iter->second.xid == xid) { 
		iter->second.retry = true;
        printf("retry handler before handler %d, %s, %d \n", lid, id.c_str(), iter->second.retry);
		pthread_cond_signal(&iter->second.retryqueue);
	//	pthread_cond_signal(&iter->second.waitqueue);
        printf("retry handler after handler %d, %s, %d \n", lid, id.c_str(), iter->second.retry);
		VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
	}
    else{
        VERIFY(pthread_mutex_unlock(&client_mutex) == 0);
        return lock_protocol::NOENT;
    }
    printf("retry end %d, %s, %d, %d\n", lid, id.c_str(), iter->second.xid, xid);
	return ret;
}


