#include "update.hxx"

#include "libclang++/libclang++.hxx"

#include "util/util.hxx"

#include <unistd.h>
#include <string>
#include <fstream>

namespace ClangTags {
namespace Indexer {

class Visitor : public LibClang::Visitor<Visitor>
{
public:
	Visitor(const std::string &fileName,
	        const std::vector<std::string> &exclude,
	        Storage &storage)
		: sourceFile_(fileName),
		exclude_(exclude),
		storage_(storage)
	{
		needsUpdate_[fileName] = storage.beginFile(fileName);
		storage_.addInclude(fileName, fileName);
	}

	CXChildVisitResult visit(LibClang::Cursor cursor,
	                         LibClang::Cursor parent)
	{
		const LibClang::Cursor cursorDef(cursor.referenced());

		// Skip non-reference cursors
		if (cursorDef.isNull())
		{
			return CXChildVisit_Recurse;
		}

		const std::string usr = cursorDef.USR();
		if (usr == "")
		{
			return CXChildVisit_Recurse;
		}

		const LibClang::SourceLocation::Position begin = cursor.location().expansionLocation();
		const String fileName = begin.file;

		if (fileName == "")
		{
			return CXChildVisit_Continue;
		}

		{                 // Skip excluded paths
			auto it  = exclude_.begin();
			auto end = exclude_.end();
			for (; it != end; ++it)
			{
				if (fileName.startsWith(*it))
				{
					return CXChildVisit_Continue;
				}
			}
		}

		if (needsUpdate_.count(fileName) == 0)
		{
			const bool needsUpdate = storage_.beginFile(fileName);
			needsUpdate_[fileName] = needsUpdate;
			if (needsUpdate)
			{
				std::cerr << "    " << fileName << std::endl;
			}
			storage_.addInclude(fileName, sourceFile_);
		}

		if (needsUpdate_[fileName])
		{
			const LibClang::SourceLocation::Position end = cursor.end().expansionLocation();
			storage_.addTag(usr, cursor.kindStr(), cursor.spelling(), fileName,
			                begin.line, begin.column, begin.offset,
			                end.line,   end.column,   end.offset,
			                cursor.isDeclaration());
		}

		return CXChildVisit_Recurse;
	}

private:
	const std::string              &sourceFile_;
	const std::vector<std::string> &exclude_;
	Storage             &storage_;
	std::map<std::string, bool>      needsUpdate_;
};


Update::Update(Storage &storage)
	: storage_(storage)
{
}

LibClang::TranslationUnit Update::translationUnit(Storage &storage,
                                                  std::string fileName)
{
	std::string directory;
	std::vector<std::string> clArgs;
	storage.getCompileCommand(fileName, directory, clArgs);

	// chdir() to the correct directory
	// (whether we need to parse the TU for the first time or reparse it)
	chdir(directory.c_str());

	static LibClang::Index index;
	LibClang::TranslationUnit tu = index.parse(clArgs);
	return tu;
}

void Update::operator()()
{
	Timer totalTimer;
	double parseTime = 0;
	double indexTime = 0;

	std::cerr << std::endl
	          << "-- Updating index" << std::endl;

	std::vector<std::string> exclude;
	storage_.getOption("index.exclude", exclude);

	bool diagnostics;
	storage_.getOption("index.diagnostics", diagnostics);

	std::string fileName;
	while ((fileName = storage_.nextFile()) != "")
	{
		std::cerr << fileName << ":" << std::endl
		          << "  parsing..." << std::flush;
		Timer timer;

		auto tu = translationUnit(storage_, fileName);

		double elapsed = timer.get();
		std::cerr << "\t" << elapsed << "s." << std::endl;
		parseTime += elapsed;

		// Print clang diagnostics if requested
		if (diagnostics)
		{
			for (unsigned int N = tu.numDiagnostics(),
			     i = 0; i < N; ++i)
			{
				std::cerr << tu.diagnostic(i) << std::endl << std::endl;
			}
		}

		std::cerr << "  indexing..." << std::endl;
		timer.reset();

		LibClang::Cursor top(tu);
		Visitor indexer(fileName, exclude, storage_);
		{
			auto transaction(storage_.beginTransaction());
			indexer.visitChildren(top);
		}

		elapsed = timer.get();
		std::cerr << "  indexing...\t" << elapsed << "s." << std::endl;
		indexTime += elapsed;
	}

	std::cerr << "Parsing time:  " << parseTime << "s." << std::endl
	          << "Indexing time: " << indexTime << "s." << std::endl
	          << "TOTAL:         " << totalTimer.get() << "s." << std::endl;
}
}
}
