#include <iostream>
#include <fstream>

#if defined(__GNUC__) || defined(__clang__)
	#include <experimental/filesystem>
	namespace FS_NAMESPACE = std::experimental::filesystem;
#elif defined(_MSC_VER)
	#include <filesystem>
	namespace FS_NAMESPACE = std::tr2::sys;
#endif

#include "res2hinterface.hpp"
#include "checksum.hpp"


bool beVerbose = false;
bool useFullPaths = false;
bool informationOnly = false;
FS_NAMESPACE::path inFilePath;
FS_NAMESPACE::path outFilePath;


//-----------------------------------------------------------------------------

bool makeCanonical(FS_NAMESPACE::path & result, const FS_NAMESPACE::path & path)
{
	//if we use canonical the file must exits, else we get an exception.
	try {
		result = FS_NAMESPACE::canonical(path);
	}
	catch (...) {
		//an error occurred. this maybe because the file is not there yet. try without the file name
		try {
			result = FS_NAMESPACE::canonical(FS_NAMESPACE::path(path).remove_filename());
			//ok. this worked. add file name again
			result /= path.filename();
		}
		catch (...) {
			//hmm. didn't work. tell the user. at least the path should be there...
			std::cout << "The path \"" << FS_NAMESPACE::path(path).remove_filename().string() << "\" couldn't be found. Please create it." << std::endl;
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------

void printVersion()
{
	std::cout << "res2hdump " << RES2H_VERSION_STRING << " - Dump data from a res2h archive file or embedded archive." << std::endl << std::endl;
}

void printUsage()
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
	for(int i = 1; i < argc; ++i) {
		//read argument from list
		std::string argument = argv[i];
		//check what it is
		if (argument == "-f") {
			useFullPaths = true;
			pastFiles = true;
		}
		else if (argument == "-i") {
			informationOnly = true;
			pastFiles = true;
		}
		else if (argument == "-v") {
			beVerbose = true;
			pastFiles = true;
		}
		//none of the options was matched until here...
		else if (!pastFiles) {
			//if no files/directories have been found yet this is probably a file/directory
			if (inFilePath.empty()) {
				if (!makeCanonical(inFilePath, FS_NAMESPACE::path(argument))) {
					return false;
				}
			}
			else if (outFilePath.empty()) {
				if (!makeCanonical(outFilePath, FS_NAMESPACE::path(argument))) {
					return false;
				}
				pastFiles = true;
			}
		}
		else {
			std::cout << "Error: Unknown argument \"" << argument << "\"!" << std::endl;
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------

bool dumpArchive(FS_NAMESPACE::path & destination, FS_NAMESPACE::path & archive, bool createPaths = true, bool dontExtract = false)
{
	 try {
		  if (Res2h<uint64_t>::getInstance().loadArchive(archive.string())) {
				//dump archive
				for (uint32_t i = 0; i < Res2h<uint64_t>::getInstance().getNrOfResources(); ++i) {
					 try {
						  //read resource entry
						  auto entry = Res2h<uint64_t>::getInstance().getResource(i);
						  //dump to console
						  std::cout << "File #" << std::dec << i << " \"" << entry.filePath << "\"" << std::endl;
						  std::cout << "Archive file: \"" << entry.archivePath << "\", archive offset: " << std::dec << entry.offsetInFile << " bytes" << std::endl;
						  std::cout << "Data offset: " << std::dec << entry.dataOffset << " bytes" << std::endl;
						  std::cout << "Data size: " << std::dec << entry.dataSize << " bytes" << std::endl;
						  std::cout << "Checksum: " << std::hex << std::showbase << entry.checksum << std::endl;					
						  if (!dontExtract) {
								//if the caller wants to dump data, do it
								try {
									 auto file = Res2h<uint64_t>::getInstance().loadFile(entry.filePath);
									 if (file.data && file.dataSize > 0) {
										  //worked. now dump file data to disk. construct output path
										  FS_NAMESPACE::path subPath = entry.filePath.erase(0, 2);
										  FS_NAMESPACE::path outPath = destination / subPath;
										  if (createPaths) {
												FS_NAMESPACE::path dirPath = destination;
												for (auto sdIt = subPath.begin(); sdIt->filename() != subPath.filename(); ++sdIt) {
													 //build output path with subdirectory
													 dirPath /= *sdIt;
													 //check if if exists
													 if (!FS_NAMESPACE::exists(dirPath)) {
														  FS_NAMESPACE::create_directory(dirPath);
													 }
												}
										  }
										  //try to open output file
										  std::ofstream outStream;
										  outStream.open(outPath.string(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
										  if (outStream.is_open() && outStream.good()) {
												//write data to disk
												outStream.write(reinterpret_cast<const char *>(file.data.get()), file.dataSize);
												//check if data has been written
												if (static_cast<uint64_t>(outStream.tellp()) != file.dataSize) {
													 std::cout << "Failed to read all data for resource #" << std::dec << i << std::endl;
												}
												//close file
												outStream.close();
										  }
										  else {
												std::cout << "Failed to open file \"" << outPath.string() << "\" for writing!" << std::endl;
										  }
									 }
									 else {
										  std::cout << "Failed to get data for resource #" << i << " from archive!" << std::endl;
									 }
								}
								catch (Res2hException e) {
									 std::cout << "Error loading resource " << std::dec << i << " from archive - " << e.whatString() << std::endl;
								}
						  } //if(!dontExtract)
					 }
					 catch (Res2hException e) {
						  std::cout << "Error reading resource #" << std::dec << i << " - " << e.whatString() << std::endl;
					 }
				}
				return true;
		  }
		  else {
				std::cout << "Failed to open archive " << archive << std::endl;
		  }
	 }
	 catch (Res2hException e) {
		  std::cout << "Error: " << e.whatString() << std::endl;
	 }
	 return false;
}

//-----------------------------------------------------------------------------

int main(int argc, const char * argv[])
{
	printVersion();

	//check number of arguments and if all arguments can be read
	if(argc < 2 || !readArguments(argc, argv)) {
		printUsage();
		return -1;
	}

	//check if the input path exist
	if (!FS_NAMESPACE::exists(inFilePath)) {
		std::cout << "Error: Invalid input file \"" << inFilePath.string() << "\"!" << std::endl;
		return -2;
	}
	//check if argument 1 is a file
	if (FS_NAMESPACE::is_directory(inFilePath)) {
		std::cout << "Error: Input must be a file!" << std::endl;
		return -2;
	}
	
	if (!informationOnly) {
		//check if argument 2 is a directory
		if (!FS_NAMESPACE::is_directory(outFilePath)) {
			std::cout << "Error: Output must be a directory!" << std::endl;
			return -2;
		}
	}

	if (!dumpArchive(outFilePath, inFilePath, useFullPaths, informationOnly)) {
			std::cout << "Failed to dump archive!" << std::endl;
			return -3;
	}

	//profit!!!
	std::cout << "res2hdump succeeded." << std::endl;
	
	return 0;
}
