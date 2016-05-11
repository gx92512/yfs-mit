// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  mutex = PTHREAD_MUTEX_INITIALIZER;
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  pthread_mutex_lock(&mutex);
  //printf("extent_client get %d\n", eid);
  if (cache_pool.find(eid) != cache_pool.end())
  {
       //printf("extent_client get %d %d\n", eid, 1);
      if (cache_pool[eid].stat == CACHED || cache_pool[eid].stat == CHANGED)
      {
          buf = cache_pool[eid].content;
          cache_pool[eid].attr.atime = time(NULL);
      }
      else if (cache_pool[eid].stat == NONE)
      {
          ret = cl->call(extent_protocol::get, eid, buf);
          if (ret == extent_protocol::OK)
          {
              cache_pool[eid].stat = CACHED;
              cache_pool[eid].content = buf;
              cache_pool[eid].attr.atime = time(NULL);
              cache_pool[eid].attr.size = buf.size();
          }
      }
      else
          ret = extent_protocol::NOENT;
  }
  else
  {
      cached_file nfile;
      ret = cl->call(extent_protocol::get, eid, buf);
      //printf("extent_client get %d %d %d\n", eid, 0, ret);
      if (ret == extent_protocol::OK)
      {
          nfile.stat = CACHED;
          nfile.content = buf;
          nfile.attr.atime = time(NULL);
          nfile.attr.size = buf.size();
          nfile.attr.ctime = 0;
          nfile.attr.mtime = 0;
          cache_pool[eid] = nfile;
      }
  }
  //ret = cl->call(extent_protocol::get, eid, buf);
  pthread_mutex_unlock(&mutex);
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  pthread_mutex_lock(&mutex);
  if (cache_pool.find(eid) == cache_pool.end())
  {
      //printf("extent_client attr %d %d\n", eid, 0);
      ret = cl->call(extent_protocol::getattr, eid, attr);
      if (ret == extent_protocol::OK)
      {
          cached_file nfile;
          nfile.stat = NONE;
          nfile.attr.atime = attr.atime;
          nfile.attr.ctime = attr.ctime;
          nfile.attr.mtime = attr.mtime;
          nfile.attr.size = attr.size;
          cache_pool[eid] = nfile;
      }
  }
  else
  {
      //printf("extent_client attr %d %d %d\n", eid, 1, cache_pool[eid].stat);
      //printf("extent_client attr %d %d %d\n", cache_pool[eid].attr.ctime, cache_pool[eid].attr.mtime, cache_pool[eid].attr.atime);
      if (cache_pool[eid].stat == NONE)
          attr = cache_pool[eid].attr;
      else if (cache_pool[eid].stat == REMOVED)
          ret = extent_protocol::NOENT;
      else
      {
          attr = cache_pool[eid].attr;
          if (cache_pool[eid].attr.ctime == 0 || cache_pool[eid].attr.atime == 0 || cache_pool[eid].attr.mtime == 0)
          {
              ret = cl->call(extent_protocol::getattr, eid, attr);
              if (ret == extent_protocol::OK)
              {
                  if (cache_pool[eid].attr.atime == 0)
                  {
                      cache_pool[eid].attr.atime = attr.atime;
                  }
                  if (cache_pool[eid].attr.ctime == 0 || cache_pool[eid].attr.mtime == 0)
                  {
                      cache_pool[eid].attr.ctime = attr.ctime;
                      cache_pool[eid].attr.mtime = attr.mtime;
                      attr.atime = cache_pool[eid].attr.atime;
                  }
              }
          }
      }
  }
  //ret = cl->call(extent_protocol::getattr, eid, attr);
  pthread_mutex_unlock(&mutex);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  pthread_mutex_lock(&mutex);
  //printf("extent_client put %d %s", eid, buf.c_str());
  if (cache_pool.find(eid) != cache_pool.end())
  {
      if (cache_pool[eid].stat == REMOVED)
          ret = extent_protocol::NOENT;
      else
      {
          cache_pool[eid].content = buf;
          cache_pool[eid].attr.ctime = time(NULL);
          cache_pool[eid].attr.mtime = time(NULL);
          cache_pool[eid].attr.size = buf.size();
          cache_pool[eid].stat = CHANGED;
      }
  }
  else
  {
      cached_file nfile;
      nfile.content = buf;
      nfile.attr.ctime = time(NULL);
      nfile.attr.mtime = time(NULL);
      nfile.attr.atime = time(NULL);
      nfile.attr.size = buf.size();
      nfile.stat = CHANGED;
      cache_pool[eid] = nfile;
  }
  //ret = cl->call(extent_protocol::put, eid, buf, r);
  pthread_mutex_unlock(&mutex);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  pthread_mutex_lock(&mutex);
  if (cache_pool.find(eid) != cache_pool.end())
  {
      if (cache_pool[eid].stat == REMOVED)
          ret = extent_protocol::NOENT;
      else
          cache_pool[eid].stat = REMOVED;
  }
  else
  {
      cached_file nfile;
      nfile.stat = REMOVED;
      cache_pool[eid] = nfile;
  }
  //ret = cl->call(extent_protocol::remove, eid, r);
  pthread_mutex_unlock(&mutex);
  return ret;
}


