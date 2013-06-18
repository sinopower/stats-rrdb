/*
 * server.cpp
 *
 *  Created on: Jun 2, 2013
 *      Author: aleksey
 */
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include <fstream>

#include "server/server.h"

#include "log.h"
#include "config.h"
#include "server/thread_pool.h"

#include "rrdb/rrdb.h"
#include "rrdb/rrdb_test.h"

#include "server/server_status.h"
#include "server/server_udp.h"
#include "server/server_tcp.h"

server::server() :
  _server_status(new server_status()),
  _exit_signals(_io_service)
{
}

server::~server()
{
}

void server::initialize(boost::shared_ptr<config> config)
{
  // create
    _rrdb.reset(new rrdb(shared_from_this()));
  _server_udp.reset(new server_udp(_rrdb));
  _server_tcp.reset(new server_tcp(_rrdb));

  // init
  _rrdb->initialize(config);
  _server_udp->initialize(_io_service, config);
  _server_tcp->initialize(_io_service, config);

  // Register signal handlers so that the server may be shut down.
  _exit_signals.add(SIGHUP);
  _exit_signals.add(SIGINT);
  _exit_signals.add(SIGQUIT);
  _exit_signals.add(SIGABRT);
  _exit_signals.add(SIGTERM);
  _exit_signals.async_wait(boost::bind(
      &server::signal_handler,
      this,
      boost::asio::placeholders::error,
      boost::asio::placeholders::signal_number
  ));
}

void server::signal_handler(const boost::system::error_code& error, int signal_number)
{
  if (!error) {
      LOG(log::LEVEL_INFO, "Received signal %d, exiting", signal_number);
      this->stop();
      LOG(log::LEVEL_INFO, "Received signal %d, done", signal_number);
  }
}

void server::daemonize(const std::string & pid_file) 
{
  // Fork the process and have the parent exit. If the process was started
  // from a shell, this returns control to the user. Forking a new process is
  // also a prerequisite for the subsequent call to setsid().
  this->notify_before_fork();
  pid_t pid = fork();
  if(pid < 0) {
      LOG(log::LEVEL_CRITICAL, "Failed to fork the process: %d", errno);
      std::terminate();
  } else if(pid > 0) {
      LOG(log::LEVEL_DEBUG, "PID after the first fork is %d", pid);

      // We're in the parent process and need to exit but first let's notify
      // everyone
      this->notify_after_fork(true);
      exit(0);
  }
  this->notify_after_fork(false);

  // Make the process a new session leader. This detaches it from the
  // terminal.
  setsid();

  // A process inherits its working directory from its parent. This could be
  // on a mounted filesystem, which means that the running daemon would
  // prevent this filesystem from being unmounted. Changing to the root
  // directory avoids this problem.
  if(chdir("/")) {
      throw exception("Unable to switch to root folder");
  }

  // The file mode creation mask is also inherited from the parent process.
  // We don't want to restrict the permissions on files created by the
  // daemon, so the mask is cleared.
  umask(0);

  // A second fork ensures the process cannot acquire a controlling terminal.
  this->notify_before_fork();
  pid = fork();
  if (pid < 0) {
      LOG(log::LEVEL_CRITICAL, "Failed to fork the process second time: %d", errno);
      std::terminate();
  } else if(pid > 0) {
      LOG(log::LEVEL_DEBUG, "PID after the second fork is %d", pid);

      // write out pid
      if(!pid_file.empty()) {
          std::fstream pidfs(pid_file.c_str(), std::ios_base::out);
          pidfs << pid;
      }

      // just exit, do nothing else but notify about exit
      this->notify_after_fork(true);
      exit(0);
  }
  this->notify_after_fork(false);

  // Close the standard streams. This decouples the daemon from the terminal
  // that started it.
  close(0); // STDIN
  close(1); // STDOUT
  close(2); // STDERR
}

void server::notify_before_fork()
{
  // Inform the io_service that we are about to become a daemon. The
  // io_service cleans up any internal resources, such as threads, that may
  // interfere with forking.
  _io_service.notify_fork(boost::asio::io_service::fork_prepare);
}

void server::notify_after_fork(bool is_parent)
{
  // Inform the io_service that we have finished becoming a daemon. The
  // io_service uses this opportunity to create any internal file descriptors
  // that need to be private to the new process
  if(is_parent) {
      _io_service.notify_fork(boost::asio::io_service::fork_parent);
  } else {
      _io_service.notify_fork(boost::asio::io_service::fork_child);
  }
}

void server::setuid_user(const std::string & user)
{
  // do we know this username?
  struct passwd *pwd = getpwnam(user.c_str());
  if(pwd == NULL) {
      throw exception("Can not find user '%s' to setuid() to it", user.c_str());
  }

  // switch the log file - the only resources we've touched
  std::string log_file = log::get_log_file();
  if(!log_file.empty() && chown(log_file.c_str(), pwd->pw_uid, pwd->pw_gid )) {
      throw exception("Can not chown() log file '%s' to user '%s' with id %u and primary group id %u: errno %d", log_file.c_str(), user.c_str(), pwd->pw_uid, pwd->pw_gid, errno);
  }

  // use user's supplemental group list
  if(initgroups(user.c_str(), pwd->pw_gid)) {
      throw exception("Can not initgroups() for user '%s' with id %u and primary group id %u: errno %d", user.c_str(), pwd->pw_uid, pwd->pw_gid, errno);
  }

  // use primary group ID
  if(setgid(pwd->pw_gid)) {
      throw exception("Can not setgid() for user '%s' with id %u and primary group id %u: errno %d", user.c_str(), pwd->pw_uid, pwd->pw_gid, errno);
  }

  // use the specified user ID
  if (setuid(pwd->pw_uid)) {
      throw exception("Can not setuid() for user '%s' with id %u and primary group id %u: errno %d", user.c_str(), pwd->pw_uid, pwd->pw_gid, errno);
  }

  // done
  LOG(log::LEVEL_INFO, "Switched to user '%s' with id %u and primary group id %u", user.c_str(), pwd->pw_uid, pwd->pw_gid);
}

void server::run()
{
  LOG(log::LEVEL_DEBUG, "Starting the server");

  _rrdb->start();
  _server_udp->start();
  _server_tcp->start();

  _io_service.run();

  // done
  LOG(log::LEVEL_INFO, "Started the server");
}

void server::stop()
{
  _io_service.stop();
  _server_udp->stop();
  _server_tcp->stop();

  _rrdb->stop();
}

boost::shared_ptr<const server_status> server::get_status()
{
  boost::lock_guard<spinlock> guard(_server_status_lock);
  if(!_server_status->is_valid()) {
      boost::shared_ptr<server_status> new_server_status(new server_status());
      _server_udp->update_status(new_server_status);
      _server_tcp->update_status(new_server_status);
      _rrdb->update_status(new_server_status);

      _server_status.swap(new_server_status);
  }
  return _server_status;
}

void server::test(const std::string & params)
{
  rrdb_test::run_test(_rrdb, params);
}