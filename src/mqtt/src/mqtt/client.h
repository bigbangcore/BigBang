/////////////////////////////////////////////////////////////////////////////
/// @file client.h
/// Declaration of MQTT client class
/// @date May 1, 2013
/// @author Frank Pagliughi
/////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * Copyright (c) 2013-2017 Frank Pagliughi <fpagliughi@mindspring.com>
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

#ifndef __mqtt_client_h
#define __mqtt_client_h

#include "mqtt/async_client.h"
#include <future>

namespace mqtt {

/////////////////////////////////////////////////////////////////////////////

/**
 * Lightweight client for talking to an MQTT server using methods that block
 * until an operation completes.
 */
class client : private callback
{
	/** An arbitrary, but relatively long timeout */
	static const std::chrono::minutes DFLT_TIMEOUT;
	/** The default quality of service */
	static constexpr int DFLT_QOS = 1;

	/** The actual client */
	async_client cli_;
	/** The longest time to wait for an operation to complete.  */
	std::chrono::milliseconds timeout_;
	/** Callback supplied by the user (if any) */
	callback* userCallback_;

	/**
	 * Creates a shared pointer to an existing non-heap object.
	 * The shared pointer is given a no-op deleter, so it will not try to
	 * destroy the object when it goes out of scope. It is up to the caller
	 * to ensure that the object remains in memory for as long as there may
	 * be pointers to it.
	 * @param val A value which may live anywhere in memory (stack,
	 *  		  file-scope, etc).
	 * @return A shared pointer to the object.
	 */
	template <typename T>
	std::shared_ptr<T> ptr(const T& val) {
		return std::shared_ptr<T>(const_cast<T*>(&val), [](T*){});
	}

	// User callbacks
	// Most are launched in a separate thread, for convenience, except
	// message_arrived, for performance.
	void connected(const string& cause) override {
		std::async(std::launch::async, &callback::connected, userCallback_, cause);
	}
	void connection_lost(const string& cause) override {
		std::async(std::launch::async,
				   &callback::connection_lost, userCallback_, cause);
	}
	void message_arrived(const_message_ptr msg) override {
		userCallback_->message_arrived(msg);
	}
	void delivery_complete(delivery_token_ptr tok) override {
		std::async(std::launch::async, &callback::delivery_complete, userCallback_, tok);
	}

	/** Non-copyable */
	client() =delete;
	client(const async_client&) =delete;
	client& operator=(const async_client&) =delete;

public:
	/** Smart pointer type for this object */
	using ptr_t = std::shared_ptr<client>;
	/** Type for a collection of QOS values */
	using qos_collection = async_client::qos_collection;

	/**
	 * Create a client that can be used to communicate with an MQTT server.
	 * This allows the caller to specify a user-defined persistence object,
	 * or use no persistence.
	 * @param serverURI the address of the server to connect to, specified
	 *  				as a URI.
	 * @param clientId a client identifier that is unique on the server
	 *  			   being connected to
	 * @param persistence The user persistence structure. If this is null,
	 *  				  then no persistence is used.
	 */
	client(const string& serverURI, const string& clientId,
		   iclient_persistence* persistence=nullptr);
	/**
	 * Create an async_client that can be used to communicate with an MQTT
	 * server.
	 * This uses file-based persistence in the specified directory.
	 * @param serverURI the address of the server to connect to, specified
	 *  				as a URI.
	 * @param clientId a client identifier that is unique on the server
	 *  			   being connected to
	 * @param persistDir The directory to use for persistence data
	 */
	client(const string& serverURI, const string& clientId,
		   const string& persistDir);
	/**
	 * Create a client that can be used to communicate with an MQTT server,
	 * which allows for off-line message buffering.
	 * This allows the caller to specify a user-defined persistence object,
	 * or use no persistence.
	 * @param serverURI the address of the server to connect to, specified
	 *  				as a URI.
	 * @param clientId a client identifier that is unique on the server
	 *  			   being connected to
	 * @param maxBufferedMessages the maximum number of messages allowed to
	 *  						  be buffered while not connected
	 * @param persistence The user persistence structure. If this is null,
	 *  				  then no persistence is used.
	 */
	client(const string& serverURI, const string& clientId,
		   int maxBufferedMessages, iclient_persistence* persistence=nullptr);
	/**
	 * Create a client that can be used to communicate with an MQTT server,
	 * which allows for off-line message buffering.
	 * This uses file-based persistence in the specified directory.
	 * @param serverURI the address of the server to connect to, specified
	 *  				as a URI.
	 * @param clientId a client identifier that is unique on the server
	 *  			   being connected to
	 * @param maxBufferedMessages the maximum number of messages allowed to
	 *  						  be buffered while not connected
	 * @param persistDir The directory to use for persistence data
	 */
	client(const string& serverURI, const string& clientId,
		   int maxBufferedMessages, const string& persistDir);
	/**
	 * Virtual destructor
	 */
	virtual ~client() {}
	/**
	 * Connects to an MQTT server using the default options.
	 */
	virtual connect_response connect();
	/**
	 * Connects to an MQTT server using the specified options.
	 * @param opts
	 */
	virtual connect_response connect(connect_options opts);
	/**
	 * Reconnects the client using options from the previous connect.
	 * The client must have previously called connect() for this to work.
	 */
	virtual connect_response reconnect();
	/**
	 * Disconnects from the server.
	 */
	virtual void disconnect();
	/**
	 * Disconnects from the server.
	 * @param timeoutMS the amount of time in milliseconds to allow for
	 *  			  existing work to finish before disconnecting. A value
	 *  			  of zero or less means the client will not quiesce.
	 */
	virtual void disconnect(int timeoutMS) {
		cli_.stop_consuming();
		cli_.disconnect(timeoutMS)->wait_for(timeout_);
	}
	/**
	 * Disconnects from the server.
	 * @param to the amount of time in milliseconds to allow for
	 *  			  existing work to finish before disconnecting. A value
	 *  			  of zero or less means the client will not quiesce.
	 */
	template <class Rep, class Period>
	void disconnect(const std::chrono::duration<Rep, Period>& to) {
		disconnect((int) to_milliseconds_count(to));
	}
	/**
	 * Gets the client ID used by this client.
	 * @return The client ID used by this client.
	 */
	virtual string get_client_id() const { return cli_.get_client_id(); }
	/**
	 * Gets the address of the server used by this client.
	 * @return The address of the server used by this client, as a URI.
	 */
	virtual string get_server_uri() const { return cli_.get_server_uri(); }
	/**
	 * Return the maximum time to wait for an action to complete.
	 * @return int
	 */
	virtual std::chrono::milliseconds get_timeout() const { return timeout_; }
	/**
	 * Get a topic object which can be used to publish messages on this
	 * client.
	 * @param top The topic name
	 * @return A topic attached to this client.
	 */
	virtual topic get_topic(const string& top) { return topic(cli_, top); }
	/**
	 * Determines if this client is currently connected to the server.
	 * @return @em true if the client is currently connected, @em false if
	 *  	   not.
	 */
	virtual bool is_connected() const { return cli_.is_connected(); }

	/**
	 * Publishes a message to a topic on the server and return once it is
	 * delivered.
	 * @param top The topic to publish
	 * @param payload The data to publish
	 * @param n The size in bytes of the data
	 * @param qos The QoS for message delivery
	 * @param retained Whether the broker should retain the message
	 */
	virtual void publish(string_ref top, const void* payload, size_t n,
						 int qos, bool retained) {
		cli_.publish(std::move(top), payload, n, qos, retained)->wait_for(timeout_);
	}
	/**
	 * Publishes a message to a topic on the server and return once it is
	 * delivered.
	 * @param top The topic to publish
	 * @param payload The data to publish
	 * @param n The size in bytes of the data
	 */
	virtual void publish(string_ref top, const void* payload, size_t n) {
		cli_.publish(std::move(top), payload, n)->wait_for(timeout_);
	}
	/**
	 * Publishes a message to a topic on the server.
	 * @param msg The message
	 */
	virtual void publish(const_message_ptr msg) {
		cli_.publish(msg)->wait_for(timeout_);
	}
	/**
	 * Publishes a message to a topic on the server.
	 * This version will not timeout since that could leave the library with
	 * a reference to memory that could disappear while the library is still
	 * using it.
	 * @param msg The message
	 */
	virtual void publish(const message& msg) {
		cli_.publish(ptr(msg))->wait();
	}
	/**
	 * Sets the callback listener to use for events that happen
	 * asynchronously.
	 * @param cb The callback functions
	 */
	virtual void set_callback(callback& cb);
	/**
	 * Set the maximum time to wait for an action to complete.
	 * @param timeoutMS The timeout in milliseconds
	 */
	virtual void set_timeout(int timeoutMS) {
		timeout_ = std::chrono::milliseconds(timeoutMS);
	}
	/**
	 * Set the maximum time to wait for an action to complete.
	 * @param to The timeout as a std::chrono duration.
	 */
	template <class Rep, class Period>
	void set_timeout(const std::chrono::duration<Rep, Period>& to) {
		timeout_ = to_milliseconds(to);
	}
	/**
	 * Subscribe to a topic, which may include wildcards using a QoS of 1.
	 * @param topicFilter
	 */
	virtual subscribe_response subscribe(const string& topicFilter);
	/**
	 * Subscribe to a topic, which may include wildcards.
	 * @param topicFilter A single topic to subscribe
	 * @param qos The QoS of the subscription
	 */
	virtual subscribe_response subscribe(const string& topicFilter, int qos);
	/**
	 * Subscribes to a one or more topics, which may include wildcards using
	 * a QoS of 1.
	 * @param topicFilters A set of topics to subscribe
	 */
	virtual subscribe_response subscribe(const string_collection& topicFilters);
	/**
	 * Subscribes to multiple topics, each of which may include wildcards.
	 * @param topicFilters A collection of topics to subscribe
	 * @param qos A collection of QoS for each topic
	 */
	virtual subscribe_response subscribe(const string_collection& topicFilters,
										 const qos_collection& qos);
	/**
	 * Requests the server unsubscribe the client from a topic.
	 * @param topicFilter A single topic to unsubscribe.
	 */
	virtual unsubscribe_response unsubscribe(const string& topicFilter);
	/**
	 * Requests the server unsubscribe the client from one or more topics.
	 * @param topicFilters A collection of topics to unsubscribe.
	 */
	virtual unsubscribe_response unsubscribe(const string_collection& topicFilters);
	/**
	 * Start consuming messages.
	 * This initializes the client to receive messages through a queue that
	 * can be read synchronously.
	 */
	void start_consuming() { cli_.start_consuming(); }
	/**
	 * Stop consuming messages.
	 * This shuts down the internal callback and discards any unread
	 * messages.
	 */
	void stop_consuming() { cli_.stop_consuming(); }
	/**
	 * Read the next message from the queue.
	 * This blocks until a new message arrives.
	 * @return The message and topic.
	 */
	const_message_ptr consume_message() { return cli_.consume_message(); }
	/**
	 * Try to read the next message from the queue without blocking.
	 * @param msg Pointer to the value to receive the message
	 * @return @em true is a message was read, @em false if no message was
	 *  	   available.
	 */
	bool try_consume_message(const_message_ptr* msg) {
		return cli_.try_consume_message(msg);
	}
	/**
	 * Waits a limited time for a message to arrive.
	 * @param msg Pointer to the value to receive the message
	 * @param relTime The maximum amount of time to wait for a message.
	 * @return @em true if a message was read, @em false if a timeout
	 *  	   occurred.
	 */
	template <typename Rep, class Period>
	bool try_consume_message_for(const_message_ptr* msg,
								 const std::chrono::duration<Rep, Period>& relTime) {
		return cli_.try_consume_message_for(msg, relTime);
	}
	/**
	 * Waits until a specific time for a message to occur.
	 * @param msg Pointer to the value to receive the message
	 * @param absTime The time point to wait until, before timing out.
	 * @return @em true if a message was read, @em false if a timeout
	 *  	   occurred.
	 */
	template <class Clock, class Duration>
	bool try_consume_message_until(const_message_ptr* msg,
								   const std::chrono::time_point<Clock,Duration>& absTime) {
		return cli_.try_consume_message_until(msg, absTime);
	}
};

/** Smart/shared pointer to an MQTT synchronous client object */
using client_ptr = client::ptr_t;

/////////////////////////////////////////////////////////////////////////////
// end namespace mqtt
}

#endif		// __mqtt_client_h

