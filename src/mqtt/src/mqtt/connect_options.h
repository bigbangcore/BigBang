/////////////////////////////////////////////////////////////////////////////
/// @file connect_options.h
/// Declaration of MQTT connect_options class
/// @date May 1, 2013
/// @author Frank Pagliughi
/////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * Copyright (c) 2013-2016 Frank Pagliughi <fpagliughi@mindspring.com>
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Frank Pagliughi - initial implementation and documentation
 *******************************************************************************/

#ifndef __mqtt_connect_options_h
#define __mqtt_connect_options_h

#include "MQTTAsync.h"
#include "mqtt/types.h"
#include "mqtt/message.h"
#include "mqtt/topic.h"
#include "mqtt/token.h"
#include "mqtt/string_collection.h"
#include "mqtt/will_options.h"
#include "mqtt/ssl_options.h"
#include <vector>
#include <chrono>

namespace mqtt {

/////////////////////////////////////////////////////////////////////////////

/**
 * Holds the set of options that control how the client connects to a
 * server.
 */
class connect_options
{
	/** The default C struct */
	static const MQTTAsync_connectOptions DFLT_C_STRUCT ;

	/** The underlying C connection options */
	MQTTAsync_connectOptions opts_;

	/** The LWT options */
	will_options will_;

	/** The SSL options  */
	ssl_options ssl_;

	/** The user name to use for the connection. */
	string_ref userName_;

	/** The password to use for the connection. */
	binary_ref password_;

	/** Shared token pointer for context, if any */
	token_ptr tok_;

	/** Collection of server URIs, if any */
	const_string_collection_ptr serverURIs_;

	/** The connect properties */
	properties props_;

	/** The client has special access */
	friend class async_client;
	friend class connect_options_test;
	friend class token_test;

	/**
	 * Gets a pointer to the C-language NUL-terminated strings for the
	 * struct.
	 * @note In the connect options, by default, the Paho C treats
	 * nullptr char arrays as unset values, so we keep that semantic and
	 * only set those char arrays if the string is non-empty.
	 * @param str The C++ string object.
	 * @return Pointer to a NUL terminated string. This is only valid until
	 *  	   the next time the string is updated.
	 */
	const char* c_str(const string_ref& sr) {
		return sr.empty() ? nullptr : sr.c_str();
	}

public:
	/** Smart/shared pointer to an object of this class. */
	using ptr_t = std::shared_ptr<connect_options>;
	/** Smart/shared pointer to a const object of this class. */
	using const_ptr_t = std::shared_ptr<const connect_options>;

	/**
	 * Constructs a new object using the default values.
	 */
	connect_options();
	/**
	 * Constructs a new object using the specified user name and password.
	 * @param userName The name of the user for connecting to the server
	 * @param password The password for connecting to the server
	 */
	connect_options(string_ref userName, binary_ref password);
	/**
	 * Copy constructor.
	 * @param opt Another object to copy.
	 */
	connect_options(const connect_options& opt);
	/**
	 * Move constructor.
	 * @param opt Another object to move into this new one.
	 */
	connect_options(connect_options&& opt);
	/**
	 * Copy assignment.
	 * @param opt Another object to copy.
	 */
	connect_options& operator=(const connect_options& opt);
	/**
	 * Move assignment.
	 * @param opt Another object to move into this new one.
	 */
	connect_options& operator=(connect_options&& opt);
	/**
	 * Gets the "keep alive" interval.
	 * @return The keep alive interval in seconds.
	 */
	std::chrono::seconds get_keep_alive_interval() const {
		return std::chrono::seconds(opts_.keepAliveInterval);
	}
	/**
	 * Gets the connection timeout.
	 * This is the amount of time the underlying library will wait for a
	 * timeout before failing.
	 * @return The connect timeout in seconds.
	 */
	std::chrono::seconds get_connect_timeout() const {
		return std::chrono::seconds(opts_.connectTimeout);
	}
	/**
	 * Gets the user name to use for the connection.
	 * @return The user name to use for the connection.
	 */
	string get_user_name() const { return userName_ ? userName_.to_string() : string(); }
	/**
	 * Gets the password to use for the connection.
	 * @return The password to use for the connection.
	 */
	binary_ref get_password() const { return password_; }
	/**
	 * Gets the password to use for the connection.
	 * @return The password to use for the connection.
	 */
	string get_password_str() const {
		return password_ ? password_.to_string() : string();
	}
	/**
	 * Gets the maximum number of messages that can be in-flight
	 * simultaneously.
	 * @return The maximum number of inflight messages.
	 */
	int get_max_inflight() const { return opts_.maxInflight; }
	/**
	 * Gets the topic to be used for last will and testament (LWT).
	 * @return The topic to be used for last will and testament (LWT).
	 */
	string get_will_topic() const {
		return will_.get_topic();
	}
	/**
	 * Gets the message to be sent as last will and testament (LWT).
	 * @return The message to be sent as last will and testament (LWT).
	 */
	const_message_ptr get_will_message() const {
		return will_.get_message();
	}
	/**
	 * Get the LWT options to use for the connection.
	 * @return The LWT options to use for the connection.
	 */
	const will_options& get_will_options() const { return will_; }
	/**
	 * Get the SSL options to use for the connection.
	 * @return The SSL options to use for the connection.
	 */
	const ssl_options& get_ssl_options() const { return ssl_; }
	/**
	 * Sets the SSL for the connection.
	 * These will only have an effect if compiled against the SSL version of
	 * the Paho C library.
	 * @param ssl The SSL options.
	 */
	void set_ssl(const ssl_options& ssl);
	void set_ssl(ssl_options&& ssl);
	/**
	 * Returns whether the server should remember state for the client
	 * across reconnects.
	 * @return @em true if requesting a clean session, @em false if not.
	 */
	bool is_clean_session() const { return to_bool(opts_.cleansession); }
	/**
	 * Gets the token used as the callback context.
	 * @return The delivery token used as the callback context.
	 */
	token_ptr get_token() const { return tok_; }
	/**
	 * Gets the list of servers to which the client will connect.
	 * @return A collection of server URI's. Each entry should be of the
	 *  	   form @em protocol://host:port where @em protocol must be tcp
	 *  	   or @em ssl. For @em host, you can specify either an IP
	 *  	   address or a domain name.
	 */
	const_string_collection_ptr get_servers() const { return serverURIs_; }
	/**
      * Gets the version of MQTT to be used on the connect.
	  * @return
	  * @li MQTTVERSION_DEFAULT (0) = default: start with 3.1.1, and if that
	  *     fails, fall back to 3.1
	  * @li MQTTVERSION_3_1 (3) = only try version 3.1
	  * @li MQTTVERSION_3_1_1 (4) = only try version 3.1.1
	  */
	int get_mqtt_version() const { return opts_.MQTTVersion; }
	/**
	 * Determines if the options have been configured for automatic
	 * reconnect.
	 * @return @em true if configured for automatic reconnect, @em false if
	 *  	   not.
	 */
	bool get_automatic_reconnect() const { return to_bool(opts_.automaticReconnect); }
	/**
	 * Gets the minimum retry interval for automatic reconnect.
	 * @return The minimum retry interval for automatic reconnect, in
	 *  	   seconds.
	 */
	std::chrono::seconds get_min_retry_interval() const {
		return std::chrono::seconds(opts_.minRetryInterval);
	}
	/**
	 * Gets the maximum retry interval for automatic reconnect.
	 * @return The maximum retry interval for automatic reconnect, in
	 *  	   seconds.
	 */
	std::chrono::seconds get_max_retry_interval() const {
		return std::chrono::seconds(opts_.maxRetryInterval);
	}

	/**
	 * Sets whether the server should remember state for the client across
	 * reconnects.
	 * @param cleanSession
	 */
	void set_clean_session(bool cleanSession) {
		opts_.cleansession = to_int(cleanSession);
	}
	/**
	 * Sets the "keep alive" interval.
	 * This is the maximum time that should pass without communications
	 * between client and server. If no massages pass in this time, the
	 * client will ping the broker.
	 * @param keepAliveInterval The keep alive interval in seconds.
	 */
	void set_keep_alive_interval(int keepAliveInterval) {
		opts_.keepAliveInterval = keepAliveInterval;
	}
	/**
	 * Sets the "keep alive" interval with a chrono duration.
	 * This is the maximum time that should pass without communications
	 * between client and server. If no massages pass in this time, the
	 * client will ping the broker.
	 * @param interval The keep alive interval.
	 */
	template <class Rep, class Period>
	void set_keep_alive_interval(const std::chrono::duration<Rep, Period>& interval) {
		// TODO: Check range
		set_keep_alive_interval((int) to_seconds_count(interval));
	}
	/**
	 * Sets the connect timeout in seconds.
	 * This is the maximum time that the underlying library will wait for a
	 * connection before failing.
	 * @param timeout The connect timeout in seconds.
	 */
	void set_connect_timeout(int timeout) {
		opts_.connectTimeout = timeout;
	}
	/**
	 * Sets the connect timeout with a chrono duration.
	 * This is the maximum time that the underlying library will wait for a
	 * connection before failing.
	 * @param timeout The connect timeout in seconds.
	 */
	template <class Rep, class Period>
	void set_connect_timeout(const std::chrono::duration<Rep, Period>& timeout) {
		// TODO: check range
		set_connect_timeout((int) to_seconds_count(timeout));
	}
	/**
	 * Sets the user name to use for the connection.
	 * @param userName
	 */
	void set_user_name(string_ref userName);
	/**
	 * Sets the password to use for the connection.
	 */
	void set_password(binary_ref password);
	/**
	 * Sets the maximum number of messages that can be in-flight
	 * simultaneously.
	 * @param n The maximum number of inflight messages.
	 */
	void set_max_inflight(int n) { opts_.maxInflight = n; }
	/**
	 * Sets the "Last Will and Testament" (LWT) for the connection.
	 * @param will The LWT options.
	 */
	void set_will(const will_options& will);
	void set_will(will_options&& will);
	/**
	 * Sets the "Last Will and Testament" (LWT) as a message
	 * @param msg The LWT message
	 */
	void set_will_message(const message& msg) {
		set_will(will_options(msg));
	}
	/**
	 * Sets the "Last Will and Testament" (LWT) as a message
	 * @param msg Pointer to a LWT message
	 */
	void set_will_message(const_message_ptr msg) {
		if (msg) set_will(will_options(*msg));
	}
	/**
	 * Sets the callback context to a delivery token.
	 * @param tok The delivery token to be used as the callback context.
	 */
	void set_token(const token_ptr& tok);
	/**
	 * Sets the list of servers to which the client will connect.
	 * @param serverURIs A pointer to a collection of server URI's. Each
	 *  				 entry should be of the form @em
	 *  				 protocol://host:port where @em protocol must be
	 *  				 @em tcp or @em ssl. For @em host, you can specify
	 *  				 either an IP address or a domain name.
	 */
	void set_servers(const_string_collection_ptr serverURIs);
	/**
	  * Sets the version of MQTT to be used on the connect.
	  *
	  * This will also set other connect options to legal values dependent on
	  * the selected version.
	  *
	  * @param mqttVersion The MQTT version to use for the connection:
	  *   @li MQTTVERSION_DEFAULT (0) = default: start with 3.1.1, and if
	  *       that fails, fall back to 3.1
	  *   @li MQTTVERSION_3_1 (3) = only try version 3.1
	  *   @li MQTTVERSION_3_1_1 (4) = only try version 3.1.1
	  *   @li MQTTVERSION_5 (5) = only try version 5
	  */
	void set_mqtt_version(int mqttVersion);
	/**
	 * Enable or disable automatic reconnects.
	 * The retry intervals are not affected.
	 * @param on Whether to turn reconnects on or off
	 */
	void set_automatic_reconnect(bool on) {
		opts_.automaticReconnect = to_int(on);
	}
	/**
	 * Enable or disable automatic reconnects.
	 * @param minRetryInterval Minimum retry interval in seconds.  Doubled
	 *  					   on each failed retry.
	 * @param maxRetryInterval Maximum retry interval in seconds.  The
	 *  					   doubling stops here on failed retries.
	 */
	void set_automatic_reconnect(int minRetryInterval, int maxRetryInterval);
	/**
	 * Enable or disable automatic reconnects.
	 * @param minRetryInterval Minimum retry interval. Doubled on each
	 *  					   failed retry.
	 * @param maxRetryInterval Maximum retry interval. The doubling stops
	 *  					   here on failed retries.
	 */
	template <class Rep1, class Period1, class Rep2, class Period2>
	void set_automatic_reconnect(const std::chrono::duration<Rep1, Period1>& minRetryInterval,
								 const std::chrono::duration<Rep2, Period2>& maxRetryInterval) {
		set_automatic_reconnect((int) to_seconds_count(minRetryInterval),
								(int) to_seconds_count(maxRetryInterval));
	}
	/**
	 * Determines if the 'clean start' flag is set for the connect.
	 * @return @em true if the 'clean start' flag is set for the connect, @em
	 *  	   false if not.
	 */
	bool is_clean_start() const {
		return to_bool(opts_.cleanstart);
	}
	/**
	 * Sets the 'clean start' flag for the connection.
	 * @param cleanStart Whether to set the 'clean start' flag for the connect.
	 */
	void set_clean_start(bool cleanStart) {
		opts_.cleanstart = to_int(cleanStart);
	}
	/**
	 * Gets the connect properties.
	 * @return A const reference to the properties for the connect.
	 */
	const properties& get_properties() const {
		return props_;
	}
	/**
	 * Sets the properties for the connect.
	 * @param props The properties to place into the message.
	 */
	void set_properties(const properties& props) {
		props_ = props;
		opts_.connectProperties = const_cast<MQTTProperties*>(&props_.c_struct());
	}
	/**
	 * Moves the properties for the connect.
	 * @param props The properties to move into the connect object.
	 */
	void set_properties(properties&& props) {
		props_ = props;
		opts_.connectProperties = const_cast<MQTTProperties*>(&props_.c_struct());
	}
	/**
	 * Gets a string representation of the object.
	 * @return A string representation of the object.
	 */
	string to_string() const;
};

/** Smart/shared pointer to a connection options object. */
using connect_options_ptr = connect_options::ptr_t;

/////////////////////////////////////////////////////////////////////////////
// end namespace mqtt
}

#endif		// __mqtt_connect_options_h

