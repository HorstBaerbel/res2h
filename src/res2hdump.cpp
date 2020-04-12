#include "checksum.h"
#include "fshelpers.h"
#include "res2hinterface.h"

#include <fstream>
#include <iostream>

#if defined(__GNUC__) || defined(__clang__)
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#elif defined(_MSC_VER)
#include <filesystem>
namespace stdfs = std::tr2::sys;
#endif

static bool beVerbose = false;
static bool useFullPaths = false;
static bool informationOnly = false;
static stdfs::path inFilePath;
static stdfs::path outFilePath;

// -----------------------------------------------------------------------------

static void printVersion()
{
	std::cout << "res2hdump " << RES2H_VERSION_STRING << " - Dump data from a res2h archive file or embedded archive." << std::endl << std::endl;
}

static void printUsage()
{
	std::cout << std::endl;
	std::cout << "Usage: res2hdump <archive> [<outdir>] [options]" << std::endl;
	std::cout << "Valid options:" << std::endl;
	std::cout << "-f Recreate path structure, creating directories as needed." << std::endl;
	std::cout << "-i Display information about the archive and files, but don't extract anything." << std::endl;
	std::cout << "-v Be verbose." << std::endl;
	std::cout << "Examples:" << std::endl;
	std::cout << "res2hdump ./resources/data.bin -i (display archive information)" << std::endl;
	std::cout << "res2hdump ./resources/data.bin ./resources (extract files from archive)" << std::endl;
	std::cout << "res2hdump ./resources/program.exe ./resources (extract files from embedded archive)" << std::endl;
}

bool readArguments(int argc, const char * argv[])
{
	bool pastFiles = false;
	for (int i = 1; i < argc; ++i)
	{
		// read argument from list
		std::string argument = argv[i];
		// check what it is
		if (argument == "-f")
		{
			useFullPaths = true;
			pastFiles = true;
		}
		else if (argument == "-i")
		{
			informationOnly = true;
			pastFiles = true;
		}
		else if (argument == "-v")
		{
			beVerbose = true;
			pastFiles = true;
		}
		// none of the options was matched until here...
		else if (!pastFiles)
		{
			// if no files/directories have been found yet this is probably a file/directory
			if (inFilePath.empty())
			{
				inFilePath = normalize(stdfs::path(argument));
				if (inFilePath.empty())
				{
					return false;
				}
			}
			else if (outFilePath.empty())
			{
				outFilePath = normalize(stdfs::path(argument));
				if (outFilePath.empty())
				{
					return false;
				}
				pastFiles = true;
			}
		}
		else
		{
			std::cout << "Error: Unknown argument \"" << argument << "\"!" << std::endl;
			return false;
		}
	}
	return true;
}

// -----------------------------------------------------------------------------

bool dumpArchive(stdfs::path & destination, stdfs::path & archive, bool createPaths, bool dontExtract)
{
	try
	{
		if (Res2h<uint64_t>::instance().loadArchive(archive.string()))
		{
			// dump archive information
			const auto archiveInfo = Res2h<uint64_t>::instance().getArchiveInfo(archive.string());
			std::cout << "Archive file: \"" << archiveInfo.filePath  << std::endl;
			std::cout << "Data offset: " << std::dec << archiveInfo.offsetInFile << " bytes" << std::endl;
			std::cout << "Size: " << std::dec << archiveInfo.size << " bytes" << std::endl;
			std::cout << "File version: " << std::dec << archiveInfo.fileVersion << std::endl;
			std::cout << "File format: " << std::hex << std::showbase << archiveInfo.formatFlags << std::endl;
			std::cout << "Bits: " << std::dec << static_cast<uint32_t>(archiveInfo.bits) << std::endl;
			std::cout << "Checksum: " << std::hex << std::showbase << archiveInfo.checksum << std::endl;
			std::cout << "------------------------------------------------------------------------" << std::endl;
			// dump resource information
			const auto resources = Res2h<uint64_t>::instance().getResourceInfo();
			for (uint32_t i = 0; i < resources.size(); ++i)
			{
				try
				{
					// read resource entry
					auto & entry = resources.at(i);
					// dump to console
					std::cout << "File #" << std::dec << i << " \"" << entry.filePath << "\"" << std::endl;
					std::cout << "Data offset: " << std::dec << entry.dataOffset << " bytes" << std::endl;
					std::cout << "Data size: " << std::dec << entry.dataSize << " bytes" << std::endl;
					std::cout << "Checksum: " << std::hex << std::showbase << entry.checksum << std::endl;
					if (!dontExtract)
					{
						// if the caller wants to dump data, do it
						try
						{
							auto file = Res2h<uint64_t>::instance().loadResource(entry.filePath);
							if (file.data && file.dataSize > 0)
							{
								// worked. now dump file data to disk. construct output path
								std::string filePath = entry.filePath;
								stdfs::path subPath = filePath.erase(0, 2);
								stdfs::path outPath = destination / subPath;
								if (createPaths)
								{
									stdfs::path dirPath = destination;
									for (auto sdIt = subPath.begin(); sdIt->filename() != subPath.filename(); ++sdIt)
									{
										// build output path with subdirectory
										dirPath /= *sdIt;
										// check if if exists
										if (!stdfs::exists(dirPath))
										{
											stdfs::create_directory(dirPath);
										}
									}
								}
								// try to open output file
								std::ofstream outStream;
								outStream.open(outPath.string(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
								if (outStream.is_open() && outStream.good())
								{
									// write data to disk
									outStream.write(reinterpret_cast<const char *>(file.data.get()), file.dataSize);
									// check if data has been written
									if (static_cast<uint64_t>(outStream.tellp()) != file.dataSize)
									{
										std::cout << "Failed to read all data for resource #" << std::dec << i << std::endl;
									}
									// close file
									outStream.close();
								}
								else
								{
									std::cout << "Failed to open file \"" << outPath.string() << "\" for writing!" << std::endl;
								}
							}
							else
							{
								std::cout << "Failed to get data for resource #" << i << " from archive!" << std::endl;
							}
						}
						catch (const Res2hException &e)
						{
							std::cout << "Error loading resource " << std::dec << i << " from archive - " << e.whatString() << std::endl;
						}
					} // if(!dontExtract)
				}
				catch (const Res2hException &e)
				{
					std::cout << "Error reading resource #" << std::dec << i << " - " << e.whatString() << std::endl;
				}
			}
			return true;
		}
		
		
			std::cout << "Failed to open archive " << archive << std::endl;
		
	}
	catch (Res2hException e)
	{
		std::cout << "Error: " << e.whatString() << std::endl;
	}
	return false;
}

// -----------------------------------------------------------------------------

int main(int argc, const char * argv[])
{
	printVersion();

	// check number of arguments and if all arguments can be read
	if (argc < 2 || !readArguments(argc, argv))
	{
		printUsage();
		return -1;
	}

	// check if the input path exist
	if (!stdfs::exists(inFilePath))
	{
		std::cout << "Error: Invalid input file \"" << inFilePath.string() << "\"!" << std::endl;
		return -2;
	}
	// check if argument 1 is a file
	if (stdfs::is_directory(inFilePath))
	{
		std::cout << "Error: Input must be a file!" << std::endl;
		return -2;
	}

	if (!informationOnly)
	{
		// check if argument 2 is a directory
		if (!stdfs::is_directory(outFilePath))
		{
			std::cout << "Error: Output must be a directory!" << std::endl;
			return -2;
		}
	}

	if (!dumpArchive(outFilePath, inFilePath, useFullPaths, informationOnly))
	{
		std::cout << "Failed to dump archive!" << std::endl;
		return -3;
	}

	// profit!!!
	std::cout << "res2hdump succeeded." << std::endl;

	return 0;
}
