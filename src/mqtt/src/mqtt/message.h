/////////////////////////////////////////////////////////////////////////////
/// @file message.h
/// Declaration of MQTT message class
/// @date May 1, 2013
/// @author Frank Pagliughi
/////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * Copyright (c) 2013-2019 Frank Pagliughi <fpagliughi@mindspring.com>
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
 *    Frank Pagliughi - MQTT v5 support (properties)
 *******************************************************************************/

#ifndef __mqtt_message_h
#define __mqtt_message_h

#include "MQTTAsync.h"
#include "mqtt/buffer_ref.h"
#include "mqtt/properties.h"
#include "mqtt/exception.h"
#include <memory>

namespace mqtt {

/////////////////////////////////////////////////////////////////////////////

/**
 * An MQTT message holds everything required for an MQTT PUBLISH message.
 * This holds the binary message payload, topic string, and all the
 * additional meta-data for an MQTT message. 
 *
 * The topic and payload buffers are kept as references to const data, so
 * they can be reassigned as needed, but the buffers can not be updated
 * in-place. Normally they would be created externally then copied or moved
 * into the message. The library to transport the messages never touchec the
 * payloads or topics.
 *
 * This also means that message objects are farily cheap to copy, since they
 * don't copy the payloads. They simply copy the reference to the buffers.
 * It is safe to pass these buffer references across threads since all
 * references promise not to update the contents of the buffer.
 */
class message
{
public:
	/** The default QoS for a message */
	static constexpr int DFLT_QOS = 0;
	/** The default retained flag */
	static constexpr bool DFLT_RETAINED = false;

private:
	/** Initializer for the C struct (from the C library) */
	static const MQTTAsync_message DFLT_C_STRUCT;

	/** The underlying C message struct */
	MQTTAsync_message msg_;
	/** The topic that the message was (or should be) sent on. */
	string_ref topic_;
	/** The message payload - an arbitrary binary blob. */
	binary_ref payload_;
	/** The properties for the message  */
	properties props_;

	/** The client has special access. */
	friend class async_client;
	friend class message_test;

	/**
	 * Set the dup flag in the underlying message
	 * @param dup
	 */
	void set_duplicate(bool dup) { msg_.dup = to_int(dup); }

public:
	/** Smart/shared pointer to this class. */
	using ptr_t = std::shared_ptr<message>;
	/** Smart/shared pointer to this class. */
	using const_ptr_t = std::shared_ptr<const message>;

	/**
	 * Constructs a message with an empty payload, and all other values set
	 * to defaults.
	 */
	message();
	/**
	 * Constructs a message with the specified array as a payload, and all
	 * other values set to defaults.
	 * @param topic The message topic
	 * @param payload the bytes to use as the message payload
	 * @param len the number of bytes in the payload
	 * @param qos The quality of service for the message.
	 * @param retained Whether the message should be retained by the broker.
	 */
	message(string_ref topic, const void* payload, size_t len,
			int qos, bool retained);
	/**
	 * Constructs a message with the specified array as a payload, and all
	 * other values set to defaults.
	 * @param topic The message topic
	 * @param payload the bytes to use as the message payload
	 * @param len the number of bytes in the payload
	 */
	message(string_ref topic, const void* payload, size_t len)
		: message(std::move(topic), payload, len, DFLT_QOS, DFLT_RETAINED) {}
	/**
	 * Constructs a message from a byte buffer.
	 * Note that the payload accepts copy or move semantics.
	 * @param topic The message topic
	 * @param payload A byte buffer to use as the message payload.
	 * @param qos The quality of service for the message.
	 * @param retained Whether the message should be retained by the broker.
	 */
	message(string_ref topic, binary_ref payload, int qos, bool retained);
	/**
	 * Constructs a message from a byte buffer.
	 * Note that the payload accepts copy or move semantics.
	 * @param topic The message topic
	 * @param payload A byte buffer to use as the message payload.
	 */
	message(string_ref topic, binary_ref payload)
		: message(std::move(topic), std::move(payload), DFLT_QOS, DFLT_RETAINED) {}
	/**
	 * Constructs a message as a copy of the message structure.
	 * @param topic The message topic
	 * @param msg A "C" MQTTAsync_message structure.
	 */
	message(string_ref topic, const MQTTAsync_message& msg);
	/**
	 * Constructs a message as a copy of the other message.
	 * @param other The message to copy into this one.
	 */
	message(const message& other);
	/**
	 * Moves the other message to this one.
	 * @param other The message to move into this one.
	 */
	message(message&& other);
	/**
	 * Destroys a message and frees all associated resources.
	 */
	~message() {}

	/**
	 * Constructs a message with the specified array as a payload, and all
	 * other values set to defaults.
	 * @param topic The message topic
	 * @param payload the bytes to use as the message payload
	 * @param len the number of bytes in the payload
	 * @param qos The quality of service for the message.
	 * @param retained Whether the message should be retained by the broker.
	 */
	static ptr_t create(string_ref topic, const void* payload, size_t len,
						int qos, bool retained) {
		return std::make_shared<message>(std::move(topic), payload, len,
										 qos, retained);
	}
	/**
	 * Constructs a message with the specified array as a payload, and all
	 * other values set to defaults.
	 * @param topic The message topic
	 * @param payload the bytes to use as the message payload
	 * @param len the number of bytes in the payload
	 */
	static ptr_t create(string_ref topic, const void* payload, size_t len) {
		return std::make_shared<message>(std::move(topic), payload, len,
										 DFLT_QOS, DFLT_RETAINED);
	}
	/**
	 * Constructs a message from a byte buffer.
	 * Note that the payload accepts copy or move semantics.
	 * @param topic The message topic
	 * @param payload A byte buffer to use as the message payload.
	 * @param qos The quality of service for the message.
	 * @param retained Whether the message should be retained by the broker.
	 */
	static ptr_t create(string_ref topic, binary_ref payload, int qos, bool retained) {
		return std::make_shared<message>(std::move(topic), std::move(payload),
										 qos, retained);
	}
	/**
	 * Constructs a message from a byte buffer.
	 * Note that the payload accepts copy or move semantics.
	 * @param topic The message topic
	 * @param payload A byte buffer to use as the message payload.
	 */
	static ptr_t create(string_ref topic, binary_ref payload) {
		return std::make_shared<message>(std::move(topic), std::move(payload),
										 DFLT_QOS, DFLT_RETAINED);
	}
	/**
	 * Constructs a message as a copy of the C message struct.
	 * @param topic The message topic
	 * @param msg A "C" MQTTAsync_message structure.
	 */
	static ptr_t create(string_ref topic, const MQTTAsync_message& msg) {
		return std::make_shared<message>(std::move(topic), msg);
	}
	/**
	 * Copies another message to this one.
	 * @param rhs The other message.
	 * @return A reference to this message.
	 */
	message& operator=(const message& rhs);
	/**
	 * Moves another message to this one.
	 * @param rhs The other message.
	 * @return A reference to this message.
	 */
	message& operator=(message&& rhs);
	/**
	 * Sets the topic string.
	 * @param topic The topic on which the message is published.
	 */
	void set_topic(string_ref topic) {
		topic_ = topic_ ? std::move(topic) : string_ref(string());
	}
	/**
	 * Gets the topic reference for the message.
	 * @return The topic reference for the message.
	 */
	const string_ref& get_topic_ref() const { return topic_; }
	/**
	 * Gets the topic for the message.
	 * @return The topic string for the message.
	 */
	const string& get_topic() const { 
		static const string EMPTY_STR;
		return topic_ ? topic_.str() : EMPTY_STR; 
	}
	/**
	 * Clears the payload, resetting it to be empty.
	 */
	void clear_payload();
	/**
	 * Gets the payload reference.
	 */
	const binary_ref& get_payload_ref() const { return payload_; }
	/**
	 * Gets the payload
	 */
	const binary& get_payload() const {
		static const binary EMPTY_BIN;
		return payload_ ? payload_.str() : EMPTY_BIN; 
	}
	/**
	 * Gets the payload as a string
	 */
	const string& get_payload_str() const {
		static const string EMPTY_STR;
		return payload_ ? payload_.str() : EMPTY_STR;
	}
	/**
	 * Returns the quality of service for this message.
	 * @return The quality of service for this message.
	 */
	int get_qos() const { return msg_.qos; }
	/**
	 * Returns whether or not this message might be a duplicate of one which
	 * has already been received.
	 * @return true this message might be a duplicate of one which
	 * has already been received, false otherwise
	 */
	bool is_duplicate() const { return to_bool(msg_.dup); }
	/**
	 * Returns whether or not this message should be/was retained by the
	 * server.
	 * @return true if this message should be/was retained by the
	 * server, false otherwise.
	 */
	bool is_retained() const { return to_bool(msg_.retained); }
	/**
	 * Sets the payload of this message to be the specified buffer.
	 * Note that this accepts copy or move operations:
	 *   set_payload(buf);
	 *   set_payload(std::move(buf));
	 * @param payload A buffer to use as the message payload.
	 */
	void set_payload(binary_ref payload);
	/**
	 * Sets the payload of this message to be the specified byte array.
	 * @param payload the bytes to use as the message payload
	 * @param n the number of bytes in the payload
	 */
	void set_payload(const void* payload, size_t n) {
		set_payload(binary_ref(static_cast<const binary_ref::value_type*>(payload), n));
	}
	/**
	 * Sets the quality of service for this message.
	 * @param qos The integer Quality of Service for the message
	 */
	void set_qos(int qos) {
		validate_qos(qos);
		msg_.qos = qos;
	}
	/**
	 * Determines if the QOS value is a valid one.
	 * @param qos The QOS value.
	 * @throw std::invalid_argument If the qos value is invalid.
	 */
	static void validate_qos(int qos) {
		if (qos < 0 || qos > 2)
			throw exception(MQTTASYNC_BAD_QOS, "Bad QoS");
	}
	/**
	 * Whether or not the publish message should be retained by the broker.
	 * @param retained @em true if the message should be retained by the
	 *  			   broker, @em false if not.
	 */
	void set_retained(bool retained) { msg_.retained = to_int(retained); }
	/**
	 * Gets the properties in the message.
	 * @return A const reference to the properties in the message.
	 */
	const properties& get_properties() const {
		return props_;
	}
	/**
	 * Sets the properties in the message.
	 * @param props The properties to place into the message.
	 */
	void set_properties(const properties& props) {
		props_ = props;
		msg_.properties = props_.c_struct();
	}
	/**
	 * Moves the properties into the message.
	 * @param props The properties to move into the message.
	 */
	void set_properties(properties&& props) {
		props_ = props;
		msg_.properties = props_.c_struct();
	}
	/**
	 * Returns a string representation of this messages payload.
	 * @return A string representation of this messages payload.
	 */
	string to_string() const { return get_payload_str(); }
};

/** Smart/shared pointer to a message */
using message_ptr = message::ptr_t;

/** Smart/shared pointer to a const message */
using const_message_ptr = message::const_ptr_t;

/**
 * Constructs a message with the specified array as a payload, and all
 * other values set to defaults.
 * @param topic The message topic
 * @param payload the bytes to use as the message payload
 * @param len the number of bytes in the payload
 */
inline message_ptr make_message(string_ref topic, const void* payload, size_t len) {
	return mqtt::message::create(std::move(topic), payload, len);
}

/**
 * Constructs a message with the specified array as a payload, and all
 * other values set to defaults.
 * @param topic The message topic
 * @param payload the bytes to use as the message payload
 * @param len the number of bytes in the payload
 * @param qos The quality of service for the message.
 * @param retained Whether the message should be retained by the broker.
 */
inline message_ptr make_message(string_ref topic, const void* payload, size_t len,
								int qos, bool retained) {
	return mqtt::message::create(std::move(topic), payload, len, qos, retained);
}

/**
 * Constructs a message with the specified buffer as a payload, and
 * all other values set to defaults.
 * @param topic The message topic
 * @param payload A string to use as the message payload.
 */
inline message_ptr make_message(string_ref topic, binary_ref payload) {
	return mqtt::message::create(std::move(topic), std::move(payload));
}

/**
 * Constructs a message with the specified values.
 * @param topic The message topic
 * @param payload A buffer to use as the message payload.
 * @param qos The quality of service for the message.
 * @param retained Whether the message should be retained by the broker.
 */
inline message_ptr make_message(string_ref topic, binary_ref payload,
								int qos, bool retained) {
	return mqtt::message::create(std::move(topic), std::move(payload), qos, retained);
}

/////////////////////////////////////////////////////////////////////////////
// end namespace mqtt
}

#endif		// __mqtt_message_h

