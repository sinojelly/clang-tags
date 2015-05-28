#pragma once

#include "clangTags/storage.hxx"
#include "libclang++/translationUnit.hxx"

namespace ClangTags {
namespace Indexer {
/** @addtogroup clangTags
 *  @{
 */

/** @brief Reindex source files
 */
class Update
{
public:
	/** @brief Constructor
	 *
	 * @param storage  Storage instance
	 */
	Update
	    (Storage &storage);

	/** @brief Reindex the source files
	 *
	 * Retrieve from the compilation database all source and header files which
	 * have been modified in the filesystem since last indexing. For each of them,
	 * (re)parse and (re)index the associated translation unit.
	 */
	void operator()
	    ();

private:
	Storage &storage_;

	/** @brief Get the translation unit associated to a source file
	 *
	 * The source file is parsed from scratch.
	 *
	 * @param storage  Storage from which compilation commands can be retrieved
	 * @param fileName full path to the source file
	 *
	 * @return an up-to-date LibClang::TranslationUnit associated to @c fileName
	 */
	LibClang::TranslationUnit translationUnit
	    (Storage &storage,
	    std::string fileName);
};
/** @} */
}
}
