#include "fshelpers.h"

// This is based on the example code found here: https:// svn.boost.org/trac/boost/ticket/1976
stdfs::path naiveUncomplete(stdfs::path const &path, stdfs::path const &base)
{
    if (path.has_root_path())
    {
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
        auto path_it = path.begin();
        auto base_it = base.begin();
        while (path_it != path.end() && base_it != base.end())
        {
            if (*path_it != *base_it)
            {
                break;
            }
            ++path_it;
            ++base_it;
        }
        stdfs::path result;
        // check if we're at the filename of the base path already
        if (*base_it != base.filename())
        {
            // add trailing ".." from path to base, but only if we're not already at the filename of the base path
            for (; base_it != base.end() && *base_it != base.filename(); ++base_it)
            {
                result /= "..";
            }
        }
        for (; path_it != path.end(); ++path_it)
        {
            result /= *path_it;
        }
        return result;
    }
    return path;
}

stdfs::path makeCanonical(const stdfs::path &path)
{
    try
    {
        return stdfs::canonical(path);
    }
    catch (const stdfs::filesystem_error & /*e*/)
    {
        // an error occurred. this maybe because part of the path not there
        // we only check the file name part here, for simplicity
        stdfs::path result = stdfs::canonical(stdfs::path(path).remove_filename());
        // ok. this worked. add file name again
        result /= path.filename();
        return result;
    }
}

bool hasPrefix(const stdfs::path &path, const stdfs::path &prefix)
{
    auto pair = std::mismatch(path.begin(), path.end(), prefix.begin(), prefix.end());
    return pair.second == prefix.end();
}
