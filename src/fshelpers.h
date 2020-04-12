#pragma once

#if defined(__GNUC__) || defined(__clang__)
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#elif defined(_MSC_VER)
#include <filesystem>
namespace stdfs = std::tr2::sys;
#endif

/// @brief Uncomplete a path, e.g. naiveUncomplete("/foo/new", "/foo/bar") returns "../new".
/// Does not return a trailing ".." when paths only differ in their file name.
stdfs::path naiveUncomplete(const stdfs::path &path, const stdfs::path &base);

/// @brief Normalize path, similar to lexically_normal (https://en.cppreference.com/w/cpp/filesystem/path/lexically_normal).
/// so no . / .. or symlinks are are in the path anymore.
/// Does not check if the path exists.
stdfs::path normalize(const stdfs::path &path, const stdfs::path &base = stdfs::current_path());

/// @brief Returns true if path starts with prefix.
bool startsWithPrefix(const stdfs::path &path, const stdfs::path &prefix);

/// @brief Returns true if path has an recursive symlink.
bool hasRecursiveSymlink(const stdfs::path &path);
