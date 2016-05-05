// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  mutex = PTHREAD_MUTEX_INITIALIZER;
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  int r;
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, cached_lock>::iterator it = lock_pool.find(lid);
  if (it == lock_pool.end())
      it = (lock_pool.insert(std::make_pair(lid, cached_lock()))).first;
  while (1)
  {
      //printf("%d %d\n", lid, it -> second.stat);
      if (it -> second.stat == NONE)
      {
          it -> second.stat = ACQUIRING;
          it -> second.retry = false;
          pthread_mutex_unlock(&mutex);
          ret = cl->call(lock_protocol::acquire, lid, id, r);
          pthread_mutex_lock(&mutex);
          //printf("protocol %d %d\n", lid, ret);
          if (ret == lock_protocol::OK)
          {
              it -> second.stat = LOCKED;
              pthread_mutex_unlock(&mutex);
              return ret;
          }
          else if (ret == lock_protocol::RETRY)
          {
              while (!(it -> second).retry)
                  pthread_cond_wait(&(it -> second).retry_con, &mutex);
              //printf("111111111111111\n");
              it -> second.stat = NONE;
          }
      }
      else if (it -> second.stat == FREE)
      {
          it -> second.stat = LOCKED;
          pthread_mutex_unlock(&mutex);
          return ret;
      }
      else if (it -> second.stat == LOCKED)
      {
          pthread_cond_wait(&(it -> second).wait_con, &mutex);
      }
      else if (it -> second.stat == ACQUIRING)
      {
          pthread_cond_wait(&(it -> second).wait_con, &mutex);
      }
      else
      {
          pthread_cond_wait(&(it -> second).release_con, &mutex);
      }
  }
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  int r;
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, cached_lock>::iterator
      it = lock_pool.find(lid);
  //printf("release called %s %d %d\n", id.c_str(), lid, it -> second.revoke);
  if (!(it -> second.revoke))
  {
      it -> second.stat = FREE;
      pthread_cond_signal(&(it -> second).wait_con);
      pthread_mutex_unlock(&mutex);
      return lock_protocol::OK;
  }
  else
  {
      it -> second.stat = RELEASING;
      it -> second.revoke = false;
      pthread_mutex_unlock(&mutex);
      ret = cl->call(lock_protocol::release, lid, id, r);
      pthread_mutex_lock(&mutex);
      it -> second.stat = NONE;
      pthread_cond_broadcast(&it->second.release_con);
      pthread_cond_signal(&(it -> second).wait_con);
      pthread_mutex_unlock(&mutex);
      return lock_protocol::OK;
  }
  return lock_protocol::OK;

}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int ret = rlock_protocol::OK;
  int r;
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, cached_lock>::iterator
      it = lock_pool.find(lid);
  //printf("revoke handler %s %d\n", id.c_str(), it -> second.stat);
  if (it -> second.stat == FREE)
  {
      it -> second.stat = RELEASING;
      it -> second.revoke = false;
      pthread_mutex_unlock(&mutex);
      ret = cl->call(lock_protocol::release, lid, id, r);
      pthread_mutex_lock(&mutex);
      it -> second.stat = NONE;
      pthread_cond_broadcast(&it->second.release_con);
      pthread_mutex_unlock(&mutex);
  }
  else
  {
      it -> second.revoke = true;
      pthread_mutex_unlock(&mutex);
  }
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, cached_lock>::iterator
      it = lock_pool.find(lid);
  it -> second.retry = true;
  //printf("retry handler %s %d\n", id.c_str(), it -> second.stat);
  pthread_cond_signal(&it->second.retry_con);
  pthread_mutex_unlock(&mutex);
  return ret;
}



