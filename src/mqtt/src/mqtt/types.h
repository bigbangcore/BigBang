/////////////////////////////////////////////////////////////////////////////
/// @file types.h
/// Basic types and type conversions for the Paho MQTT C++ library.
/// @date May 17, 2015 @author Frank Pagliughi
/////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * Copyright (c) 2015-2017 Frank Pagliughi <fpagliughi@mindspring.com>
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

#ifndef __mqtt_types_h
#define __mqtt_types_h

#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace mqtt {

/////////////////////////////////////////////////////////////////////////////
// Basic data types

/** A 'byte' is an 8-bit, unsigned int */
using byte = uint8_t;

/** An mqtt string is just a std::string */
using string = std::string;
/** A binary blob of data is, umm, just a string too! */
using binary = std::string;		//std::basic_string<byte>;

/** Smart/shared pointer to a const string */
using string_ptr = std::shared_ptr<const string>;
/** Smart/shared pointer to a const binary blob */
using binary_ptr = std::shared_ptr<const binary>;

/////////////////////////////////////////////////////////////////////////////
// General protocol enumerations

/**
 * The MQTT v5 Reason Codes.
 *
 * These map to the Paho C MQTTReasonCodes
 */
enum ReasonCode {
    SUCCESS = 0,
    NORMAL_DISCONNECTION = 0,
    GRANTED_QOS_0 = 0,
    GRANTED_QOS_1 = 1,
    GRANTED_QOS_2 = 2,
    DISCONNECT_WITH_WILL_MESSAGE = 4,
    NO_MATCHING_SUBSCRIBERS = 16,
    NO_SUBSCRIPTION_FOUND = 17,
    CONTINUE_AUTHENTICATION = 24,
    RE_AUTHENTICATE = 25,
    UNSPECIFIED_ERROR = 128,
    MALFORMED_PACKET = 129,
    PROTOCOL_ERROR = 130,
    IMPLEMENTATION_SPECIFIC_ERROR = 131,
    UNSUPPORTED_PROTOCOL_VERSION = 132,
    CLIENT_IDENTIFIER_NOT_VALID = 133,
    BAD_USER_NAME_OR_PASSWORD = 134,
    NOT_AUTHORIZED = 135,
    SERVER_UNAVAILABLE = 136,
    SERVER_BUSY = 137,
    BANNED = 138,
    SERVER_SHUTTING_DOWN = 139,
    BAD_AUTHENTICATION_METHOD = 140,
    KEEP_ALIVE_TIMEOUT = 141,
    SESSION_TAKEN_OVER = 142,
    TOPIC_FILTER_INVALID = 143,
    TOPIC_NAME_INVALID = 144,
    PACKET_IDENTIFIER_IN_USE = 145,
    PACKET_IDENTIFIER_NOT_FOUND = 146,
    RECEIVE_MAXIMUM_EXCEEDED = 147,
    TOPIC_ALIAS_INVALID = 148,
    PACKET_TOO_LARGE = 149,
    MESSAGE_RATE_TOO_HIGH = 150,
    QUOTA_EXCEEDED = 151,
    ADMINISTRATIVE_ACTION = 152,
    PAYLOAD_FORMAT_INVALID = 153,
    RETAIN_NOT_SUPPORTED = 154,
    QOS_NOT_SUPPORTED = 155,
    USE_ANOTHER_SERVER = 156,
    SERVER_MOVED = 157,
    SHARED_SUBSCRIPTIONS_NOT_SUPPORTED = 158,
    CONNECTION_RATE_EXCEEDED = 159,
    MAXIMUM_CONNECT_TIME = 160,
    SUBSCRIPTION_IDENTIFIERS_NOT_SUPPORTED = 161,
    WILDCARD_SUBSCRIPTIONS_NOT_SUPPORTED = 162,
	MQTTPP_V3_CODE = 255	// This is not a protocol code; used internally by the library
};

/////////////////////////////////////////////////////////////////////////////
// Time functions

/**
 * Convert a chrono duration to seconds.
 * This casts away precision to get integer seconds.
 * @param dur A chrono duration type
 * @return The duration as a chrono seconds value
 */
template <class Rep, class Period>
std::chrono::seconds to_seconds(const std::chrono::duration<Rep, Period>& dur) {
	return std::chrono::duration_cast<std::chrono::seconds>(dur);
}

/**
 * Convert a chrono duration to a number of seconds.
 * This casts away precision to get integer seconds.
 * @param dur A chrono duration type
 * @return The duration as a number of seconds
 */
template <class Rep, class Period>
long to_seconds_count(const std::chrono::duration<Rep, Period>& dur) {
	return (long) to_seconds(dur).count();
}

/**
 * Convert a chrono duration to milliseconds.
 * This casts away precision to get integer milliseconds.
 * @param dur A chrono duration type
 * @return The duration as a chrono milliseconds value
 */
template <class Rep, class Period>
std::chrono::milliseconds to_milliseconds(const std::chrono::duration<Rep, Period>& dur) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(dur);
}

/**
 * Convert a chrono duration to a number of milliseconds.
 * This casts away precision to get integer milliseconds.
 * @param dur A chrono duration type
 * @return The duration as a number of milliseconds
 */
template <class Rep, class Period>
long to_milliseconds_count(const std::chrono::duration<Rep, Period>& dur) {
	return (long) to_milliseconds(dur).count();
}

/////////////////////////////////////////////////////////////////////////////
// Misc

/**
 * Converts an into to a bool.
 * @param n An integer.
 * @return @em true if n not equal to zero, @em false otherwise
 */
inline bool to_bool(int n) { return n != 0; }
/**
 * Converts the boolean into a C integer true/false value.
 * @param b A boolean
 * @return Zero if b is false, non-zero if b is true.
 */
inline int to_int(bool b) { return b ? (!0) : 0; }

/**
 * Gets a valid string for the char pointer.
 * @param cstr A C-string pointer
 * @return A string copy of the C array. If `cstr` is NULL, this returns an
 *  	   empty string.
 */
inline string to_string(const char* cstr) {
	return cstr ? string(cstr) : string();
}

/////////////////////////////////////////////////////////////////////////////
// end namespace mqtt
}

#endif		// __mqtt_types_h

