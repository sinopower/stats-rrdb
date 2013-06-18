/*
 * server_udp.h
 *
 *  Created on: Jun 4, 2013
 *      Author: aleksey
 */

#ifndef SERVER_UDP_H_
#define SERVER_UDP_H_

#include <cstddef>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

class config;
class thread_pool;
class rrdb;
class connection_udp;
class server_status;

class server_udp :
    public boost::enable_shared_from_this<server_udp>
{
  friend class connection_udp;

public:
  server_udp(boost::shared_ptr<rrdb> rrdb);
  virtual ~server_udp();

  void initialize(boost::asio::io_service& io_service, boost::shared_ptr<config> config);

  void start();
  void stop();

  void update_status(boost::shared_ptr<server_status> status);

protected:
  void receive();

  void handle_receive(
    boost::shared_ptr<connection_udp> new_connection,
    const boost::system::error_code & error,
    std::size_t bytes_transferred
  );

  inline boost::shared_ptr<rrdb> get_rrdb() const {
    return _rrdb;
  }

private:
  boost::shared_ptr<thread_pool>                  _thread_pool;
  boost::shared_ptr<rrdb>                         _rrdb;
  boost::shared_ptr<boost::asio::ip::udp::socket> _socket;

  std::string  _address;
  int          _port;
  std::size_t  _thread_pool_size;
  std::size_t  _buffer_size;
  bool         _send_success_response;
}; // server_udp

#endif /* SERVER_UDP_H_ */