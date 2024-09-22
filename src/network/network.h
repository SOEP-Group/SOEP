#pragma once
#include <curl/curl.h>

namespace SOEP {


	class Network {
	public:

		static void Init();
		static void Shutdown();

		using CurlOptionMap = std::map<CURLoption, std::string>;
		using HeaderList = std::vector<std::pair<std::string, std::string>>;

		static std::shared_ptr<std::string> Call(const std::string& url, const std::function<void(std::shared_ptr<std::string>& result)> custom_callback, const CurlOptionMap* options, const HeaderList* headers);

	private:
		Network();
		~Network();

		static size_t Callback(void* contents, size_t size, size_t nmemb, void* userp);
		static void ApplyCurlOptions(CURL* curl_instance, const CurlOptionMap* options);

	private:
	};
}