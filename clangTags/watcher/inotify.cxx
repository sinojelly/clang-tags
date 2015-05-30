#include "inotify.hxx"

#include <iostream>

#include <unistd.h>
#include <sys/inotify.h>
#include <sys/poll.h>

namespace ClangTags {
namespace Watcher {

Inotify::Inotify(Indexer::Indexer &indexer)
	: Watcher(indexer),
	updateRequested_(true),
	exitRequested_(false)
{
	// Initialize inotify
	fd_inotify_ = inotify_init();
	if (fd_inotify_ == -1)
	{
		perror("inotify_init");
		throw std::runtime_error("inotify");
	}
}

Inotify::~Inotify()
{
	// Close inotify file
	// -> the kernel will clean up all associated watches
	::close(fd_inotify_);
}

void Inotify::update()
{
	updateRequested_.store(true, std::memory_order_relaxed);
}

void Inotify::exit()
{
	exitRequested_.store(true, std::memory_order_relaxed);
}

void Inotify::update_()
{
	std::cerr << "Updating watchlist..." << std::endl;
	for (const auto &fileName : storage_.listFiles())
	{

		std::cerr << "Watching " << fileName << std::endl;
		int wd = inotify_add_watch(fd_inotify_, fileName.c_str(), IN_MODIFY);
		if (wd == -1)
		{
			perror("inotify_add_watch");
		}
	}
}

void Inotify::operator()()
{
	const size_t BUF_LEN = 1024;
	char buf[BUF_LEN] __attribute__((aligned(4)));

	struct pollfd fd;
	fd.fd = fd_inotify_;
	fd.events = POLLIN;

	for (;; )
	{

		// Check for interruptions
		if (exitRequested_.load(std::memory_order_relaxed))
			return;

		// Check whether a re-indexing was requested
		bool requested {true};
		if (updateRequested_.compare_exchange_weak(requested, false,
		                                           std::memory_order_release,
		                                           std::memory_order_relaxed))
		{

			// Reindex and update list
			update_();
		}

		// Look for an inotify event
		switch (poll(&fd, 1, 1000))
		{
		case -1:
			perror("poll");
			break;

		default:
			if (fd.revents & POLLIN)
			{
				ssize_t len, i = 0;
				len = read(fd.fd, buf, BUF_LEN);
				if (len < 1)
				{
					perror("read");
					break;
				}

				while (i<len)
				{
					// Read event & update index
					struct inotify_event *event = (struct inotify_event *) &(buf[i]);
					i += sizeof(struct inotify_event) + event->len;

					std::cerr << "Detected modification of " << event->name << std::endl;
				}

				// Schedule an index update
				reindex();
			}
		}
	}
}
}
}
