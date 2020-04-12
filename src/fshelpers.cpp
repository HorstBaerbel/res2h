#include "fshelpers.h"

// This is based on the example code found here: https://svn.boost.org/trac/boost/ticket/1976
stdfs::path naiveUncomplete(const stdfs::path &path, const stdfs::path &base)
{
    if (path.has_root_path())
    {
        // make sure the roots of both paths match, otherwise they're on different drives
        if (path.root_path() != base.root_path())
        {
            return path;
        }
        return naiveUncomplete(path.relative_path(), base.relative_path());
    }
    else
    {
        if (base.has_root_path())
        {
            return path;
        }
        // remove file name portion of base path
        auto baseDir = base;
        if (baseDir.has_filename())
        {
            baseDir = baseDir.parent_path();
        }
        // skip all directories both paths have in common
        auto pathIt = path.begin();
        auto baseIt = baseDir.begin();
        while (pathIt != path.end() && baseIt != baseDir.end())
        {
            if (*pathIt != *baseIt)
            {
                break;
            }
            ++pathIt;
            ++baseIt;
        }
        stdfs::path result;
        // now if there's directories left over in base, we need to add .. for every level
        while (baseIt != baseDir.end())
        {
            result /= "..";
            ++baseIt;
        }
        // now if there's directories left over in path add them to the result
        while (pathIt != path.end())
        {
            result /= *pathIt;
             ++pathIt;
        }
        return result;
    }
}

stdfs::path normalize(const stdfs::path &path, const stdfs::path &base)
{
    stdfs::path absPath = stdfs::absolute(path, base);
    stdfs::path result;
    for (stdfs::path::iterator pIt = absPath.begin(); pIt != absPath.end(); ++pIt)
    {
        if (*pIt == "..")
        {
            // /a/b/.. is not necessarily /a if b is a symbolic link
            if (stdfs::is_symlink(result))
            {
                result /= *pIt;
            }
            // /a/b/../.. is not /a/b/.. under most circumstances
            // We can end up with ..s in our result because of symbolic links
            else if (result.filename() == "..")
            {
                result /= *pIt;
            }
            // Otherwise it should be safe to resolve the parent
            else
            {
                result = result.parent_path();
            }
        }
        else if (*pIt == ".")
        {
            // Ignore .
        }
        else
        {
            // Just cat other path entries
            result /= *pIt;
        }
    }
    return result;
}

bool startsWithPrefix(const stdfs::path &path, const stdfs::path &prefix)
{
    if (path.empty() || prefix.empty())
    {
        return false;
    }
    auto pair = std::mismatch(path.begin(), path.end(), prefix.begin(), prefix.end());
    return pair.second == prefix.end();
}

bool hasRecursiveSymlink(const stdfs::path &path)
{
	if (stdfs::is_symlink(path))
	{
		// check if the symlink points somewhere in the path. this would recurse
		if (startsWithPrefix(stdfs::canonical(path), path))
		{
			return true;
		}
	}
    return false;
}