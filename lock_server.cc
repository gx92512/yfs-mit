// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
	mutex = PTHREAD_MUTEX_INITIALIZER;
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status 
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	printf("acquire request from clt %d\n", clt);
	pthread_mutex_lock(&mutex);
	std::map<lock_protocol::lockid_t, lock*>::iterator 
		it = lock_pool.find(lid);
	if (it != lock_pool.end())
	{
		while(it -> second -> status != lock::FREE)
			pthread_cond_wait(&(it -> second -> cond), &mutex);
		it -> second -> status = lock::LOCKED;
		pthread_mutex_unlock(&mutex);
		return ret;
	}
	else
	{
		lock* nlock = new lock(lid);
		nlock -> status = lock::LOCKED;
		lock_pool.insert(std::make_pair(lid, nlock));
		pthread_mutex_unlock(&mutex);
		return ret;
	}
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	printf("release request from clt %d\n", clt);
	pthread_mutex_lock(&mutex);
	std::map<lock_protocol::lockid_t, lock*>::iterator
		it = lock_pool.find(lid);
	if (it != lock_pool.end())
	{
		it -> second -> status = lock::FREE;
		pthread_mutex_unlock(&mutex);
		pthread_cond_signal(&(it -> second -> cond));
		return ret;
	}
	else
	{
		ret = lock_protocol::IOERR;
		pthread_mutex_unlock(&mutex);
		return ret;
	}
}
