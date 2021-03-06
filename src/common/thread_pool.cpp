/*
 * thread_pool.cpp
 *
 *  Created on: Jun 4, 2013
 *      Author: aleksey
 */

#include "common/exception.h"
#include "common/log.h"
#include "common/thread_pool.h"

thread_pool::thread_pool(my::size_t pool_size) :
  _pool_size(pool_size),
  _work(_io_service),
  _used_threads(0),
  _started_jobs(0),
  _finished_jobs(0)
{
  LOG(log::LEVEL_DEBUG, "Creating thread pool");

  // init
  for(my::size_t ii = 0; ii < _pool_size; ++ii) {
      _threads.create_thread(boost::bind(
          static_cast<size_t (boost::asio::io_service::*)()>(&boost::asio::io_service::run),
          &_io_service
      ));
  }

  // done
  LOG(log::LEVEL_INFO, "Created thread pool with %lu threads", SIZE_T_CAST _pool_size);
}

thread_pool::~thread_pool()
{
  LOG(log::LEVEL_DEBUG, "Stopping thread pool");

  // Force all threads to return from io_service::run().
  _io_service.stop();

  // Suppress all exceptions.
  try {
      _threads.join_all();
  } catch (std::exception& e) {
    LOG(log::LEVEL_ERROR, "%s", e.what());
  }

  LOG(log::LEVEL_INFO, "Stopped thread pool");
}

my::size_t thread_pool::run(const boost::intrusive_ptr<thread_pool_task> & task)
{
  // ready to execute?
  my::size_t used_threads = _used_threads.fetch_add(1, boost::memory_order_acquire);
  if(_used_threads >= _pool_size) {
      LOG(log::LEVEL_DEBUG, "No available threads to execute the task right away: %ld out of %ld used", SIZE_T_CAST used_threads, SIZE_T_CAST _pool_size);
  }

  // go!
  _started_jobs.fetch_add(1, boost::memory_order_acquire);
  _io_service.post(boost::bind( &thread_pool::wrap_task_run, this, task)) ;

  // done
  return used_threads;
}

void thread_pool::wrap_task_run(const boost::intrusive_ptr<thread_pool_task> & task)
{
  // run the task, ignore all exception
  try {
      task->run();
  } catch(std::exception & e) {
      LOG(log::LEVEL_ERROR, "%s", e.what());
  } catch(...) {
      LOG(log::LEVEL_ERROR, "unknown exception");
  }

  // done!
  _used_threads.fetch_sub(1, boost::memory_order_relaxed);
  _finished_jobs.fetch_add(1, boost::memory_order_release);
}
