#include <gtest/gtest.h>
#include "headers.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

TEST(iterHeaders, Empty) 
{
  std::vector<std::pair<std::string, std::string>> headers;

  iterHeaders("", [&](std::string_view name, std::string_view value) {
      headers.emplace_back(std::string(name), std::string(value));
  });

  EXPECT_TRUE(headers.empty());
}

TEST(iterHeaders, SkipRequestLine) 
{
  std::vector<std::pair<std::string, std::string>> headers;

  std::string request =
      "GET http://example.com/ HTTP/1.1\r\n"
      "\r\n";

  iterHeaders(request, [&](std::string_view name, std::string_view value) {
      headers.emplace_back(std::string(name), std::string(value));
  });

  EXPECT_TRUE(headers.empty());
}

TEST(iterHeaders, SingleHeader) 
{
  std::vector<std::pair<std::string, std::string>> headers;

  std::string request =
      "GET http://example.com/ HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "\r\n";

  iterHeaders(request, [&](std::string_view name, std::string_view value) {
      headers.emplace_back(std::string(name), std::string(value));
  });

  ASSERT_EQ(headers.size(), 1);
  EXPECT_EQ(headers[0].first, "Host");
  EXPECT_EQ(headers[0].second, "example.com");
}

TEST(iterHeaders, MultipleHeaders) 
{
  std::vector<std::pair<std::string, std::string>> headers;

  std::string request =
      "GET http://example.com/ HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "User-Agent: TestClient\r\n"
      "\r\n";

  iterHeaders(request, [&](std::string_view name, std::string_view value) {
      headers.emplace_back(std::string(name), std::string(value));
  });

  ASSERT_EQ(headers.size(), 2);

  EXPECT_EQ(headers[0].first, "Host");
  EXPECT_EQ(headers[0].second, "example.com");

  EXPECT_EQ(headers[1].first, "User-Agent");
  EXPECT_EQ(headers[1].second, "TestClient");
}

TEST(iterHeaders, MultipleSameHeaders) 
{
  std::vector<std::pair<std::string, std::string>> headers;

  std::string request =
    "GET http://example.com/ HTTP/1.1\r\n"
    "Cookie: a=1\r\n"
    "Cookie: b=2\r\n"
    "\r\n";

  iterHeaders(request, [&](std::string_view name, std::string_view value) {
      headers.emplace_back(std::string(name), std::string(value));
  });

  ASSERT_EQ(headers.size(), 2);

  EXPECT_EQ(headers[0].first, "Cookie");
  EXPECT_EQ(headers[0].second, "a=1");

  EXPECT_EQ(headers[1].first, "Cookie");
  EXPECT_EQ(headers[1].second, "b=2");
}

TEST(findHostPort, Simple) 
{
  std::string request =
    "GET http://example.com/ HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "\r\n";

  auto [host, port] = findHostPort(request);

  EXPECT_EQ(host, "example.com");
  EXPECT_EQ(port, "80");
}

TEST(findHostPort, NoHost) 
{
  std::string request =
    "GET http://example.com/ HTTP/1.1\r\n"
    "User-Agent: TestClient\r\n"
    "\r\n";

  EXPECT_THROW(findHostPort(request), std::runtime_error);
}

TEST(findContentLength, Simple) 
{
  std::string response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 123\r\n"
    "\r\n";

  auto content_length = findContentLength(response);

  ASSERT_TRUE(content_length.has_value());
  EXPECT_EQ(*content_length, 123);
}

TEST(findContentLength, NoContentLength) 
{
  std::string response =
    "HTTP/1.1 200 OK\r\n"
    "Server: nginx\r\n"
    "\r\n";

  auto content_length = findContentLength(response);

  EXPECT_FALSE(content_length.has_value());
}
