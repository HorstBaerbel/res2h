#pragma once

#if defined(__GNUC__) || defined(__clang__)
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#elif defined(_MSC_VER)
#include <filesystem>
namespace stdfs = std::tr2::sys;
#endif

/// @brief Uncomplete a path, e.g. NaiveUncomplete("/foo/new", "/foo/bar") returns "../new".
/// Does not return a trailing ".." when paths only differ in their file name.
stdfs::path naiveUncomplete(const stdfs::path &path, const stdfs::path &base);

/// @brief Make path canonical, but does not throw an exception,
/// if the file part of  do not exist.
/// Sort of replaces weak_canonical: https://en.cppreference.com/w/cpp/filesystem/canonical
/// as that does not seem to be available yet.
/// @throw Throws stdfs::filesystem_error if part of the path does not exits.
stdfs::path makeCanonical(const stdfs::path &path);

/// @brief Returns true if path starts with prefix.
bool hasPrefix(const stdfs::path &path, const stdfs::path &prefix);
