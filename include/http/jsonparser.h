#ifndef _JSON_PARSER_H_
#define _JSON_PARSER_H_

#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <new>     // For placement new
#include <cstddef> // For nullptr_t

/**
 * @enum JSONType
 * @brief Enumeration of JSON value types
 *
 * Uses 2-byte storage (short) for memory efficiency
 */
enum class JSONType : short
{
    NUMBER, ///< JSON number (e.g., 123, 4.56)
    STRING, ///< JSON string (e.g., "hello")
    BOOL,   ///< JSON boolean (true/false)
    NULLT,  ///< JSON null value
    OBJECT, ///< JSON object (unordered key-value pairs)
    ARRAY   ///< JSON array (ordered list of values)
};

/**
 * @class JSONNode
 * @brief Represents a node in a JSON document tree
 *
 * Features:
 * - Type-safe value storage
 * - Recursive object/array structures
 * - Value conversion operators
 * - Memory-efficient union storage
 */
class JSONNode
{
    JSONType d_type; ///< Type of this JSON node

    /// Object storage (only used when d_type == OBJECT)
    std::unordered_map<std::string, JSONNode> d_data;

    /// Array storage (only used when d_type == ARRAY)
    std::vector<JSONNode> d_array;

    /**
     * @union value
     * @brief Union for storing primitive JSON values
     *
     * Only one member is active at a time, determined by d_type
     */
    union value
    {
        std::string d_string; ///< String value (JSONType::STRING)
        double d_number;      ///< Numeric value (JSONType::NUMBER)
        bool d_bool;          ///< Boolean value (JSONType::BOOL)

        value() {}  ///< Default constructor
        ~value() {} ///< Destructor

        /// Conversion to std::string
        operator std::string() { return d_string; }

        /// Conversion to double
        operator double() { return d_number; }

        /// Conversion to int (truncates decimal)
        operator int() { return d_number; }

        /// Conversion to bool
        operator bool() { return d_bool; }
    } d_value;

    /// @throws std::runtime_error if node is not an array
    void limitToArray() const
    {
        if (!isArray())
            throw std::runtime_error("This operation is only available to array node");
    }
    /// @throws std::runtime_error if node is not an object
    void limitToObject() const
    {
        if (!isObject())
            throw std::runtime_error("This operation is only available to object node");
    }

public:
    // Constructors
    /**
     * @brief Construct with specific JSON type
     * @param type The JSON type to create
     */
    explicit JSONNode(JSONType type) : d_type(type) {};

    ~JSONNode()
    {
        if (d_type == JSONType::STRING)
        {
            d_value.d_string.~basic_string();
        }
    }

    /// Default constructor creates null node
    JSONNode() : d_type(JSONType::NULLT) {};

    /// Construct null node (explicit nullptr overload)
    JSONNode(std::nullptr_t value) : JSONNode() {};

    /**
     * @brief Construct number node from double
     * @param value Numeric value
     */
    explicit JSONNode(double value) : d_type(JSONType::NUMBER)
    {
        d_value.d_number = value;
    }

    /**
     * @brief Construct array node
     * @param nodes Initial array elements
     */
    explicit JSONNode(const std::vector<JSONNode> &nodes) : d_type(JSONType::ARRAY), d_array(nodes) {}

    /**
     * @brief Construct number node from int
     * @param value Integer value
     */
    explicit JSONNode(int value) : d_type(JSONType::NUMBER)
    {
        d_value.d_number = value;
    }

    /**
     * @brief Construct string node
     * @param value String value
     */
    explicit JSONNode(const std::string &value) : d_type(JSONType::STRING)
    {
        new (&d_value.d_string) std::string(value);
    }

    /**
     * @brief Construct string node
     * @param value char * value
     */
    explicit JSONNode(const char *value) : d_type(JSONType::STRING)
    {
        new (&d_value.d_string) std::string(value);
    }

    /**
     * @brief Construct bool value
     * @param value bool value
     */
    explicit JSONNode(bool value) : d_type(JSONType::BOOL)
    {
        d_value.d_bool = value;
    }

    // Copy constructor
    JSONNode(const JSONNode &other) : d_type(other.d_type)
    {
        switch (d_type)
        {
        case JSONType::STRING:
            new (&d_value.d_string) std::string(other.d_value.d_string);
            break;
        case JSONType::NUMBER:
            d_value.d_number = other.d_value.d_number;
            break;
        case JSONType::BOOL:
            d_value.d_bool = other.d_value.d_bool;
            break;
        case JSONType::OBJECT:
            d_data = other.d_data;
            break;
        case JSONType::ARRAY:
            d_array = other.d_array;
            break;
        case JSONType::NULLT:
            break;
        }
    }

    // Move constructor
    JSONNode(JSONNode &&other) noexcept : d_type(other.d_type)
    {
        switch (d_type)
        {
        case JSONType::STRING:
            new (&d_value.d_string) std::string(std::move(other.d_value.d_string));
            break;
        case JSONType::NUMBER:
            d_value.d_number = other.d_value.d_number;
            break;
        case JSONType::BOOL:
            d_value.d_bool = other.d_value.d_bool;
            break;
        case JSONType::OBJECT:
            d_data = std::move(other.d_data);
            break;
        case JSONType::ARRAY:
            d_array = std::move(other.d_array);
            break;
        case JSONType::NULLT:
            break;
        }
        other.d_type = JSONType::NULLT;
    }

    // Copy assignment operator
    JSONNode &operator=(const JSONNode &node)
    {
        if (this != &node)
        {
            // Destroy existing string if present
            if (d_type == JSONType::STRING)
            {
                d_value.d_string.~basic_string();
            }

            d_type = node.d_type;
            d_data = node.d_data;
            d_array = node.d_array;

            switch (d_type)
            {
            case JSONType::STRING:
                new (&d_value.d_string) std::string(node.d_value.d_string);
                break;
            case JSONType::NUMBER:
                d_value.d_number = node.d_value.d_number;
                break;
            case JSONType::BOOL:
                d_value.d_bool = node.d_value.d_bool;
                break;
            case JSONType::NULLT:
            case JSONType::OBJECT:
            case JSONType::ARRAY:
                // No action needed
                break;
            }
        }
        return *this;
    }

    // Move assignment operator
    JSONNode &operator=(JSONNode &&node) noexcept
    {
        if (this != &node)
        {
            // Destroy existing string if present
            if (d_type == JSONType::STRING)
            {
                d_value.d_string.~basic_string();
            }

            d_type = node.d_type;
            d_data = std::move(node.d_data);
            d_array = std::move(node.d_array);

            switch (d_type)
            {
            case JSONType::STRING:
                new (&d_value.d_string) std::string(std::move(node.d_value.d_string));
                node.d_value.d_string.~basic_string();
                break;
            case JSONType::NUMBER:
                d_value.d_number = node.d_value.d_number;
                break;
            case JSONType::BOOL:
                d_value.d_bool = node.d_value.d_bool;
                break;
            case JSONType::NULLT:
            case JSONType::OBJECT:
            case JSONType::ARRAY:
                // No action needed
                break;
            }
            node.d_type = JSONType::NULLT;
        }
        return *this;
    }

    /**
     * @brief Checks if node contains a primitive JSON value
     * @return true if node is bool, number, string, or null
     * @return false if node is object or array
     *
     * @note Useful for type checking before value extraction
     */
    bool isValue() const
    {
        return (d_type == JSONType::BOOL ||
                d_type == JSONType::NUMBER ||
                d_type == JSONType::STRING ||
                d_type == JSONType::NULLT);
    }

    /**
     * @brief Checks if node is explicitly null
     * @return true if node is null
     * @return false otherwise
     */
    bool isNULL() const
    {
        return d_type == JSONType::NULLT;
    }

    /**
     * @brief Appends a node to JSON array
     * @param node Node to append
     * @throws std::runtime_error if node is not an array
     *
     * @note Only valid for ARRAY type nodes
     */
    void appendArray(const JSONNode &node)
    {
        limitToArray();
        d_array.push_back(node);
    }

    /**
     * @brief Type check for array nodes
     * @return true if node is JSON array
     */
    bool isArray() const
    {
        return d_type == JSONType::ARRAY;
    }

    /**
     * @brief Type check for object nodes
     * @return true if node is JSON object
     */
    bool isObject() const
    {
        return d_type == JSONType::OBJECT;
    }

    /**
     * @brief Template value extractor
     * @tparam T Target type (int, double, bool, std::string)
     * @return Extracted value
     * @throws std::runtime_error if node isn't a primitive value type
     *
     * @example
     * double num = node.get<double>();
     * string str = node.get<std::string>();
     */
    template <typename T>
    T get() const
    {
        if (!isValue())
            throw std::runtime_error("unable to get value for this type");

        switch (d_type)
        {
        case JSONType::STRING:
            if constexpr (std::is_same_v<T, std::string>)
            {
                return d_value.d_string;
            }
            else
            {
                throw std::runtime_error("type mismatch: requested type is not std::string");
            }
        case JSONType::NUMBER:
            if constexpr (std::is_same_v<T, double>)
            {
                return d_value.d_number;
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                return static_cast<int>(d_value.d_number);
            }
            else
            {
                throw std::runtime_error("type mismatch: requested type is not a number type");
            }
        case JSONType::BOOL:
            if constexpr (std::is_same_v<T, bool>)
            {
                return d_value.d_bool;
            }
            else
            {
                throw std::runtime_error("type mismatch: requested type is not bool");
            }
        case JSONType::NULLT:
            throw std::runtime_error("cannot get value from null node");
        default:
            throw std::runtime_error("unable to get value for this type");
        }
    }

    /**
     * @brief Array element accessor
     * @param index Array position
     * @return Reference to JSONNode at index
     * @throws std::runtime_error if node isn't an array
     * @throws std::out_of_range if index is invalid
     */
    JSONNode &operator[](int index)
    {
        limitToArray();
        return d_array[index];
    }

    const JSONNode &operator[](int index) const
    {
        limitToArray();
        return d_array[index];
    }

    /**
     * @brief Object member accessor (string key)
     * @param key Object member name
     * @return Reference to JSONNode for key
     * @throws std::runtime_error if node isn't an object
     *
     * @note Creates the key if it doesn't exist
     */
    JSONNode &operator[](const std::string &key)
    {
        limitToObject();
        return d_data[key];
    }

    /**
     * @brief Object member accessor (C-string key)
     * @param key Object member name
     * @return Reference to JSONNode for key
     * @throws std::runtime_error if node isn't an object
     *
     * @note Convenience overload for string literals
     */
    JSONNode &operator[](const char *key)
    {
        limitToObject();
        return d_data[key];
    }

    /**
     * @brief Object member accessor (string key) - const
     */
    const JSONNode &operator[](const std::string &key) const
    {
        limitToObject();
        return d_data.at(key);
    }

    /**
     * @brief Object member accessor (C-string key) - const
     */
    const JSONNode &operator[](const char *key) const
    {
        limitToObject();
        return d_data.at(key);
    }

    /* Value conversion operators */
    explicit operator std::string() const
    {
        if (d_type != JSONType::STRING)
            throw std::runtime_error("node is not a string");
        return d_value.d_string;
    }

    explicit operator int() const
    {
        if (d_type != JSONType::NUMBER)
            throw std::runtime_error("node is not a number");
        return static_cast<int>(d_value.d_number);
    }

    explicit operator bool() const
    {
        if (d_type != JSONType::BOOL)
            throw std::runtime_error("node is not a boolean");
        return d_value.d_bool;
    }

    explicit operator double() const
    {
        if (d_type != JSONType::NUMBER)
            throw std::runtime_error("node is not a number");
        return d_value.d_number;
    }

    /**
     * @brief Parses JSON string into node tree
     * @param s JSON-formatted string
     * @return Root JSONNode
     * @throws std::runtime_error on parse errors
     */
    static JSONNode parse(const std::string &s);

    /**
     * @brief Serializes JSONNode to string
     * @param node Node to serialize
     * @return JSON-formatted string
     * @throws std::runtime_error on circular references
     */
    static std::string stringify(const JSONNode &node);

    /**
     * @brief  This code defines an overloaded << operator for std::ostream, allowing a
     *  JSONNode object to be printed to an output stream (e.g., std::cout) in a pretty-
     * formatted JSON style.D
     * @param nodes Initial array elements
     */
    friend std::ostream &operator<<(std::ostream &stream, const JSONNode &json)
    {
        std::string s = JSONNode::stringify(json);
        int spaces = 0;
        int tab = 4;
        for (char c : s)
        {
            if (c == '{' || c == '[')
            {
                stream << c;
                stream << '\n';
                spaces += tab;
                stream << std::string(spaces, ' ');
            }
            else if (c == '}' || c == ']')
            {
                spaces -= tab;
                stream << '\n';
                stream << std::string(spaces, ' ');
                stream << c;
            }
            else if (c == ',')
            {
                stream << c;
                stream << '\n';
                stream << std::string(spaces, ' ');
            }
            else
            {
                stream << c;
            }
        }
        return stream;
    }
};

/**
 * @brief Parses a JSON object from a given JSON string.
 *
 * This function extracts key-value pairs from a JSON-formatted string and
 * constructs a JSONNode object. It handles nested objects and arrays using recursion.
 *
 * @param s The input JSON string.
 * @param start The index of the opening curly brace '{' for this object.
 * @param end The index of the corresponding closing curly brace '}' for this object.
 * @param bracePair A map that maps indices of opening brackets to their corresponding closing brackets.
 *
 * @return A JSONNode object representing the parsed JSON object.
 */
JSONNode parseObject(const std::string &s, int start, int end, std::unordered_map<int, int> &bracePair);

/**
 * @brief Parses a JSON array from a given JSON string.
 *
 * This function extracts values from a JSON-formatted string and constructs
 * a JSONNode object representing an array. It processes elements sequentially
 * and handles whitespace correctly.
 *
 * @param s The input JSON string.
 * @param start The index of the opening square bracket '[' for this array.
 * @param end The index of the corresponding closing square bracket ']' for this array.
 * @param bracePair A map that maps indices of opening brackets to their corresponding closing brackets.
 *
 * @return A JSONNode object representing the parsed JSON array.
 */
JSONNode parseArray(const std::string &s, int start, int end, std::unordered_map<int, int> &bracePair);

using JSON = JSONNode;

#endif