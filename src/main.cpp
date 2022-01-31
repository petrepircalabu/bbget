#include "certs.hpp"
#include "connection.hpp"
#include "downloader.hpp"
#include "proxy.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/program_options.hpp>
#include <cppcodec/base64_rfc4648.hpp>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

int main(int argc, char* argv[])
{
	std::vector<std::string> urls;
	bbget::proxy::config     proxy_config;
	spdlog::set_pattern("%v");

	try {
		std::string_view const title
		    = "BBGet v0.1.0 - a Boost Beast based non-interactive network retriever\n"
		      "Usage: bbget [OPTION]... [URL]...";
		namespace po = boost::program_options;

		po::options_description general("General options");
		general.add_options()("help", "produce a help message")("version",
		                                                        "output the version number");

		po::options_description log_and_config("Logging and Configuration");
		general.add_options()("debug,d", "output the version number");

		po::options_description proxy("Proxy");
		proxy.add_options()("proxy,p", po::value<std::string>(), "Use proxy for transfer");
		proxy.add_options()("proxy-auth", po::value<std::string>(), "Proxy authentication");

		po::options_description all("");
		all.add(general);
		all.add(log_and_config);
		all.add(proxy);
		all.add_options()("urls", po::value<std::vector<std::string>>(), "urls");

		po::positional_options_description p;
		p.add("urls", -1);

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(all).positional(p).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			spdlog::info("{}", title);
			spdlog::info("{}", all);
			return 0;
		}

		if (vm.count("version")) {
			spdlog::info("{}", title);
			return 0;
		}

		if (vm.count("urls") == 0) {
			spdlog::info("{}", title);
			return -1;
		}

		if (vm.count("debug"))
			spdlog::set_level(spdlog::level::debug);

		if (vm.count("proxy")) {
			proxy_config.enabled = true;
			try {
				std::tie(proxy_config.host, proxy_config.port, proxy_config.ssl)
				    = bbget::proxy::decode(vm["proxy"].as<std::string>());
			} catch (const boost::system::system_error& e) {
				spdlog::error("Failed to parse proxy configuration, error : {}", e.what());
				return -1;
			} catch (const std::exception& e) {
				spdlog::error("Failed to parse proxy configuration, error : {}", e.what());
				return -1;
			}

			if (vm.count("proxy-auth"))
				proxy_config.auth
				    = cppcodec::base64_rfc4648::encode(vm["proxy-auth"].as<std::string>());
		}

		urls = vm["urls"].as<std::vector<std::string>>();

	} catch (std::exception& e) {
		std::cout << e.what() << "\n";
	}

	// Set-up the io context;
	boost::asio::io_context   ioc;
	boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv13_client);

	ssl_ctx.set_verify_mode(boost::asio::ssl::context::verify_peer
	                        | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
#ifdef WIN32
	bbget::certs::add_windows_root_certs(ssl_ctx);
#else
	ssl_ctx.set_default_verify_paths();
#endif

	spdlog::debug("Use proxy: {}, host: {}  Port: {} Ssl: {} Auth:{}", proxy_config.enabled,
	              proxy_config.host, proxy_config.port, proxy_config.ssl, proxy_config.auth);

	bbget::http::outbound::connection_ops ops;
	for (auto&& url : urls) {
		try {
			bbget::http::downloader<bbget::http::outbound::connection_ops> session(
			    ioc, ssl_ctx, url, std::move(proxy_config), std::move(ops));
			session();
		} catch (const boost::system::system_error& e) {
			spdlog::error("{}", e.what());
		} catch (const std::exception& e) {
			spdlog::error("{}", e.what());
		}
	}

	return 0;
}
