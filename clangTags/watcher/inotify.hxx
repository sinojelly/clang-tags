#pragma once

#include "config.h"
#if defined HAVE_INOTIFY

#include "watcher.hxx"
#include "clangTags/storage.hxx"

#include <atomic>
#include <unordered_set>

namespace ClangTags {
namespace Watcher {
/** @addtogroup clangTags
 *  @{
 */

/** @brief Implementation of the Watcher interface using Linux' inotify service.
 *
 * An Inotify instance is destined to be invoked as functor in a separate thread
 * of execution.
 */
class Inotify : public Watcher
{
public:

	/** @brief Constructor
	 *
	 * This object accesses the database using its own @ref Storage handle.
	 *
	 * Inotify instances are constructed with an initial index update already
	 * scheduled (see update()).
	 *
	 * @param indexer  @ref Indexer::Indexer "Indexer" instance to notify
	 */
	Inotify
	    (Indexer::Indexer &indexer);

	~Inotify
	    ();

	/** @brief Main loop
	 *
	 * This is the main loop executed by the thread. It watches for:
	 * - source files list update requests coming from other threads (see update())
	 * - notifications that sources have changed on the file system (using Linux's
	 *   @c inotify service)
	 */
	void operator()
	    ();

	/** @brief Schedule an update of the watched source files
	 *
	 * Calling this method asks the Watch thread to update its list of source
	 * files to watch
	 */
	void update
	    ();

	/** @brief Request the inotify loop to quit
	 */
	void exit
	    ();

private:
	void update_
	    ();

	Storage storage_;
	int fd_inotify_;
	std::unordered_set<std::string> watchedFiles_;
	std::atomic_bool updateRequested_;
	std::atomic_bool exitRequested_;
};
/** @} */
}
}

#endif //defined HAVE_INOTIFY
