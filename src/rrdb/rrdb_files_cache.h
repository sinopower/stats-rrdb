/*
 * rrdb_files_cache.h
 *
 *  Created on: Jun 20, 2013
 *      Author: aleksey
 */

#ifndef RRDB_FILE_CACHE_H_
#define RRDB_FILE_CACHE_H_

#include <string>
#include <fstream>

#include <boost/shared_ptr.hpp>

#include "rrdb/rrdb.h"

#include "common/types.h"
#include "common/spinlock.h"

class rrdb;
class config;
class rrdb_files_cache_impl;

class rrdb_files_cache
{
public:
  typedef boost::shared_ptr<std::fstream> fstream_ptr;

public:
  rrdb_files_cache();
  virtual ~rrdb_files_cache();

  void initialize(boost::shared_ptr<config> config);
  void clear_cache();

  my::size_t get_max_size() const;
  void set_max_size(const my::size_t & max_size);

  std::string get_path() const;
  void set_path(const std::string & path);

  my::size_t get_cache_size() const;
  my::size_t get_cache_hits() const;
  my::size_t get_cache_misses() const;

  fstream_ptr open_file(const std::string & filename, bool new_file = false);
  void move_file(const std::string & from, const std::string & to);
  void delete_file(const std::string & filename);

  std::string get_filename(const std::string & metric_name) const;
  void load_metrics(const boost::shared_ptr<rrdb> & rrdb, rrdb::t_metrics_map & metrics);

private:
  void purge();
  std::string get_full_path(const std::string & filename) const;

private:
  mutable spinlock _lock;

  // params
  std::string      _path;
  my::size_t       _max_size;
  // data
  boost::shared_ptr<rrdb_files_cache_impl> _files_cache_impl;

  // stats
  my::size_t      _cache_hits;
  my::size_t      _cache_misses;
}; // rrdb_files_cache

#endif /* RRDB_FILE_CACHE_H_ */