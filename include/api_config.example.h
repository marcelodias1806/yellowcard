#pragma once

namespace YellowCardConfig {

struct ApiConfig {
  const char *ipOnlineBaseUrl;
  const char *tecnocorpBaseUrl;
  const char *apiToken;
};

// Copy this file to api_config.h and configure it only in your local checkout.
// Use non-production endpoints and scoped tokens during development.
constexpr ApiConfig kApiConfig = {
    "https://iponline.example.invalid",
    "https://tecnocorp.example.invalid",
    "YOUR_LOCAL_API_TOKEN",
};

}  // namespace YellowCardConfig
