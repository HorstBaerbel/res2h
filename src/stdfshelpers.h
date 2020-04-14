#pragma once

#include "stdfs.h"

/// @brief Uncomplete a path similar to relative (https://en.cppreference.com/w/cpp/filesystem/relative)
//  e.g. naiveRelative("/foo/new.file", "/foo/bar/") returns "../new.file".
/// Does not return a trailing ".." when paths only differ in their file name.
stdfs::path naiveRelative(const stdfs::path &path, const stdfs::path &base);

/// @brief Normalize path, similar to lexically_normal (https://en.cppreference.com/w/cpp/filesystem/path/lexically_normal).
/// so no . / .. or symlinks are are in the path anymore.
/// Does not check if the path exists.
stdfs::path naiveLexicallyNormal(const stdfs::path &path, const stdfs::path &base = stdfs::current_path());

/// @brief Returns true if path starts with prefix.
bool startsWithPrefix(const stdfs::path &path, const stdfs::path &prefix);

/// @brief Returns true if path has an recursive symlink.
bool hasRecursiveSymlink(const stdfs::path &path);

/// @brief Appends the content of srcFile to the end of dstFile.
/// @throw std::runtime_exception if one of the files can't be opened or reading / writing fails.
void appendFileContent(const stdfs::path &dstFile, const stdfs::path &srcFile);

/// @brief Compare the content of file a to file b and returns true if the content is binary equal.
/// @throw std::runtime_exception if one of the files can't be opened or reading fails.
bool compareFileContent(const stdfs::path &a, const stdfs::path &b);
