#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
	#include <experimental/filesystem>
	namespace FS_NAMESPACE = std::experimental::filesystem;
#elif defined(_MSC_VER)
	#include <filesystem>
	namespace FS_NAMESPACE = std::tr2::sys;
#endif

#include <stdlib.h> // for system()

#include "res2h.h"
#include "checksum.h"


struct FileData
{
	FS_NAMESPACE::path inPath;
	FS_NAMESPACE::path outPath;
	std::string internalName;
	std::string dataVariableName;
	std::string sizeVariableName;
	size_t size;
};

#ifdef WIN32
	FS_NAMESPACE::path inDir = "../test";
	FS_NAMESPACE::path outDir = "../results";
	#ifdef _DEBUG
		const FS_NAMESPACE::path res2hPath = "..\\Debug\\res2h.exe";
		const FS_NAMESPACE::path res2hdumpPath = "..\\Debug\\res2hdump.exe";
	#else
		const FS_NAMESPACE::path res2hPath = "..\\Release\\res2h.exe";
		const FS_NAMESPACE::path res2hdumpPath = "..\\Release\\res2hdump.exe";
	#endif
#else
	FS_NAMESPACE::path inDir = "./test";
	FS_NAMESPACE::path outDir = "./results";
	const FS_NAMESPACE::path res2hPath = "./res2h";
	const FS_NAMESPACE::path res2hdumpPath = "./res2hdump";
#endif

const FS_NAMESPACE::path outFile = "test.bin";
const std::string res2hdumpOptions = "-v -f"; // dump using full paths
const std::string res2hOptions = "-v -s -b"; // recurse and build binary archive

// -----------------------------------------------------------------------------

void printHeader()
{
	std::cout << "res2h unit test " << RES2H_VERSION_STRING << std::endl;
	std::cout << "Reading all files from " << inDir << std::endl;
	std::cout << "and packing them to " << outDir / outFile << "." << std::endl;
	std::cout << "Then unpacking all files again and comparing binary data." << std::endl << std::endl;
}

std::vector<FileData> getFileDataFrom(const FS_NAMESPACE::path & inPath, const FS_NAMESPACE::path & outPath, const FS_NAMESPACE::path & parentDir, const bool recurse)
{
	// get all files from directory
	std::vector<FileData> files;
	// check for infinite symlinks
	if (FS_NAMESPACE::is_symlink(inPath))
	{
		// check if the symlink points somewhere in the path. this would recurse
		if (inPath.string().find(FS_NAMESPACE::canonical(inPath).string()) == 0)
		{
			std::cout << "Warning: Path " << inPath << " contains recursive symlink! Skipping." << std::endl;
			return files;
		}
	}
	// iterate through source directory searching for files
	const FS_NAMESPACE::directory_iterator dirEnd;
	for (FS_NAMESPACE::directory_iterator fileIt(inPath); fileIt != dirEnd; ++fileIt)
	{
		FS_NAMESPACE::path filePath = (*fileIt).path();
		if (!FS_NAMESPACE::is_directory(filePath))
		{
			// add file to list
			FileData temp;
			temp.inPath = filePath;
			// replace dots in file name with '_' and add a .c/.cpp extension
			std::string newFileName = filePath.filename().generic_string();
			// remove parent directory of file from path for internal name. This could surely be done in a safer way
			FS_NAMESPACE::path subPath(filePath.generic_string().substr(parentDir.generic_string().size() + 1));
			// add a ":/" before the name to mark internal resources (Yes. Hello Qt!)
			temp.internalName = ":/" + subPath.generic_string();
			// add subdir below parent path to name to enable multiple files with the same name
			std::string subDirString(subPath.parent_path().generic_string());
			// build new output file name
			temp.outPath = outPath / subDirString / newFileName;
			// get file size
			try
			{
				temp.size = (size_t)FS_NAMESPACE::file_size(filePath);
			}
			catch (...)
			{
				std::cout << "Error: Failed to get size of " << filePath << "!" << std::endl;
				temp.size = 0;
			}
			// add file to list
			files.push_back(temp);
		}
	}
	// does the user want subdirectories?
	if (recurse)
	{
		// iterate through source directory again searching for directories
		for (FS_NAMESPACE::directory_iterator dirIt(inPath); dirIt != dirEnd; ++dirIt)
		{
			FS_NAMESPACE::path dirPath = (*dirIt).path();
			if (FS_NAMESPACE::is_directory(dirPath))
			{
				// subdirectory found. recurse.
				std::vector<FileData> subFiles = getFileDataFrom(dirPath, outPath, parentDir, recurse);
				// add returned result to file list
				files.insert(files.end(), subFiles.cbegin(), subFiles.cend());
			}
		}
	}
	// return result
	return files;
}

bool compareAtoB(const FS_NAMESPACE::path & a, const FS_NAMESPACE::path & b)
{
	// try opening the first file.
	std::ifstream aStream;
	aStream.open(a.string(), std::ofstream::binary);
	if (aStream.is_open() && aStream.good())
	{
		// open second file
		std::ifstream bStream;
		bStream.open(b.string(), std::ifstream::binary);
		if (bStream.is_open() && bStream.good())
		{
			// copy data from input to output file
			while (!aStream.eof() && aStream.good())
			{
				unsigned char bufferA[4096];
				unsigned char bufferB[4096];
				std::streamsize readSizeA = sizeof(bufferA);
				std::streamsize readSizeB = sizeof(bufferB);
				try
				{
					// try reading data from input file
					aStream.read(reinterpret_cast<char *>(&bufferA), sizeof(bufferA));
				}
				catch (std::ios_base::failure) { /*ignore read failure. salvage what we can.*/ }
				// store how many bytes were actually read
				readSizeA = aStream.gcount();
				// try reading second file
				try
				{
					// try reading data from input file
					bStream.read(reinterpret_cast<char *>(&bufferB), sizeof(bufferB));
				}
				catch (std::ios_base::failure) { /*ignore read failure. salvage what we can.*/ }
				// store how many bytes were actually read
				readSizeB = bStream.gcount();
				// check if size is the same
				if (readSizeA == readSizeB)
				{
					// check if data block is the same
					for (int i = 0; i < readSizeA; ++i)
					{
						if (bufferA[i] != bufferB[i])
						{
							std::cout << "Error: File data does not match!" << std::endl;
							aStream.close();
							bStream.close();
							return false;
						}
					}
				}
				else
				{
					std::cout << "Error: Different file size!" << std::endl;
					aStream.close();
					bStream.close();
					return false;
				}
			}
			// close first file
			aStream.close();
		}
		else
		{
			std::cout << "Error: Failed to open file " << b << " for reading!" << std::endl;
			aStream.close();
			return false;
		}
		// close second file
		bStream.close();
		return true;
	}
	else
	{
		std::cout << "Error: Failed to open file " << a << " for reading!" << std::endl;
	}
	return false;
}

int main(int argc, const char * argv[])
{
	printHeader();

	std::cout << "TEST: Running in " << FS_NAMESPACE::current_path() << std::endl;

	// remove all files in results directory
	std::cout << "TEST: Deleting and re-creating " << outDir << "." << std::endl;
	try
	{
		FS_NAMESPACE::remove_all(outDir);
	}
	catch (FS_NAMESPACE::filesystem_error e)
	{
		// directory was probably not there...
		std::cout << "Warning: " << e.what() << std::endl;
	}
	// and re-create the directory
	FS_NAMESPACE::create_directory(outDir);

	// check if the input/output directories exist now
	try
	{
		inDir = FS_NAMESPACE::canonical(inDir);
		outDir = FS_NAMESPACE::canonical(outDir);
	}
	catch (FS_NAMESPACE::filesystem_error e)
	{
		// directory was probably not there...
		std::cout << "Error: " << e.what() << std::endl;
		return -1;
	}

	// get all files from source directory
	std::vector<FileData> fileList = getFileDataFrom(inDir, outDir, inDir, true);

	std::stringstream command;
	// run res2h creating binary archive
	std::cout << "TEST: Running res2h to create binary archive." << std::endl << std::endl;
	command << res2hPath.string() << " " << inDir << " " << (outDir / outFile) << " " << res2hOptions;
	if (system(command.str().c_str()) != 0)
	{
		// an error occurred running res2h
		std::cout << "The call \"" << command.str() << "\" failed!" << std::endl;
		return -2;
	}

	command.str(std::string());
	// run res2hdump, unpacking all files
	std::cout << "TEST: Running res2hdump to unpack binary archive." << std::endl << std::endl;
	command << res2hdumpPath.string() << " " << (outDir / outFile) << " " << outDir << " " << res2hdumpOptions;
	if (system(command.str().c_str()) != 0)
	{
		// an error occurred running res2h
		std::cout << "The call \"" << command.str() << "\" failed!" << std::endl;
		return -3;
	}

	// compare binary
	std::cout << std::endl << "TEST: Comparing files binary." << std::endl << std::endl;
	for (auto fdIt = fileList.begin(); fdIt != fileList.cend(); ++fdIt)
	{
		if (!compareAtoB(fdIt->inPath, fdIt->outPath))
		{
			std::cout << "Error: Binary comparison of " << fdIt->inPath << " to " << fdIt->outPath << " failed!" << std::endl;
			return -4;
		}
	}

	std::cout << "unit test succeeded!" << std::endl;

	return 0;
}
