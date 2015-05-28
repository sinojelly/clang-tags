#include "load.hxx"
#include "json/json.h"

#include <unistd.h>
#include <stdexcept>

namespace ClangTags {
namespace Server {

Load::Load(Storage &storage,
           Indexer::Indexer    &indexer)
	: Request::CommandParser("load", "Read a compilation database"),
	storage_(storage),
	indexer_(indexer)
{
	prompt_ = "load> ";
	defaults();

	using Request::key;
	add(key("database", args_.fileName)
	    ->metavar("FILEPATH")
	    ->description("Load compilation commands from a JSON compilation database"));

	cwd_.resize(4096);
	if (getcwd(cwd_.data(), cwd_.size()) == NULL)
	{
		// FIXME: correctly handle this case
		throw std::runtime_error("Not enough space to store current directory name.");
	}
}

Load::~Load()
{
}

void Load::defaults()
{
	args_.fileName = "compile_commands.json";
}

void Load::run(std::ostream &cout)
{
	// Change back to the original WD (in case `index` or `update` would have
	// changed it)
	chdir(cwd_.data());

	Json::Value root;
	Json::Reader reader;

	std::ifstream json(args_.fileName);
	bool ret = reader.parse(json, root);
	if ( !ret )
	{
		cout << "Failed to parse compilation database `" << args_.fileName << "'\n"
		     << reader.getFormattedErrorMessages();
	}

	auto transaction(storage_.beginTransaction());

	for (const auto &section : root)
	{
		auto fileName = section["file"].asString();
		auto directory = section["directory"].asString();

		std::vector<std::string> clArgs;
		std::istringstream command(section["command"].asString());
		std::string arg;
		std::getline(command, arg, ' ');
		do {
			std::getline(command, arg, ' ');
			clArgs.push_back(arg);
		} while (!command.eof());

		cout << "  " << fileName << std::endl;
		storage_.setCompileCommand(fileName, directory, clArgs);
	}

	indexer_.index();
}
}
}
