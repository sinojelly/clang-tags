#include "config.h"

#include <boost/program_options.hpp>

#include "clangTags/indexer/indexer.hxx"
#include "clangTags/watcher/inotify.hxx"
#include "clangTags/server/server.hxx"

#include <thread>

int main(int argc, char **argv)
{
	namespace po = boost::program_options;

	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	int line_length = w.ws_col;

	po::options_description desc("Options", line_length, line_length / 2);
	desc.add_options()
		("help,h", "print this help message and exit")
		("stdin,s", "read a request from the standard input and exit")
		;

	po::variables_map vm;

	try
	{
		po::store(po::command_line_parser(argc, argv)
					.options(desc)
					.run(),
				  vm);
	}
	catch (...)
	{
		std::cerr << desc;
		return 1;
	}

	if (vm.count("help"))
	{
		std::cerr << desc;
		return 0;
	}

	const bool fromStdin = vm.count("stdin") > 0;

	ClangTags::Indexer::Indexer indexer;

#if defined(HAVE_INOTIFY)
	ClangTags::Watcher::Inotify watcher(indexer);
	indexer.setWatcher(&watcher);
#endif

	try
	{
		std::vector<std::thread> threads;

		threads.emplace_back(std::ref(indexer));
#if defined(HAVE_INOTIFY)
		threads.emplace_back(std::ref(watcher));
#endif

		ClangTags::Server::Server server(indexer);
		server.run(fromStdin);

		indexer.exit();
#if defined(HAVE_INOTIFY)
		watcher.exit();
#endif

		for (auto &t : threads)
			t.join();
	}
	catch (std::exception&e)
	{
		std::cerr << std::endl << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
