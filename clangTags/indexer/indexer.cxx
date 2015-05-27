#include "indexer.hxx"

#include "clangTags/watcher/watcher.hxx"

namespace ClangTags {
namespace Indexer {

Indexer::Indexer ()
  : update_         (storage_),
    indexRequested_ (true),
    indexUpdated_   (false),
    watcher_        (NULL),
    exitRequested_  (false)
{}

void Indexer::setWatcher (Watcher::Watcher * watcher)
{
  watcher_ = watcher;
}

void Indexer::index () {
  indexRequested_.set (true);
}

void Indexer::exit () {
  exitRequested_.store(true, std::memory_order_relaxed);
  indexRequested_.set(false);
}

void Indexer::wait () {
  indexUpdated_.get();
}

void Indexer::updateIndex_ () {
  // Reindex source files
  update_();

  // Notify the watching thread
  if (watcher_)
    watcher_->update();

  // Reset flag and notify waiting threads
  indexRequested_.set (false);
  indexUpdated_.set (true);
}

void Indexer::operator() () {
  updateIndex_();

  for ( ; ; ) {
    if (exitRequested_.load(std::memory_order_relaxed))
      break;

    // Check whether a re-indexing was requested
    if (indexRequested_.get() == true) {
      updateIndex_();
    }
  }
}
}
}
