#include <boost/program_options.hpp>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char* argv[])
{
	std::vector<std::string> urls;
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

		po::options_description all("");
		all.add(general);
		all.add(log_and_config);
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

		urls = vm["urls"].as<std::vector<std::string>>();

	} catch (std::exception& e) {
		std::cout << e.what() << "\n";
	}

	for (auto&& url : urls) {
		spdlog::debug("Downloading {}", url);
	}

	return 0;
}
