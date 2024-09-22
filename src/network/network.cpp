#include "pch.h"
#include "network.h"

namespace SOEP {

	struct NetworkData {
		Network* Instance = nullptr;
	};

	NetworkData* s_Data = nullptr;

	void Network::Init()
	{
		s_Data = new NetworkData();
		s_Data->Instance = new Network();
	}

	void Network::Shutdown()
	{
		delete s_Data->Instance;
		delete s_Data;
	}

	std::shared_ptr<std::string> Network::Call(const std::string& url, const std::function<void(std::shared_ptr<std::string>& result)> custom_callback, const CurlOptionMap* options, const HeaderList* headers)
	{
		SOEP_ASSERT(s_Data && s_Data->Instance, "No network instance found, have you forgot to run Init() before making any calls?");
		CURL* curl_instance = curl_easy_init();
		SOEP_ASSERT(curl_instance, "Curl couldn't initialize!");
		std::shared_ptr<std::string> result = std::make_shared<std::string>();

		curl_easy_setopt(curl_instance, CURLOPT_URL, url.c_str());


		ApplyCurlOptions(curl_instance, options);

		struct curl_slist* curl_headers = nullptr;
		if (headers) {
			for (const auto& header : *headers) {
				std::string full_header = header.first + ": " + header.second;
				curl_headers = curl_slist_append(curl_headers, full_header.c_str());
			}
		}

		curl_easy_setopt(curl_instance, CURLOPT_HTTPHEADER, curl_headers);
		curl_easy_setopt(curl_instance, CURLOPT_WRITEFUNCTION, Callback);
		curl_easy_setopt(curl_instance, CURLOPT_WRITEDATA, result.get());

		CURLcode res = curl_easy_perform(curl_instance);
		if (res != CURLE_OK) {
			spdlog::error("Curl request failed: {}", curl_easy_strerror(res));
			return nullptr;
		}

		curl_easy_cleanup(curl_instance);
		if (curl_headers) {
			curl_slist_free_all(curl_headers);
		}

		if (custom_callback) {
			custom_callback(result);
		}

		return result;
	}

	void Network::ApplyCurlOptions(CURL* curl_instance, const CurlOptionMap* options)
	{
		if (!options) {
			return;
		}
		for (auto it = options->begin(); it != options->end(); it++) {
			switch (it->first) {
			case CURLOPT_TIMEOUT:
			case CURLOPT_VERBOSE:
			case CURLOPT_FOLLOWLOCATION:
				curl_easy_setopt(curl_instance, it->first, std::stol(it->second));
				break;
			case CURLOPT_WRITEFUNCTION: // Ignore as we already set the Callback function
			case CURLOPT_URL:  // Same principle
				break;
			default:
				curl_easy_setopt(curl_instance, it->first, it->second.c_str());
				break;
			}
		}
	}

	Network::Network()
	{

	}

	Network::~Network()
	{

	}
	size_t Network::Callback(void* contents, size_t size, size_t nmemb, void* userp)
	{
		std::string* result = static_cast<std::string*>(userp);
		size_t totalSize = size * nmemb;
		result->append(static_cast<char*>(contents), totalSize);
		return totalSize;
	}
}
