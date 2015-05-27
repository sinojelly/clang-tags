#include "config.h"

#include "getopt++/getopt.hxx"
#include "MT/threadsList.hxx"

#include "clangTags/indexer/indexer.hxx"
#include "clangTags/watcher/inotify.hxx"
#include "clangTags/server/server.hxx"

int main (int argc, char **argv) {
  Getopt options (argc, argv);
  options.add ("help", 'h', 0,
               "print this help message and exit");
  options.add ("stdin", 's', 0,
               "read a request from the standard input and exit");

  try {
    options.get();
  } catch (...) {
    std::cerr << options.usage();
    return 1;
  }

  if (options.getCount ("help") > 0) {
    std::cerr << options.usage();
    return 0;
  }

  const bool fromStdin = options.getCount ("stdin") > 0;

  ClangTags::Indexer::Indexer indexer;

#if defined(HAVE_INOTIFY)
  ClangTags::Watcher::Inotify watcher (indexer);
  indexer.setWatcher (&watcher);
#endif

  try {
    MT::ThreadsList threads;
    threads.add (boost::ref(indexer));
#if defined(HAVE_INOTIFY)
    threads.add (boost::ref(watcher));
#endif

    ClangTags::Server::Server server (indexer);
    server.run (fromStdin);
  }
  catch (std::exception& e) {
    std::cerr << std::endl << "Caught exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
