/*
 * parsers_tests.cpp
 *
 *  Created on: Jun 22, 2013
 *      Author: aleksey
 */

#include "tests/parsers_tests.h"

#include "parser/interval.h"
#include "parser/retention_policy.h"
#include "parser/memory_size.h"

#include "tests/stats_rrdb_tests.h"

#include "common/log.h"
#include "common/config.h"
#include "common/exception.h"

parsers_tests::parsers_tests()
{
}

parsers_tests::~parsers_tests()
{
}

void parsers_tests::run()
{
  parsers_tests test;
  test.test_interval(0);
  test.test_retention_policy(1);
  test.test_memory_size(2);
}

// tests
void parsers_tests::test_interval(const int & n)
{
  TEST_SUBTEST_START(n, "interval parser/serializer", false);

  // parser
  TEST_CHECK_EQUAL(interval_parse("1 sec"),   1 * INTERVAL_SEC);
  TEST_CHECK_EQUAL(interval_parse("5 secs"),  5 * INTERVAL_SEC);
  TEST_CHECK_EQUAL(interval_parse("1 min"),   1 * INTERVAL_MIN);
  TEST_CHECK_EQUAL(interval_parse("12 mins"), 12 * INTERVAL_MIN);
  TEST_CHECK_EQUAL(interval_parse("1 hour"),  1 * INTERVAL_HOUR);
  TEST_CHECK_EQUAL(interval_parse("3 hours"), 3 * INTERVAL_HOUR);
  TEST_CHECK_EQUAL(interval_parse("1 day"),   1 * INTERVAL_DAY);
  TEST_CHECK_EQUAL(interval_parse("2 days"),  2 * INTERVAL_DAY);
  TEST_CHECK_EQUAL(interval_parse("1 week"),  1 * INTERVAL_WEEK);
  TEST_CHECK_EQUAL(interval_parse("2 weeks"), 2 * INTERVAL_WEEK);
  TEST_CHECK_EQUAL(interval_parse("1 month"), 1 * INTERVAL_MONTH);
  TEST_CHECK_EQUAL(interval_parse("5 months"),5 * INTERVAL_MONTH);
  TEST_CHECK_EQUAL(interval_parse("1 year"),  1 * INTERVAL_YEAR);
  TEST_CHECK_EQUAL(interval_parse("3 years"), 3 * INTERVAL_YEAR);

  // parser bad strings
  TEST_CHECK_THROW(interval_parse(""), "Parser error: expecting <interval value> at \"\"");
  TEST_CHECK_THROW(interval_parse("1"), "Parser error: expecting <interval unit> at \"\"");
  TEST_CHECK_THROW(interval_parse("1 xyz"), "Parser error: expecting <interval unit> at \"xyz\"");
  TEST_CHECK_THROW(interval_parse("1 sec 123"), "Error parsing interval: unexpected '123'");

  // serializer
  TEST_CHECK_EQUAL(interval_write(100 * INTERVAL_SEC), "100 secs");
  TEST_CHECK_EQUAL(interval_write(1 * INTERVAL_SEC),   "1 sec");
  TEST_CHECK_EQUAL(interval_write(5 * INTERVAL_SEC),   "5 secs");
  TEST_CHECK_EQUAL(interval_write(1 * INTERVAL_MIN),   "1 min");
  TEST_CHECK_EQUAL(interval_write(12 * INTERVAL_MIN),  "12 mins");
  TEST_CHECK_EQUAL(interval_write(1 * INTERVAL_HOUR),  "1 hour");
  TEST_CHECK_EQUAL(interval_write(3 * INTERVAL_HOUR),  "3 hours");
  TEST_CHECK_EQUAL(interval_write(1 * INTERVAL_DAY),   "1 day");
  TEST_CHECK_EQUAL(interval_write(2 * INTERVAL_DAY),   "2 days");
  TEST_CHECK_EQUAL(interval_write(1 * INTERVAL_WEEK),  "1 week");
  TEST_CHECK_EQUAL(interval_write(2 * INTERVAL_WEEK),  "2 weeks");
  TEST_CHECK_EQUAL(interval_write(1 * INTERVAL_MONTH), "1 month");
  TEST_CHECK_EQUAL(interval_write(5 * INTERVAL_MONTH), "5 months");
  TEST_CHECK_EQUAL(interval_write(1 * INTERVAL_YEAR),  "1 year");
  TEST_CHECK_EQUAL(interval_write(3 * INTERVAL_YEAR),  "3 years");

  // done
  TEST_SUBTEST_END();
}

void parsers_tests::test_retention_policy(const int & n)
{
  TEST_SUBTEST_START(n, "retention_policy parser/serializer", false);
  t_retention_policy rp;

  // parser
  rp = retention_policy_parse("1 sec for 3 min");
  TEST_CHECK_EQUAL(rp.size(), 1);
  TEST_CHECK_EQUAL(rp[0]._freq,     1 * INTERVAL_SEC);
  TEST_CHECK_EQUAL(rp[0]._duration, 3 * INTERVAL_MIN);

  rp = retention_policy_parse("2 hours for 1 day, 6 hours for 3 months");
  TEST_CHECK_EQUAL(rp.size(), 2);
  TEST_CHECK_EQUAL(rp[0]._freq,     2 * INTERVAL_HOUR);
  TEST_CHECK_EQUAL(rp[0]._duration, 1 * INTERVAL_DAY);
  TEST_CHECK_EQUAL(rp[1]._freq,     6 * INTERVAL_HOUR);
  TEST_CHECK_EQUAL(rp[1]._duration, 3 * INTERVAL_MONTH);

  // parser bad strings
  TEST_CHECK_THROW(retention_policy_parse(""), "Parser error: expecting <memory size value> at \"\"");
  TEST_CHECK_THROW(retention_policy_parse("1"), "Parser error: expecting <memory size unit> at \"\"");
  TEST_CHECK_THROW(retention_policy_parse("1 sec for"), "Parser error: expecting <memory size unit> at \"xyz\"");
  TEST_CHECK_THROW(retention_policy_parse("1 sec for ,"), "Parser error: expecting <memory size unit> at \"xyz\"");
  TEST_CHECK_THROW(retention_policy_parse("1 sec for 2,"), "Parser error: expecting <memory size unit> at \"xyz\"");
  TEST_CHECK_THROW(retention_policy_parse("1 sec for 2 mins,"), "Parser error: expecting <memory size unit> at \"xyz\"");
  TEST_CHECK_THROW(retention_policy_parse("1 sec for 2 mins 123"), "Parser error: expecting <memory size unit> at \"xyz\"");
  TEST_CHECK_THROW(retention_policy_parse("1 sec for 2 mins, 123"), "Parser error: expecting <memory size unit> at \"xyz\"");
  TEST_CHECK_THROW(retention_policy_parse("1 sec for 2 mins, abc"), "Parser error: expecting <memory size unit> at \"xyz\"");

  // serializer
  rp.clear();
  rp.push_back(t_retention_policy_elem(2 * INTERVAL_SEC, 10 * INTERVAL_MIN));
  TEST_CHECK_EQUAL(retention_policy_write(rp), "2 secs for 10 mins");

  rp.clear();
  rp.push_back(t_retention_policy_elem(3 * INTERVAL_HOUR, 2 * INTERVAL_DAY));
  rp.push_back(t_retention_policy_elem(6 * INTERVAL_HOUR, 1 * INTERVAL_YEAR));
  TEST_CHECK_EQUAL(retention_policy_write(rp), "3 hours for 2 days, 6 hours for 1 year");

  // done
  TEST_SUBTEST_END();
}

void parsers_tests::test_memory_size(const int & n)
{
  TEST_SUBTEST_START(n, "memory_size parser/serializer", false);

  // parser
  TEST_CHECK_EQUAL(memory_size_parse("100"),     100 * MEMORY_SIZE_BYTE);
  TEST_CHECK_EQUAL(memory_size_parse("1 byte"),  1 * MEMORY_SIZE_BYTE);
  TEST_CHECK_EQUAL(memory_size_parse("5 bytes"), 5 * MEMORY_SIZE_BYTE);
  TEST_CHECK_EQUAL(memory_size_parse("4 KB"),    4 * MEMORY_SIZE_KILOBYTE);
  TEST_CHECK_EQUAL(memory_size_parse("4KB"),     4 * MEMORY_SIZE_KILOBYTE);
  TEST_CHECK_EQUAL(memory_size_parse("3 MB"),    3 * MEMORY_SIZE_MEGABYTE);
  TEST_CHECK_EQUAL(memory_size_parse("3MB"),     3 * MEMORY_SIZE_MEGABYTE);
  TEST_CHECK_EQUAL(memory_size_parse("2 GB"),    2 * MEMORY_SIZE_GIGABYTE);
  TEST_CHECK_EQUAL(memory_size_parse("2GB"),     2 * MEMORY_SIZE_GIGABYTE);

  // parser bad strings
  TEST_CHECK_THROW(memory_size_parse(""), "Parser error: expecting <memory size value> at \"\"");
  TEST_CHECK_THROW(memory_size_parse("1"), "Parser error: expecting <memory size unit> at \"\"");
  TEST_CHECK_THROW(memory_size_parse("1 xyz"), "Parser error: expecting <memory size unit> at \"xyz\"");
  TEST_CHECK_THROW(memory_size_parse("1MB 123"), "Error parsing interval: unexpected '123'");


  // serializer
  TEST_CHECK_EQUAL(memory_size_write(100 * MEMORY_SIZE_BYTE),   "100 bytes");
  TEST_CHECK_EQUAL(memory_size_write(1 * MEMORY_SIZE_BYTE),     "1 byte");
  TEST_CHECK_EQUAL(memory_size_write(5 * MEMORY_SIZE_BYTE),     "5 bytes");
  TEST_CHECK_EQUAL(memory_size_write(4 * MEMORY_SIZE_KILOBYTE), "4KB");
  TEST_CHECK_EQUAL(memory_size_write(3 * MEMORY_SIZE_MEGABYTE), "3MB");
  TEST_CHECK_EQUAL(memory_size_write(2 * MEMORY_SIZE_GIGABYTE), "2GB");

  // done
  TEST_SUBTEST_END();
}

