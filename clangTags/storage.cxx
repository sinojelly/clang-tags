#include "storage.hxx"
#include "sqlite++/sqlite.hxx"

#include <sys/stat.h>
#include <sstream>

namespace ClangTags {
class Storage::Impl
{
public:
	Impl()
		: db_(".ct.sqlite")
	{
		db_.execute("CREATE TABLE IF NOT EXISTS files ("
		            "  id      INTEGER,"
		            "  name    TEXT,"
		            "  indexed INTEGER,"
		            "  PRIMARY KEY (id)"
		            ")");
		db_.execute("CREATE TABLE IF NOT EXISTS commands ("
		            "  fileId     INTEGER REFERENCES files(id),"
		            "  directory  TEXT,"
		            "  args       TEXT,"
		            "  PRIMARY KEY (fileId)"
		            ")");
		db_.execute("CREATE TABLE IF NOT EXISTS includes ("
		            "  sourceId   INTEGER REFERENCES files(id),"
		            "  includedId INTEGER REFERENCES files(id),"
		            "  PRIMARY KEY (sourceId, includedId)"
		            ")");
		db_.execute("CREATE TABLE IF NOT EXISTS tags ("
		            "  fileId   INTEGER REFERENCES files(id),"
		            "  usr      TEXT,"
		            "  kind     TEXT,"
		            "  spelling TEXT,"
		            "  line1    INTEGER,"
		            "  col1     INTEGER,"
		            "  offset1  INTEGER,"
		            "  line2    INTEGER,"
		            "  col2     INTEGER,"
		            "  offset2  INTEGER,"
		            "  isDecl   BOOLEAN,"
		            "  PRIMARY KEY (fileId, usr, offset1, offset2)"
		            ")");
		db_.execute("CREATE TABLE IF NOT EXISTS options ( "
		            "  name   TEXT,"
		            "  value  TEXT,"
		            "  PRIMARY KEY (name)"
		            ")");
		setOptionDefault("index.exclude",     "[\"/usr\"]");
		setOptionDefault("index.diagnostics", "true");

		db_.execute("ANALYZE");
	}

	void setCompileCommand(const std::string &fileName,
	                       const std::string &directory,
	                       const std::vector<std::string> &args)
	{
		int fileId = addFile_(fileName);
		addInclude_(fileId, fileId);

		db_.prepare("DELETE FROM commands "
		            "WHERE fileId=?")
		.bind(fileId)
		.step();

		db_.prepare("INSERT INTO commands VALUES (?,?,?)")
		.bind(fileId)
		.bind(directory)
		.bind(serialize_(args))
		.step();
	}

	void getCompileCommand(const std::string &fileName,
	                       std::string &directory,
	                       std::vector<std::string> &args)
	{
		int fileId = fileId_(fileName);
		Sqlite::Statement stmt
		    = db_.prepare("SELECT commands.directory, commands.args "
		                  "FROM includes "
		                  "INNER JOIN commands ON includes.sourceId = commands.fileId "
		                  "WHERE includes.includedId = ?");
		stmt.bind(fileId);

		switch (stmt.step())
		{
		case SQLITE_DONE:
			throw std::runtime_error("no compilation command for file `"
			                         + fileName + "'");
			break;
		default:
			std::string serializedArgs;
			stmt >> directory >> serializedArgs;
			deserialize_(serializedArgs, args);
		}
	}

	std::vector<std::string> listFiles()
	{
		Sqlite::Statement stmt = db_.prepare("SELECT name FROM files ORDER BY name");

		std::vector<std::string> list;
		while (stmt.step() == SQLITE_ROW)
		{
			std::string fileName;
			stmt >> fileName;
			list.push_back(fileName);
		}
		return list;
	}

	std::string nextFile()
	{
		Sqlite::Statement stmt
		    = db_.prepare("SELECT included.name, included.indexed, source.name, "
		                  "       count(source.name) AS sourceCount "
		                  "FROM includes "
		                  "INNER JOIN files AS source ON source.id = includes.sourceId "
		                  "INNER JOIN files AS included ON included.id = includes.includedId "
		                  "GROUP BY included.id "
		                  "ORDER BY sourceCount ");
		while (stmt.step() == SQLITE_ROW)
		{
			std::string includedName;
			int indexed;
			std::string sourceName;
			stmt >> includedName >> indexed >> sourceName;

			struct stat fileStat;
			if (stat(includedName.c_str(), &fileStat) != 0)
			{
				std::cerr << "Warning: could not stat() file `" << includedName << "'" << std::endl
				          << "  removing it from the index" << std::endl;
				removeFile_(includedName);
				continue;
			}
			int modified = fileStat.st_mtime;

			if (modified > indexed)
			{
				return sourceName;
			}
		}

		return "";
	}

	bool beginFile(const std::string &fileName)
	{
		int fileId = addFile_(fileName);

		int indexed;
		{
			Sqlite::Statement stmt
			    = db_.prepare("SELECT indexed FROM files WHERE id = ?");
			stmt.bind(fileId);
			stmt.step();
			stmt >> indexed;
		}

		struct stat fileStat;
		stat(fileName.c_str(), &fileStat);
		int modified = fileStat.st_mtime;

		if (modified > indexed)
		{
			db_.prepare("DELETE FROM tags WHERE fileId=?").bind(fileId).step();
			db_.prepare("DELETE FROM includes WHERE sourceId=?").bind(fileId).step();
			db_.prepare("UPDATE files"
			            " SET indexed=?"
			            " WHERE id=?")
			.bind(modified)
			.bind(fileId)
			.step();
			return true;
		}
		else
		{
			return false;
		}
	}

	void addInclude(const std::string &includedFile,
	                const std::string &sourceFile)
	{
		int includedId = fileId_(includedFile);
		int sourceId   = fileId_(sourceFile);
		if (includedId == -1 || sourceId == -1)
			throw std::runtime_error("Cannot add inclusion for unknown files `"
			                         + includedFile + "' and `" + sourceFile + "'");
		addInclude_(includedId, sourceId);
	}

	void addTag(const std::string &usr,
	            const std::string &kind,
	            const std::string &spelling,
	            const std::string &fileName,
	            const int line1, const int col1, const int offset1,
	            const int line2, const int col2, const int offset2,
	            bool isDeclaration)
	{
		int fileId = fileId_(fileName);
		if (fileId == -1)
		{
			return;
		}

		db_.prepare("INSERT INTO tags VALUES (?,?,?,?,?,?,?,?,?,?,?)")
		.bind(fileId).bind(usr).bind(kind).bind(spelling)
		.bind(line1).bind(col1).bind(offset1)
		.bind(line2).bind(col2).bind(offset2)
		.bind(isDeclaration)
		.step();
	}

	std::vector<ClangTags::Identifier> findDefinition(const std::string &fileName,
	                                                  int offset)
	{
		int fileId = fileId_(fileName);
		Sqlite::Statement stmt =
		    db_.prepare("SELECT ref.offset1, ref.offset2, ref.kind, ref.spelling,"
		                "       def.usr, defFile.name,"
		                "       def.line1, def.line2, def.col1, def.col2, "
		                "       def.kind, def.spelling "
		                "FROM tags AS ref "
		                "INNER JOIN tags AS def ON def.usr = ref.usr "
		                "INNER JOIN files AS defFile ON def.fileId = defFile.id "
		                "WHERE def.isDecl = 1 "
		                "  AND ref.fileId = ?  "
		                "  AND ref.offset1 <= ? "
		                "  AND ref.offset2 >= ? "
		                "ORDER BY (ref.offset2 - ref.offset1)");
		stmt.bind(fileId)
		    .bind(offset)
		    .bind(offset);

		std::vector<ClangTags::Identifier> ret;
		while (stmt.step() == SQLITE_ROW)
		{
			ClangTags::Identifier identifier;
			ClangTags::Identifier::Reference &ref = identifier.ref;
			ClangTags::Identifier::Definition &def = identifier.def;

			stmt >> ref.offset1 >> ref.offset2 >> ref.kind >> ref.spelling
			>> def.usr >> def.file
			>> def.line1 >> def.line2 >> def.col1 >> def.col2
			>> def.kind >> def.spelling;
			ref.file = fileName;
			ret.push_back(identifier);
		}
		return ret;
	}

	std::vector<ClangTags::Identifier::Reference> grep(const std::string &usr)
	{
		Sqlite::Statement stmt =
		    db_.prepare("SELECT ref.line1, ref.line2, ref.col1, ref.col2, "
		                "       ref.offset1, ref.offset2, refFile.name, ref.kind "
		                "FROM tags AS ref "
		                "INNER JOIN files AS refFile ON ref.fileId = refFile.id "
		                "WHERE ref.usr = ?");
		stmt.bind(usr);

		std::vector<ClangTags::Identifier::Reference> ret;
		while (stmt.step() == SQLITE_ROW)
		{
			ClangTags::Identifier::Reference ref;
			stmt >> ref.line1 >> ref.line2 >> ref.col1 >> ref.col2
			>> ref.offset1 >> ref.offset2 >> ref.file >> ref.kind;
			ret.push_back(ref);
		}
		return ret;
	}

	void setOptionDefault(const std::string &name, const std::string &value)
	{
		int res = db_.prepare("SELECT name FROM options WHERE name = ?")
		          .bind(name)
		          .step();

		if (res == SQLITE_DONE)
		{
			db_.prepare("INSERT INTO options VALUES (?, ?)")
			.bind(name)
			.bind(value)
			.step();
		}
	}

	void setOption(const std::string &name, const std::string &value)
	{
		db_.prepare("UPDATE options SET value = ? WHERE name = ?")
		.bind(value)
		.bind(name)
		.step();
	}

	void getOption(const std::string &name, bool &destination)
	{
		getOption_(name, destination);
	}

	void getOption(const std::string &name, std::string &destination)
	{
		Sqlite::Statement stmt =
		    db_.prepare("SELECT value FROM options "
		                "WHERE name = ?");
		stmt.bind(name);

		if (stmt.step() == SQLITE_ROW)
		{
			stmt >> destination;
		}
		else
		{
			throw std::runtime_error("No stored value for option: `" + name + "'");
		}
	}

	void getOption(const std::string &name, std::vector<std::string> &destination)
	{
		std::string val;
		getOption(name, val);
		deserialize_(val, destination);
	}

	Sqlite::Transaction beginTransaction()
	{
		return Sqlite::Transaction(db_);
	}

private:
	int fileId_(const std::string &fileName)
	{
		Sqlite::Statement stmt
		    = db_.prepare("SELECT id FROM files WHERE name=?");
		stmt.bind(fileName);

		int id = -1;
		if (stmt.step() == SQLITE_ROW)
		{
			stmt >> id;
		}

		return id;
	}

	int addFile_(const std::string &fileName)
	{
		int id = fileId_(fileName);
		if (id == -1)
		{
			db_.prepare("INSERT INTO files VALUES (NULL, ?, 0)")
			.bind(fileName)
			.step();

			id = db_.lastInsertRowId();
		}
		return id;
	}

	void removeFile_(const std::string &fileName)
	{
		int fileId = fileId_(fileName);
		db_
		.prepare("DELETE FROM commands WHERE fileId = ?")
		.bind(fileId)
		.step();

		db_
		.prepare("DELETE FROM includes WHERE sourceId = ?")
		.bind(fileId)
		.step();

		db_
		.prepare("DELETE FROM includes WHERE includedId = ?")
		.bind(fileId)
		.step();

		db_
		.prepare("DELETE FROM tags WHERE fileId = ?")
		.bind(fileId)
		.step();

		db_.prepare("DELETE FROM files WHERE id = ?")
		.bind(fileId)
		.step();
	}

	void addInclude_(const int includedId,
	                 const int sourceId)
	{
		int res = db_.prepare("SELECT * FROM includes "
		                      "WHERE sourceId=? "
		                      "  AND includedId=?")
		          .bind(sourceId).bind(includedId)
		          .step();
		if (res == SQLITE_DONE)         // No matching row
		{
			db_.prepare("INSERT INTO includes VALUES (?,?)")
			.bind(sourceId).bind(includedId)
			.step();
		}
	}

	template <typename T>
	void getOption_(const std::string &name, T &destination)
	{
		std::string val;
		getOption(name, val);
		std::istringstream iss(val);
		iss >> std::boolalpha >> destination;
	}

	std::string serialize_(const std::vector<std::string> &v)
	{
		Json::Value json;
		auto it = v.begin();
		auto end = v.end();
		for (; it != end; ++it)
		{
			json.append(*it);
		}

		Json::FastWriter writer;
		return writer.write(json);
	}

	void deserialize_(const std::string &s, std::vector<std::string> &v)
	{
		Json::Value json;
		Json::Reader reader;
		if (!reader.parse(s, json))
		{
			throw std::runtime_error(reader.getFormattedErrorMessages());
		}

		for (unsigned int i=0; i<json.size(); ++i)
		{
			v.push_back(json[i].asString());
		}
	}

	Sqlite::Database db_;
};

Storage::Storage()
	: impl_(new Impl)
{
}

Storage::~Storage()
{
	delete impl_;
}

void Storage::setCompileCommand(const std::string &fileName,
                                const std::string &directory,
                                const std::vector<std::string> &args)
{
	return impl_->setCompileCommand(fileName, directory, args);
}

void Storage::getCompileCommand(const std::string &fileName,
                                std::string &directory,
                                std::vector<std::string> &args)
{
	return impl_->getCompileCommand(fileName, directory, args);
}

std::vector<std::string> Storage::listFiles()
{
	return impl_->listFiles();
}

std::string Storage::nextFile()
{
	return impl_->nextFile();
}

bool Storage::beginFile(const std::string &fileName)
{
	return impl_->beginFile(fileName);
}

void Storage::addInclude(const std::string &includedFile,
                         const std::string &sourceFile)
{
	return impl_->addInclude(includedFile, sourceFile);
}

// TODO use a structure instead of a large number of arguments
void Storage::addTag(const std::string &usr,
                     const std::string &kind,
                     const std::string &spelling,
                     const std::string &fileName,
                     const int line1, const int col1, const int offset1,
                     const int line2, const int col2, const int offset2,
                     bool isDeclaration)
{
	return impl_->addTag(usr, kind, spelling, fileName,
	                     line1, col1, offset1, line2, col2, offset2,
	                     isDeclaration);
}


std::vector<ClangTags::Identifier> Storage::findDefinition(const std::string &fileName,
                                                           int offset)
{
	return impl_->findDefinition(fileName,
	                             offset);
}

std::vector<ClangTags::Identifier::Reference> Storage::grep(const std::string &usr)
{
	return impl_->grep(usr);
}

void Storage::getOption(const std::string &name, std::string &destination)
{
	return impl_->getOption(name, destination);
}

void Storage::getOption(const std::string &name, bool &destination)
{
	return impl_->getOption(name, destination);
}

void Storage::getOption(const std::string &name, std::vector<std::string> &destination)
{
	return impl_->getOption(name, destination);
}

void Storage::setOption(const std::string &name, const std::string &value)
{
	return impl_->setOption(name, value);
}

void Storage::setOptionDefault(const std::string &name, const std::string &value)
{
	return impl_->setOptionDefault(name, value);
}

Sqlite::Transaction Storage::beginTransaction()
{
	return impl_->beginTransaction();
}
}

// vim: set noet ts=4 sw=4:
// dbext:type=SQLITE:dbname=.ct.sqlite
