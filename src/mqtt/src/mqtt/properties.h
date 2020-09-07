/////////////////////////////////////////////////////////////////////////////
/// @file properties.h
/// Declaration of MQTT properties class
/// @date July 7, 2019
/// @author Frank Pagliughi
/////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * Copyright (c) 2019 Frank Pagliughi <fpagliughi@mindspring.com>
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

#ifndef __mqtt_properties_h
#define __mqtt_properties_h

extern "C" {
	#include "MQTTProperties.h"
}

#include "mqtt/types.h"
#include "mqtt/buffer_ref.h"
#include "mqtt/exception.h"
#include <tuple>
#include <initializer_list>

#include <iostream>

namespace mqtt {

using string_pair = std::tuple<string, string>;

/////////////////////////////////////////////////////////////////////////////

/**
 * A single MQTT v5 property.
 */
class property
{
	/** The underlying Paho C property struct. */
	MQTTProperty prop_;

	// Make a deep copy of the property struct into this one.
	// For string properties, this allocates memory and copied the string(s)
	void copy(const MQTTProperty& other);

public:
	enum code {
        PAYLOAD_FORMAT_INDICATOR = 1,
        MESSAGE_EXPIRY_INTERVAL = 2,
        CONTENT_TYPE = 3,
        RESPONSE_TOPIC = 8,
        CORRELATION_DATA = 9,
        SUBSCRIPTION_IDENTIFIER = 11,
        SESSION_EXPIRY_INTERVAL = 17,
        ASSIGNED_CLIENT_IDENTIFER = 18,
        SERVER_KEEP_ALIVE = 19,
        AUTHENTICATION_METHOD = 21,
        AUTHENTICATION_DATA = 22,
        REQUEST_PROBLEM_INFORMATION = 23,
        WILL_DELAY_INTERVAL = 24,
        REQUEST_RESPONSE_INFORMATION = 25,
        RESPONSE_INFORMATION = 26,
        SERVER_REFERENCE = 28,
        REASON_STRING = 31,
        RECEIVE_MAXIMUM = 33,
        TOPIC_ALIAS_MAXIMUM = 34,
        TOPIC_ALIAS = 35,
        MAXIMUM_QOS = 36,
        RETAIN_AVAILABLE = 37,
        USER_PROPERTY = 38,
        MAXIMUM_PACKET_SIZE = 39,
        WILDCARD_SUBSCRIPTION_AVAILABLE = 40,
        SUBSCRIPTION_IDENTIFIERS_AVAILABLE = 41,
        SHARED_SUBSCRIPTION_AVAILABLE = 42
	};

	/**
	 * Create a numeric property.
	 * This can be a byte, or 2-byte, 4-byte, or variable byte integer.
	 * @param c The property code
	 * @param val The integer value for the property
	 */
	property(code c, int32_t val);
	/**
	 * Create a string or binary property.
	 * @param c The property code
	 * @param val The value for the property
	 */
	property(code c, string_ref val);
	/**
	 * Create a string pair property.
	 * @param c The property code
	 * @param name The string name for the property
	 * @param val The string value for the property
	 */
	property(code c, string_ref name, string_ref val);
	/**
	 * Creates a property list from an C struct.
	 * @param cprop A C struct for a property list.
	 */
	explicit property(const MQTTProperty& cprop);
	/**
	 * Moves a C struct into this property list.
	 * This takes ownership of any memory that the C struct is holding.
	 * @param cprop A C struct for a property list.
	 */
	explicit property(MQTTProperty&& cprop);
	/**
	 * Copy constructor
	 * @param other The other property to copy into this one.
	 */
	property(const property& other);
	/**
	 * Move constructor.
	 * @param other The other property that is moved into this one.
	 */
	property(property&& other);
	/**
	 * Destructor
	 */
	~property();
	/**
	 * Copy assignment.
	 * @param rhs Another property list to copy into this one.
	 * @return A reference to this object.
	 */
	property& operator=(const property& rhs);
	/**
	 * Move assignment.
	 * @param rhs Another property list to move into this one.
	 * @return A reference to this object.
	 */
	property& operator=(property&& rhs);
	/**
	 * Gets the underlying C property struct.
	 * @return A const reference to the underlying C property
	 *  	   struct.
	 */
	const MQTTProperty& c_struct() const { return prop_; }
	/**
	 * Gets the property type (identifier).
	 * @return The code for the property type.
	 */
	code type() const { return code(prop_.identifier); }
	/**
	 * Gets a printable name for the property type.
	 * @return A printable name for the property type.
	 */
	const char* type_name() const {
		return ::MQTTPropertyName(prop_.identifier);
	}
};

/**
 * Extracts the value from the property as the specitied type.
 * @return The value from the property as the specitied type.
 */
template <typename T>
inline T get(const property&) { throw bad_cast(); }

template <>
inline uint8_t get<uint8_t>(const property& prop) {
	return (uint8_t) prop.c_struct().value.byte;
}

template <>
inline uint16_t get<uint16_t>(const property& prop) {
	return (uint16_t) prop.c_struct().value.integer2;
}

template <>
inline int16_t get<int16_t>(const property& prop) {
	return (int16_t) prop.c_struct().value.integer2;
}

template <>
inline uint32_t get<uint32_t>(const property& prop) {
	return (uint32_t) prop.c_struct().value.integer4;
}

template <>
inline int32_t get<int32_t>(const property& prop) {
	return (int32_t) prop.c_struct().value.integer4;
}

template <>
inline string get<string>(const property& prop) {
	return (!prop.c_struct().value.data.data) ? string()
		: string(prop.c_struct().value.data.data, prop.c_struct().value.data.len);
}

template <>
inline string_pair get<string_pair>(const property& prop) {
	string name = (!prop.c_struct().value.data.data) ? string()
		: string(prop.c_struct().value.data.data, prop.c_struct().value.data.len);

	string value = (!prop.c_struct().value.value.data) ? string()
		: string(prop.c_struct().value.value.data, prop.c_struct().value.value.len);

	return std::make_tuple(std::move(name), std::move(value));
}

/////////////////////////////////////////////////////////////////////////////

/**
 * MQTT v5 property list.
 *
 * A collection of properties that can be added to outgoing packets or
 * retrieved from incoming packets.
 */
class properties
{
	/** The underlying C properties struct  */
	MQTTProperties props_;

	template<typename T>
	friend T get(const properties& props, property::code propid, size_t idx);

	template<typename T>
	friend T get(const properties& props, property::code propid);

public:
	/**
	 * Default constructor.
	 * Creates an empty properties list.
	 */
	properties() {
		std::memset(&props_, 0, sizeof(MQTTProperties));
	}
	/**
	 * Copy constructor.
	 * @param other The property list to copy.
	 */
	properties(const properties& other)
			: props_(::MQTTProperties_copy(&other.props_)) {}
	/**
	 * Move constructor.
	 * @param other The property list to move to this one.
	 */
	properties(properties&& other) : props_(other.props_) {
		std::memset(&other.props_, 0, sizeof(MQTTProperties));
	}
	/**
	 * Creates a list of properties from a C struct.
	 * @param cprops The c struct of properties
	 */
	properties(const MQTTProperties& cprops) {
		props_ = ::MQTTProperties_copy(&cprops);
	}
	/**
	 * Constructs from a list of property objects.
	 * @param An initializer list of property objects.
	 */
	properties(std::initializer_list<property> props);
	/**
	 * Destructor.
	 */
	~properties() { ::MQTTProperties_free(&props_); }
	/**
	 * Gets a reference to the underlying C properties structure.
	 * @return A const reference to the underlying C properties structure.
	 */
	const MQTTProperties& c_struct() const { return props_; }
	/**
	 * Copy assignment.
	 * @param rhs The other property list to copy into this one
	 * @return A reference to this object.
	 */
	properties& operator=(const properties& rhs) {
		if (&rhs != this) {
			::MQTTProperties_free(&props_);
			props_ = ::MQTTProperties_copy(&rhs.props_);
		}
		return *this;
	}
	/**
	 * Move assignment
	 * @param rht The property list to move to this one.
	 * @return A reference to this object.
	 */
	properties& operator=(properties&& rhs) {
		if (&rhs != this) {
			::MQTTProperties_free(&props_);
			props_ = rhs.props_;
			std::memset(&rhs.props_, 0, sizeof(MQTTProperties));
		}
		return *this;
	}
	/**
	 * Determines if the property list is empty.
	 * @return @em true if there are no properties in the list, @em false if
	 *  	   the list contains any items.
	 */
	bool empty() const { return props_.count == 0; }
	/**
	 * Gets the numbers of property items in the list.
	 * @return The number of property items in the list.
	 */
	size_t size() const { return size_t(props_.count); }
	/**
	 * Gets the number of bytes required for the serialized
	 * structure on the wire.
	 * @return The number of bytes required for the serialized
	 *  	   struct.
	 */
	size_t byte_length() const {
		return (size_t) ::MQTTProperties_len(const_cast<MQTTProperties*>(&props_));
	}
	/**
	 * Adds a property to the list.
	 * @param prop The property to add to the list.
	 */
	void add(const property& prop) {
		::MQTTProperties_add(&props_, &prop.c_struct());
	}
	/**
	 * Removes all the items from the property list.
	 */
	void clear();
	/**
	 * Determines if the list contains a specific property.
	 * @param propid The property ID (code).
	 * @return @em true if the list contains the property, @em false if not.
	 */
	bool contains(property::code propid) const {
		return ::MQTTProperties_hasProperty(const_cast<MQTTProperties*>(&props_),
											MQTTPropertyCodes(propid)) != 0;
	}
	/**
	 * Get the number of properties in the list with the specified property
	 * ID.
	 *
	 * Most properties can exist only once. User properties and subscription
	 * ID's can exist more than once.
	 *
	 * @param propid The property ID (code).
	 * @return The number of properties in the list with the specified ID.
	 */
	size_t count(property::code propid) {
		return size_t(::MQTTProperties_propertyCount(&props_, MQTTPropertyCodes(propid)));
	}
	/**
	 * Gets the property with the specified ID.
	 *
	 * @param propid The property ID (code).
	 * @param idx Which instance of the property to retrieve, if there are
	 *  		  more than one.
	 * @return The requested property
	 */
	property get(property::code propid, size_t idx=0);
};

// --------------------------------------------------------------------------

template<typename T>
inline T get(const properties& props, property::code propid, size_t idx)
{
	MQTTProperty* prop = MQTTProperties_getPropertyAt(
								const_cast<MQTTProperties*>(&props.c_struct()),
								MQTTPropertyCodes(propid), int(idx));
	if (!prop)
		throw bad_cast();

	return get<T>(property(*prop));
}

template<typename T>
inline T get(const properties& props, property::code propid)
{
	return get<T>(props, propid, 0);
}

/////////////////////////////////////////////////////////////////////////////
// end namespace mqtt
}

#endif		// __mqtt_properties_h

