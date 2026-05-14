#include "headers.h"

#include <ranges>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <stdexcept>
#include <string>

using namespace std::string_view_literals;

namespace {
  std::string toLower(std::string_view str)
  {
    std::string result;
    result.reserve(str.size());

    for(char ch : str)
    {
      result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return result;
  }

  std::string trimToString(std::string_view str)
  {
    while(!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
    {
      str.remove_prefix(1);
    }

    while(!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))
    {
      str.remove_suffix(1);
    }

    return std::string(str);
  }
}


void iterHeaders(std::string_view req, Callback&& callback) 
{
    constexpr std::string_view line_delimiter = "\r\n";

    std::size_t pos = 0;

    std::size_t line_end = req.find(line_delimiter, pos);

    if (line_end == std::string_view::npos) {
        return;
    }

    pos = line_end + line_delimiter.size();

    while (pos < req.size()) 
    {
        line_end = req.find(line_delimiter, pos);

        if (line_end == std::string_view::npos) 
        {
            break;
        }

        std::string_view line = req.substr(pos, line_end - pos);

        if (line.empty()) 
        {
            break;
        }

        std::size_t colon_pos = line.find(':');

        if (colon_pos != std::string_view::npos) 
        {
            std::string_view name = line.substr(0, colon_pos);
            std::string_view value = line.substr(colon_pos + 1);

            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) 
            {
                value.remove_prefix(1);
            }

            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) 
            {
                value.remove_suffix(1);
            }

            callback(name, value);
        }

        pos = line_end + line_delimiter.size();
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) 
{
    std::string host;
    std::string port = "80";

    iterHeaders(req, [&](std::string_view name, std::string_view value) 
    {
        if(toLower(name) != "host")
        {
          return;
        }

        std::string host_value = trimToString(value);

        std::size_t colon_pos = host_value.find(':');
        if(colon_pos == std::string::npos)
        {
          host = host_value;
          port = "80";
        }
        else
        {
          host = host_value.substr(0, colon_pos);
          port = host_value.substr(colon_pos + 1);
        }
    });

    if(host.empty())
    {
      throw std::runtime_error{"HTP request does not contain Host header"};
    }

    return{host, port};
}

std::optional<size_t> findContentLength(std::string_view rsp) 
{
    std::optional<size_t> result;

    iterHeaders(rsp, [&](std::string_view name, std::string_view value) 
    {
        if (toLower(name) != "content-length") 
        {
            return;
        }

        value = std::string_view(value.data(), value.size());

        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) 
        {
            value.remove_prefix(1);
        }

        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) 
        {
            value.remove_suffix(1);
        }

        size_t number = 0;

        auto [ptr, ec] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            number
        );

        if (ec == std::errc{}) 
        {
            result = number;
        }
    });

    return result;
}
