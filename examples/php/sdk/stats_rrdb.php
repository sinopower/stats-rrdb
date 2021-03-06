<?php
/*
 * stats_rrdb.php
 *
 *  Created on: Jun 9, 2013
 *      Author: aleksey
 */

class StatsRRDB {
	private $_host;
	private $_port;
	private $_buf_size;
	
	private $_udp_socket; // 

	/**
	 * Creates a StatsRRDB client. In the most common case the TCP and UDP host/port
	 * will be the same so one client object can be used for both. If the TCP and UDP
	 * configuration is different then two clients can be used one for each protocol.
	 */
	public function __construct($host = '127.0.0.1', $port = 9876, $buf_size = 1024)
	{
		$this->_host     = $host;
		$this->_port     = $port;
		$this->_buf_size = $buf_size;
	}
	
	/**
	 * Update metric
	 */
	public function update($metric_name, $value = 1, $ts = false)
	{
		// set $ts if not set
		if($ts === false) {
			$ts = $_SERVER['REQUEST_TIME'];
		}			
		$this->send_udp_command("u|{$metric_name}|{$value}|{$ts}");
	}
	
	/**
	 * Queries data from metric
	 */
	public function select($metric_name, $columns, $from_ts, $to_ts, $group_by = false) {
		// construct query
		$query = "select {$columns} from '{$metric_name}' between {$from_ts} and {$to_ts}";
		if(!empty($group_by)) {
			$query .= " group by {$group_by} ";
		}
		
		// execute query
		$res = $this->send_tcp_command("{$query};");
		if(!$res) {
			return array();
		}
	
		// process results: just parse CSV and extract the column
		return self::parse_csv($res);
	}
	
	/**
	 * Queries the list of all available metrics
	 */
	public function show_metrics($like = false) {
		// construct query
		$query = "show metrics";
		if(!empty($like)) {
			$query .= " like '{$like}'";
		}
	
		// execute query
		$res = $this->send_tcp_command("{$query};");
		if(!$res) {
			return array();
		}
	
		// process results: break and sort
		$metrics_list = preg_split("/\n/", $res, -1, PREG_SPLIT_NO_EMPTY);
		sort($metrics_list);
		return $metrics_list;
	}
	
	/**
	 * Queries the server status
	 */
	public function show_status($like = false) {
		// construct query
		$query = "show status";
		if(!empty($like)) {
			$query .= " like '{$like}'";
		}
	
		// execute query
		$res = $this->send_tcp_command("{$query};");
		if(!$res) {
			return array();
		}
		$data = self::parse_csv($res);
	
		// process results: normalize data a into n => v pairs
		$ret = array();
		foreach($data as $d) {
			$ret[ $d[0] ] = $d[2];
		}
		return $ret;
	}	
	
	/**
	 * Creates a metric
	 */
	public function create_metric($name, $retention_policy = false) {
		// construct query
		$query = "create metric '{$name}'";
		if(!empty($retention_policy)) {
			$query .= " KEEP $retention_policy";
		}
		
		// execute query
		return $this->send_tcp_command("{$query};");
	}
	
	/**
	 * Drops a metric
	 */
	public function drop_metric($name) {
		// construct query
		$query = "drop metric '{$name}'";
	
		// execute query
		return $this->send_tcp_command("{$query};");
	}
	
	/**
	 * Show metric policy
	 */
	public function show_metric_policy($name) {
		// construct query
		$query = "show metric policy '{$name}'";
	
		// execute query
		return $this->send_tcp_command("{$query};");
	}
				
	/**
	 * Sends a TCP command
	 */
	public function send_tcp_command($command)
	{
		// connect - we can not re-use tcp socket
		$tcp_socket = $this->connect("tcp", $this->_host, $this->_port);
		
		// send command and shutdown the socket on our end to indicate
		// we are done
		fwrite($tcp_socket, $command);
		stream_socket_shutdown($tcp_socket, STREAM_SHUT_WR);
		
		// read data
		$res = '';
		while (!feof($tcp_socket)) {
			$res .= fread($tcp_socket, $this->_buf_size);
		}
		
		// close socket
		fclose($tcp_socket);
		
		// check for errors
		if(preg_match('/^ERROR:/', $res)) {
			throw new Exception($res);
		}
		
		// done
		return $res;		
	}
	
	/**
	 * Sends a UDP command
	 */
	public function send_udp_command($command)
	{
		// connect if needed - we reuse UDP socket, it's fine
		if(!$this->_udp_socket) {
			$this->_udp_socket= $this->connect("udp", $this->_host, $this->_port);
			stream_set_blocking($this->_udp_socket, 0);
		}
			
		// write and flush to force send udp
		fwrite($this->_udp_socket, $command);
		fflush($this->_udp_socket);	
	}

	/**
	 * Opens socket to protocol://host:port
	 */
	private static function connect($protocol, $host, $port) {
		// open socket
		$fp = stream_socket_client("{$protocol}://{$host}:{$port}", $errno, $errstr, 30);
		if (!$fp) {
			throw new Exception("Can not connect to stats_rrdb at '{$protocol}://{$host}:{$port}': {$errstr} ({$errno})\n");
		}
		return $fp;
	}
	
	/**
	 * Parses CSV data and extracts a specified column (if needed)
	 */
	private static function parse_csv(&$string) {
		// break into lines
		$lines = preg_split("/\n/", $string);
		if(empty($lines)) {
			throw new Exception("No lines in the results");
		}
		unset($string);
		
		// and then break each line into pieces
		$ii = 0;
		$ret = array();
		foreach($lines as $line) {
			$data = preg_split('/,/', $line, -1, PREG_SPLIT_NO_EMPTY);
			if(empty($data)) continue;

			$ret[] = $data;
			++$ii;
		}
		
		// done
		return $ret;		
	}
}; // class StatsRRDB

