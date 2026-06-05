#include <gtest/gtest.h>

#include <App.h>

TEST(UWebSocketsVendorTests, VendoredHeadersAndRuntimeConstruct) {
  uWS::App App = uWS::App();
  App.get("/health", [](auto *Response, auto *Request) {
    (void)Request;
    Response->end("ok");
  });
  SUCCEED();
}
