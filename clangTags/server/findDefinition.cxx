#include "findDefinition.hxx"
#include "sourceFile.hxx"

#include "libclang++/libclang++.hxx"
#include <iostream>
#include <cstdlib>

namespace ClangTags {
namespace Server {

FindDefinition::FindDefinition(Storage &storage)
	: Request::CommandParser("find", "Find the definition of a symbol"),
	storage_(storage)
{
	prompt_ = "find> ";
	defaults();

	using Request::key;
	add(key("file", args_.fileName)
	    ->metavar("FILENAME")
	    ->description("Source file name"));
	add(key("offset", args_.offset)
	    ->metavar("OFFSET")
	    ->description("Offset in bytes"));
	add(key("mostSpecific", args_.mostSpecific)
	    ->metavar("true|false")
	    ->description("Display only the most specific identifier at this location"));
	add(key("diagnostics", args_.diagnostics)
	    ->metavar("true|false")
	    ->description("Print compilation diagnostics"));
}

void FindDefinition::defaults()
{
	args_.fileName = "";
	args_.offset = 0;
	args_.mostSpecific = false;
	args_.diagnostics = true;
}

void FindDefinition::prettyPrint(const ClangTags::Identifier &identifier,
                                 std::ostream &cout)
{
	const ClangTags::Identifier::Reference  &ref = identifier.ref;
	const ClangTags::Identifier::Definition &def = identifier.def;

	// Display reference
	{
		SourceFile sourceFile(ref.file);

		cout << "-- " << sourceFile.substring(ref.offset1, ref.offset2) << " -- "
		     << ref.kind << " " << ref.spelling
		     << std::endl;
	}

	// Display definition
	{
		cout << "   "
		     << def.file  << ":"
		     << def.line1 << "-" << def.line2 << ":"
		     << def.col1  << "-" << def.col2-1 << ": "
		     << def.kind  << " " << def.spelling
		     << std::endl << "   USR: " << def.usr
		     << std::endl << std::endl;
	}
}

void FindDefinition::output(const ClangTags::Identifier &identifier,
                            std::ostream &cout)
{
	Json::FastWriter writer;
	Json::Value json = identifier.json();

	const ClangTags::Identifier::Reference &ref = identifier.ref;
	SourceFile sourceFile(ref.file);
	json["ref"]["substring"] = sourceFile.substring(ref.offset1, ref.offset2);

	cout << writer.write(json);
}

void FindDefinition::output(LibClang::Cursor cursor,
                            std::ostream &cout)
{
	const LibClang::SourceLocation location(cursor.location());
	const LibClang::Cursor cursorDef = cursor.referenced();

	if (cursorDef.isNull())
	{
		return;
	}

	ClangTags::Identifier identifier;
	ClangTags::Identifier::Reference &ref = identifier.ref;
	ClangTags::Identifier::Definition &def = identifier.def;

	// Get cursor information
	{
		const LibClang::SourceLocation::Position begin = location.expansionLocation();
		const LibClang::SourceLocation::Position end = cursor.end().expansionLocation();
		ref.file = begin.file;
		ref.offset1 = begin.offset;
		ref.offset2 = end.offset;
		ref.kind = cursor.kindStr();
		ref.spelling = cursor.spelling();
	}

	// Get referenced cursor information
	{
		const LibClang::SourceLocation::Position begin = cursorDef.location().expansionLocation();
		const LibClang::SourceLocation::Position end = cursorDef.end().expansionLocation();
		def.file = begin.file;
		def.line1 = begin.line;
		def.line2 = end.line;
		def.col1 = begin.column;
		def.col2 = end.column;
		def.kind = cursorDef.kindStr();
		def.spelling = cursorDef.spelling();
		def.usr = cursorDef.USR();
	}

	output(identifier, cout);
}

void FindDefinition::run(std::ostream &cout)
{
	const auto refDefs = storage_.findDefinition(args_.fileName, args_.offset);
	auto refDef = refDefs.begin();
	const auto end = args_.mostSpecific
	                 ? refDef + 1
					 : refDefs.end();
	for (; refDef != end; ++refDef )
	{
		output(*refDef, cout);
	}
}
}
}
