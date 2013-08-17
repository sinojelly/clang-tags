#include "libclang++/libclang++.hxx"
#include "getopt++/getopt.hxx"

#include "util/util.hxx"
#include "index.hxx"

#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>

namespace ClangTags {

class Indexer : public LibClang::Visitor<Indexer> {
public:
  Indexer (const std::string & fileName,
           const std::vector<std::string> & exclude,
           Storage & storage,
           std::ostream & cout)
    : sourceFile_ (fileName),
      exclude_    (exclude),
      storage_    (storage),
      cout_       (cout)
  {
    needsUpdate_[fileName] = storage.beginFile (fileName);
    storage_.addInclude (fileName, fileName);
  }

  CXChildVisitResult visit (LibClang::Cursor cursor,
                            LibClang::Cursor parent)
  {
    const LibClang::Cursor cursorDef (cursor.referenced());

    // Skip non-reference cursors
    if (cursorDef.isNull()) {
      return CXChildVisit_Recurse;
    }

    const std::string usr = cursorDef.USR();
    if (usr == "") {
      return CXChildVisit_Recurse;
    }

    const LibClang::SourceLocation::Position begin = cursor.location().expansionLocation();
    const String fileName = begin.file;

    if (fileName == "") {
      return CXChildVisit_Continue;
    }

    { // Skip excluded paths
      auto it  = exclude_.begin();
      auto end = exclude_.end();
      for ( ; it != end ; ++it) {
        if (fileName.startsWith (*it)) {
          return CXChildVisit_Continue;
        }
      }
    }

    if (needsUpdate_.count(fileName) == 0) {
      cout_ << "    " << fileName << std::endl;
      needsUpdate_[fileName] = storage_.beginFile (fileName);
      storage_.addInclude (fileName, sourceFile_);
    }

    if (needsUpdate_[fileName]) {
      const LibClang::SourceLocation::Position end = cursor.end().expansionLocation();
      storage_.addTag (usr, cursor.kindStr(), cursor.spelling(), fileName,
                       begin.line, begin.column, begin.offset,
                       end.line,   end.column,   end.offset,
                       cursor.isDeclaration());
    }

    return CXChildVisit_Recurse;
  }

private:
  const std::string              & sourceFile_;
  const std::vector<std::string> & exclude_;
  Storage                        & storage_;
  std::map<std::string, bool>      needsUpdate_;
  std::ostream                   & cout_;
};


Index::Index (Storage & storage, Cache & cache)
  : storage_ (storage),
    cache_   (cache)
{}

void Index::operator() (std::ostream & cout) {
  Timer totalTimer;

  cout << std::endl
       << "-- Updating index" << std::endl;

  std::vector<std::string> exclude;
  storage_.getOption ("index.exclude", exclude);

  bool diagnostics;
  storage_.getOption ("index.diagnostics", diagnostics);

  storage_.beginIndex();
  std::string fileName;
  while ((fileName = storage_.nextFile()) != "") {
    cout << fileName << ":" << std::endl
         << "  parsing..." << std::flush;
    Timer timer;

    LibClang::TranslationUnit tu = cache_.translationUnit (fileName);

    cout << "\t" << timer.get() << "s." << std::endl;
    timer.reset();

    // Print clang diagnostics if requested
    if (diagnostics) {
      for (unsigned int N = tu.numDiagnostics(),
             i = 0 ; i < N ; ++i) {
        cout << tu.diagnostic (i) << std::endl << std::endl;
      }
    }

    cout << "  indexing..." << std::endl;
    LibClang::Cursor top (tu);
    Indexer indexer (fileName, exclude, storage_, cout);
    indexer.visitChildren (top);
    cout << "  indexing...\t" << timer.get() << "s." << std::endl;
  }
  storage_.endIndex();
  cout << totalTimer.get() << "s." << std::endl;
}
}