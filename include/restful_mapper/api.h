#ifndef RESTFUL_MAPPER_API_H
#define RESTFUL_MAPPER_API_H

#include <stdexcept>
#include <string>
#include <algorithm>
#include <map>
#include <cctype>
#include <restful_mapper/json.h>

// Struct used for sending data
typedef struct
{
  const char *data;
  size_t length;
} RequestBody;

namespace restful_mapper
{

enum RequestType { GET, POST, PUT, DEL };

/**
 * Lazy evaluated singleton class holding global API configuration.
 */
class Api
{
public:
  Api();
  Api(const Api &) = default;            // Don't Implement
  Api& operator=(const Api &) = default; // Don't implement

  ~Api();

  std::string get(const std::string &endpoint) 
  {
    return get_(endpoint);
  }

  std::string post(const std::string &endpoint, const std::string &body) 
  {
    return post_(endpoint, body);
  }

  std::string put(const std::string &endpoint, const std::string &body) 
  {
    return put_(endpoint, body);
  }

  std::string del(const std::string &endpoint) 
  {
    return del_(endpoint);
  }

  std::string escape(const std::string &value) const
  {
    return escape_(value);
  }

  std::string query_param(const std::string &url, const std::string &param, const std::string &value) const
  {
    return query_param_(url, param, value);
  }

  std::string url()  
  {
    return url_;
  }

  std::string url(const std::string &endpoint) const
  {
    return url_ + endpoint;
  }

  std::string set_url(const std::string &url)
  {
    return url_ = url;
  }

  std::string proxy()
  {
    return proxy_;
  }

  std::string set_proxy(const std::string &proxy)
  {
    return proxy_ = proxy;
  }

  void clear_proxy()
  {
    read_environment_proxy();
  }

  std::string username() const
  {
    return username_;
  }

  std::string set_username(const std::string &username)
  {
    return username_ = username;
  }

  std::string password() const
  {
    return password_;
  }

  std::string set_password(const std::string &password)
  {
    return password_ = password;
  }

private:
  std::string url_;
  std::string proxy_;
  std::string username_;
  std::string password_;
  static const char *user_agent_;
  static const char *content_type_;
  void *curl_handle_;
  std::string responseBody_;
  RequestBody requestBody_;
  // Dont forget to declare these two. You want to make sure they
  // are unaccessable otherwise you may accidently get copies of
  // your singleton appearing.
 
  /**
   * Return the singleton instance
   */
  //static Api &instance()
  //{
  //    static Api instance;  // Guaranteed to be destroyed, instantiated on first use

  //    return instance;
  //}

  // Curl read send request function
  std::string send_request(const RequestType &type, const std::string &endpoint, const std::string &body);

  //Curl write_callback wrapper
  static size_t write_callback_wrapper(void *ptr, size_t size, size_t nmemb, void* api);

  // Curl write callback function
  size_t write_callback(void *ptr, size_t size, size_t nmemb);//, void *userdata);
 
  //Curl read_callback wrapper
  static size_t read_callback_wrapper(void *data, size_t size, size_t nmemb, void* api);

  // Curl read callback function
  size_t read_callback(void *ptr, size_t size, size_t nmemb);//, void *userdata);

  // Check whether an error occurred
  void check_http_error(const RequestType &type, const std::string &endpoint, long &http_code, const std::string &response_body) const;

  // Get environment proxy into proxy_
  void read_environment_proxy();

  // Request methods
  std::string get_(const std::string &endpoint);
  std::string post_(const std::string &endpoint, const std::string &body);
  std::string put_(const std::string &endpoint, const std::string &body);
  std::string del_(const std::string &endpoint);

  // String methods
  std::string escape_(const std::string &value) const;
  std::string query_param_(const std::string &url, const std::string &param, const std::string &value) const;
};

class ApiError : public std::runtime_error
{
public:
  explicit ApiError(const std::string &what, const int &code)
    : std::runtime_error(what), code_(code) {}
  virtual ~ApiError() throw() {}

  virtual const int &code() const
  {
    return code_;
  }

private:
  int code_;
};

class AuthenticationError : public ApiError
{
public:
  explicit AuthenticationError()
    : ApiError("Unable to authenticate using the specified credentials", 401) {}
  virtual ~AuthenticationError() throw() {}
};

class ResponseError : public ApiError
{
public:
  explicit ResponseError(const std::string &what, const int &code, const std::string &details)
    : ApiError(what, code), details_(details) {}
  virtual ~ResponseError() throw() {}

  virtual const char *details() const
  {
    return details_.c_str();
  }

private:
  std::string details_;
};

class BadRequestError : public ApiError
{
public:
  explicit BadRequestError(const std::string &json_struct) : ApiError("", 400)
  {
    try
    {
      Json::Parser parser(json_struct);
      what_ = parser.find("message").to_string();
    }
    catch (std::runtime_error &e)
    {
      // Swallow
    }
  }
  virtual ~BadRequestError() throw() {}

  virtual const char *what() const throw()
  {
    return what_.c_str();
  }

private:
  std::string what_;
};

class ValidationError : public ApiError
{
public:
  typedef std::map<std::string, std::string> FieldMap;

  explicit ValidationError(const std::string &json_struct) : ApiError("", 400)
  {
    try
    {
      std::string separator = "";
      Json::Parser parser(json_struct);

      std::map<std::string, Json::Node> errors;
      errors = parser.find("validation_errors").to_map();

      std::map<std::string, Json::Node>::const_iterator i, i_end = errors.end();
      for (i = errors.begin(); i != i_end; ++i)
      {
        std::string field_name  = i->first;

        errors_[field_name] = parse_errors(i->second, 1, false);

        what_ += separator;
        what_ += format_field(field_name);
        what_ += parse_errors(i->second);
        separator = "\n";
      }
    }
    catch (std::runtime_error &e)
    {
      // Swallow
    }
  }

  virtual ~ValidationError() throw() {}

  const FieldMap &errors() const
  {
    return errors_;
  }

  virtual const char *what() const throw()
  {
    return what_.c_str();
  }

  std::string operator[](const std::string &field) const
  {
    FieldMap::const_iterator i = errors_.find(field);

    if (i != errors_.end())
    {
      return i->second;
    }

    return "";
  }

private:
  FieldMap errors_;
  std::string what_;

  std::string parse_errors(const Json::Node &node, const unsigned int &indent = 1, const bool &add_space = true)
  {
    if (node.is_array())
    {
      std::string formatted;

      std::vector<Json::Node> errors = node.to_array();

      std::vector<Json::Node>::const_iterator i, i_end = errors.end();
      for (i = errors.begin(); i != i_end; ++i)
      {
        formatted += parse_errors(*i, indent);
      }

      return formatted;
    }
    else if (node.is_map())
    {
      std::string formatted;

      std::map<std::string, Json::Node> errors = node.to_map();

      std::map<std::string, Json::Node>::const_iterator i, i_end = errors.end();
      for (i = errors.begin(); i != i_end; ++i)
      {
        std::string field_name  = i->first;
        std::string field_value = parse_errors(i->second, indent + 1);

        formatted += "\n";
        for (unsigned int j = 0; j < indent; j++)
        {
          formatted += "  ";
        }
        formatted += format_field(field_name);
        formatted += field_value;
      }

      return formatted;
    }
    else if (node.is_string())
    {
      std::string formatted;

      if (add_space)
      {
        formatted += " ";
      }

      formatted += node.to_string();

      return formatted;
    }

    return "";
  }

  std::string format_field(const std::string &field)
  {
    std::string formatted(field);
    std::replace(formatted.begin(), formatted.end(), '_', ' ');
    formatted[0] = std::toupper(formatted[0]);
    return formatted;
  }
};

}

#endif // RESTFUL_MAPPER_API_H

