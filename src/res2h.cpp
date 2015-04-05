#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <boost/filesystem.hpp>

#include "res2h.h"
#include "res2hutils.hpp"


struct FileData {
	boost::filesystem::path inPath;
	boost::filesystem::path outPath;
	std::string internalName;
	std::string dataVariableName;
	std::string sizeVariableName;
	size_t size;
};

bool beVerbose = false;
bool useRecursion = false;
bool useC = false;
bool createBinary = false;
bool appendFile = false;
bool combineResults = false;
boost::filesystem::path commonHeaderFilePath;
boost::filesystem::path utilitiesFilePath;
boost::filesystem::path inFilePath;
boost::filesystem::path outFilePath;

std::ofstream badOfStream; //we need this later as a default parameter...

//-----------------------------------------------------------------------------

//This is based on the example code found here: https://svn.boost.org/trac/boost/ticket/1976
//but changed to not return a trailing ".." when paths only differ in their file name.
//The function still seems to be missing in boost as of 1.54.0.
boost::filesystem::path naiveUncomplete(boost::filesystem::path const path, boost::filesystem::path const base)
{
    if (path.has_root_path()) {
        if (path.root_path() != base.root_path()) {
            return path;
        } else {
            return naiveUncomplete(path.relative_path(), base.relative_path());
        }
    } else {
        if (base.has_root_path()) {
            return path;
        } else {
            auto path_it = path.begin();
            auto base_it = base.begin();
            while ( path_it != path.end() && base_it != base.end() ) {
                if (*path_it != *base_it) break;
                ++path_it; ++base_it;
            }
            boost::filesystem::path result;
			//check if we're at the filename of the base path already
            if (*base_it != base.filename()) {
				//add trailing ".." from path to base, but only if we're not already at the filename of the base path
                for (; base_it != base.end() && *base_it != base.filename(); ++base_it) {
                    result /= "..";
                }
            }
            for (; path_it != path.end(); ++path_it) {
                result /= *path_it;
            }
            return result;
        }
    }
	return path;
}

bool makeCanonical(boost::filesystem::path & result, const boost::filesystem::path & path)
{
    //if we use canonical the file must exits, else we get an exception.
    try {
        result = boost::filesystem::canonical(path);
    }
    catch(...) {
        //an error occurred. this maybe because the file is not there yet. try without the file name
        try {
            result = boost::filesystem::canonical(boost::filesystem::path(path).remove_filename());
            //ok. this worked. add file name again
            result /= path.filename();
        }
        catch (...) {
            //hmm. didn't work. tell the user. at least the path should be there...
            std::cout << "The path \"" << boost::filesystem::path(path).remove_filename().string() << "\" couldn't be found. Please create it." << std::endl;
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------

void printVersion()
{
	std::cout << "res2h " << RES2H_VERSION_STRING << " - Load plain binary data and dump to a raw C/C++ array." << std::endl << std::endl;
}

void printUsage()
{
    std::cout << std::endl;
	std::cout << "Usage: res2h <infile/indir> <outfile/outdir> [options]" << std::endl;
	std::cout << "Valid options:" << std::endl;
	std::cout << "-s Recurse into subdirectories below indir." << std::endl;
	std::cout << "-c Use .c files and arrays for storing the data definitions, else" << std::endl << "    uses .cpp files and std::vector/std::map." << std::endl;
	std::cout << "-h <headerfile> Puts all declarations in a common \"headerfile\" using \"extern\"" << std::endl << "    and includes that header file in the source files." << std::endl;
	std::cout << "-u <sourcefile> Create utility functions and arrays in a .c/.cpp file." << std::endl << "    Only makes sense in combination with -h" << std::endl;
    std::cout << "-1 Combine all converted files into one big .c/.cpp file (use with -u)." << std::endl;
    std::cout << "-b Compile binary archive outfile containing all infile(s). For reading in your" << std::endl << "    software include res2hinterface.h/.c/.cpp (depending on -c) and consult the docs." << std::endl;
	std::cout << "-a Append infile to outfile. Can be used to append an archive to an executable." << std::endl;
	std::cout << "-v Be verbose." << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "res2h ./lenna.png ./resources/lenna_png.cpp (convert single file)" << std::endl;
    std::cout << "res2h ./data ./resources -s -h resources.h -u resources.cpp (convert directory)" << std::endl;
    std::cout << "res2h ./data ./resources/data.bin -b (convert directory to binary file)" << std::endl;
	std::cout << "res2h ./resources/data.bin ./program.exe -a (append archive to executable)" << std::endl;
}

bool readArguments(int argc, const char * argv[])
{
	bool pastFiles = false;
	for(int i = 1; i < argc; ++i) {
		//read argument from list
		std::string argument = argv[i];
		//check what it is
		if (argument == "-a") {
            if (!commonHeaderFilePath.empty() || !utilitiesFilePath.empty()) {
                std::cout << "Error: Option -a can not be combined with -h or -u!" << std::endl;
				return false;
            }
			else if (createBinary) {
                std::cout << "Error: Option -a can not be combined with -b!" << std::endl;
				return false;
			}
            else if (combineResults) {
                std::cout << "Error: Option -a can not be combined with -1!" << std::endl;
				return false;
			}
			appendFile = true;
			pastFiles = true;
		}
        else if (argument == "-1") {
            //-u must be used for this to work. check if specified
            for(int j = 1; j < argc; ++j) {
                //read argument from list
                std::string argument = argv[j];
                if (argument == "-u") {
                    combineResults = true;
                    pastFiles = true;
                    break;
                }
            }
            if (!combineResults) {
                //-u not specified. complain to user.
                std::cout << "Error: Option -1 has to be combined with -u!" << std::endl;
                return false;
            }
        }
        else if (argument == "-b") {
            if (!commonHeaderFilePath.empty() || !utilitiesFilePath.empty()) {
                std::cout << "Error: Option -b can not be combined with -h or -u!" << std::endl;
				return false;
            }
			else if (appendFile) {
                std::cout << "Error: Option -b can not be combined with -a!" << std::endl;
				return false;
			}
            else if (combineResults) {
                std::cout << "Warning: Creating binary archive. Option -1 ignored!" << std::endl;
				return false;
			}
            createBinary = true;
			pastFiles = true;
		}
		else if (argument == "-c") {
			useC = true;
			pastFiles = true;
		}
        else if (argument == "-s") {
			useRecursion = true;
			pastFiles = true;
		}
        else if (argument == "-v") {
			beVerbose = true;
			pastFiles = true;
		}
		else if (argument == "-h") {
            if (createBinary) {
                std::cout << "Error: Option -h can not be combined with -b!" << std::endl;
				return false;
            }
			else if (appendFile) {
                std::cout << "Error: Option -h can not be combined with -a!" << std::endl;
				return false;
			}
			//try getting next argument as header file name
			i++;
			if (i < argc && argv[i] != nullptr) {
                if (!makeCanonical(commonHeaderFilePath, boost::filesystem::path(argv[i]))) {
                    return false;
                }
			}
			else {
				std::cout << "Error: Option -h specified, but no file name found!" << std::endl;
				return false;
			}
			pastFiles = true;
		}
		else if (argument == "-u") {
            if (createBinary) {
                std::cout << "Error: Option -u can not be combined with -b!" << std::endl;
				return false;
            }
			else if (appendFile) {
                std::cout << "Error: Option -u can not be combined with -a!" << std::endl;
				return false;
			}
			//try getting next argument as utility file name
			i++;
			if (i < argc && argv[i] != nullptr) {
                if (!makeCanonical(utilitiesFilePath, boost::filesystem::path(argv[i]))) {
                    return false;
                }
			}
			else {
				std::cout << "Error: Option -u specified, but no file name found!" << std::endl;
				return false;
			}
			if (!utilitiesFilePath.empty() && commonHeaderFilePath.empty()) {
				std::cout << "Warning: -u does not make much sense without -h..." << std::endl;
			}
			pastFiles = true;
		}
		//none of the options was matched until here...
		else if (!pastFiles) {
			//if no files/directories have been found yet this is probably a file/directory
			if (inFilePath.empty()) {
                if (!makeCanonical(inFilePath, boost::filesystem::path(argument))) {
                    return false;
                }
			}
			else if (outFilePath.empty()) {
				if (!makeCanonical(outFilePath, boost::filesystem::path(argument))) {
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

std::vector<FileData> getFileDataFrom(const boost::filesystem::path & inPath, const boost::filesystem::path & outPath, const boost::filesystem::path & parentDir, const bool recurse)
{
	//get all files from directory
	std::vector<FileData> files;
	//check for infinite symlinks
	if(boost::filesystem::is_symlink(inPath)) {
		//check if the symlink points somewhere in the path. this would recurse
		if(inPath.string().find(boost::filesystem::canonical(inPath).string()) == 0) {
			std::cout << "Warning: Path " << inPath << " contains recursive symlink! Skipping." << std::endl;
			return files;
		}
	}
	//iterate through source directory searching for files
	const boost::filesystem::directory_iterator dirEnd;
	for (boost::filesystem::directory_iterator fileIt(inPath); fileIt != dirEnd; ++fileIt) {
		boost::filesystem::path filePath = (*fileIt).path();
		if (!boost::filesystem::is_directory(filePath)) {
			if (beVerbose) {
				std::cout << "Found input file " << filePath << std::endl;
			}
			//add file to list
			FileData temp;
			temp.inPath = filePath;
			//replace dots in file name with '_' and add a .c/.cpp extension
			std::string newFileName = filePath.filename().generic_string();
			std::replace(newFileName.begin(), newFileName.end(), '.', '_');
			if (useC) {
				newFileName.append(".c");
			}
			else {
				newFileName.append(".cpp");
			}
            //remove parent directory of file from path for internal name. This could surely be done in a safer way
            boost::filesystem::path subPath(filePath.generic_string().substr(parentDir.generic_string().size() + 1));
            //add a ":/" before the name to mark internal resources (Yes. Hello Qt!)
            temp.internalName = ":/" + subPath.generic_string();
            //add subdir below parent path to name to enable multiple files with the same name
            std::string subDirString(subPath.remove_filename().generic_string());
            if (!subDirString.empty()) {
                //replace dir separators by underscores
                std::replace(subDirString.begin(), subDirString.end(), '/', '_');
                //add in front of file name
                newFileName = subDirString + "_" + newFileName;
            }
            //build new output file name
            temp.outPath = outPath / newFileName;
			if (beVerbose) {
				std::cout << "Internal name will be \"" << temp.internalName << "\"" << std::endl;
				std::cout << "Output path is " << temp.outPath << std::endl;
			}
			//get file size
			try {
				temp.size = (size_t)boost::filesystem::file_size(filePath);
				if (beVerbose) {
					std::cout << "Size is " << temp.size << " bytes." << std::endl;
				}
			}
			catch(...) {
				std::cout << "Error: Failed to get size of " << filePath << "!" << std::endl;
				temp.size = 0;
			}
			//add file to list
			files.push_back(temp);
		}
	}
	//does the user want subdirectories?
	if (recurse) {
		//iterate through source directory again searching for directories
		for (boost::filesystem::directory_iterator dirIt(inPath); dirIt != dirEnd; ++dirIt) {
			boost::filesystem::path dirPath = (*dirIt).path();
			if (boost::filesystem::is_directory(dirPath)) {
				if (beVerbose) {
					std::cout << "Found subdirectory " << dirPath << std::endl;
				}
				//subdirectory found. recurse.
				std::vector<FileData> subFiles = getFileDataFrom(dirPath, outPath, parentDir, recurse);
				//add returned result to file list
				files.insert(files.end(), subFiles.cbegin(), subFiles.cend());
			}
		}
	}
	//return result
	return files;
}

bool convertFile(FileData & fileData, const boost::filesystem::path & commonHeaderPath, std::ofstream & outStream = badOfStream, bool addHeader = true)
{
	if (boost::filesystem::exists(fileData.inPath)) {
		//try to open the input file
		std::ifstream inStream;
		inStream.open(fileData.inPath.string(), std::ifstream::in | std::ifstream::binary);
		if (inStream.is_open() && inStream.good()) {
			if (beVerbose) {
				std::cout << "Converting input file " << fileData.inPath;
			}
			//try getting size of data
			inStream.seekg(0, std::ios::end);
			fileData.size = (size_t)inStream.tellg();
			inStream.seekg(0);
            //check if the caller passed and output stream and use that
            bool closeOutStream = false;
            if (!outStream.is_open() || !outStream.good()) {
                if (!fileData.outPath.empty()) {
                    //try opening the output stream. truncate it when it exists
                    outStream.open(fileData.outPath.string(), std::ofstream::out | std::ofstream::trunc);
                }
                else {
                    std::cout << "Error: No output stream passed, but output path for \"" << fileData.inPath.filename().string() << "\" is empty! Skipping." << std::endl;
                    return false;
                }
                closeOutStream = true;
            }
            //now write to stream
			if (outStream.is_open() && outStream.good()) {
                //check if caller want to add a header
                if (addHeader) {
                    //add message 
                    outStream << "//this file was auto-generated from \"" << fileData.inPath.filename().string() << "\" by res2h" << std::endl << std::endl;
                    //add header include
                    if (!commonHeaderPath.empty()) {
                        //common header path must be relative to destination directory
                        boost::filesystem::path relativeHeaderPath = naiveUncomplete(commonHeaderPath, fileData.outPath);
                        outStream << "#include \"" << relativeHeaderPath.generic_string() << "\"" << std::endl << std::endl;
                    }
                }
				//create names for variables
				fileData.dataVariableName = fileData.outPath.filename().stem().string() + "_data";
				fileData.sizeVariableName = fileData.outPath.filename().stem().string() + "_size";
				//add size and data variable
				outStream << "const size_t " << fileData.sizeVariableName << " = " << std::dec << fileData.size << ";" << std::endl;
				outStream << "const unsigned char " << fileData.dataVariableName << "[" << std::dec << fileData.size << "] = {" << std::endl;
				outStream << "    "; //first indent
				//now add content
				size_t breakCounter = 0;
				while (!inStream.eof()) {
					//read byte from source
					unsigned char dataByte;
					inStream.read((char *)&dataByte, 1);
					//check if we have actually read something
					if (inStream.gcount() != 1 || inStream.eof()) {
						//we failed to read. break the read loop and close the file.
						break;
					}
					//write to destination in hex with a width of 2 and '0' as padding
                    //we do not use showbase as it doesn't work with zero values
                    outStream << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)dataByte;
					//was this the last character?
					if (!inStream.eof() && fileData.size > (size_t)inStream.tellg()) {
						//no. add comma.
						outStream << ",";
						//add break after 10 bytes and add indent again
						if (++breakCounter % 10 == 0) {
							outStream << std::endl << "    ";
						}
					}
				}
				//close curly braces
				outStream << std::endl << "};" << std::endl << std::endl;
				//close files
                if (closeOutStream) {
				    outStream.close();
                }
				inStream.close();
				if (beVerbose) {
					std::cout << " - succeeded." << std::endl;
				}
				return true;
			}
			else {
				std::cout << "Error: Failed to open file \"" << fileData.outPath.string() << "\" for writing!" << std::endl;
				return false;
			}
		}
		else {
			std::cout << "Error: Failed to open file \"" << fileData.inPath.string() << "\" for reading!" << std::endl;
			return false;
		}
	}
	else {
		std::cout << "Error: File \"" << fileData.inPath.string() << "\" does not exist!" << std::endl;
	}
	return false;
}

bool createCommonHeader(const std::vector<FileData> & fileList, const boost::filesystem::path & commonHeaderPath, bool addUtilityFunctions = false, bool useCConstructs = false)
{
	//try opening the output file. truncate it when it exists
	std::ofstream outStream;
    outStream.open(commonHeaderPath.generic_string(), std::ofstream::out | std::ofstream::trunc);
	if (outStream.is_open() && outStream.good()) {
		if (beVerbose) {
			std::cout << std::endl << "Creating common header " << commonHeaderPath;
		}
		//add message
		outStream << "//this file was auto-generated by res2h" << std::endl << std::endl;
		//add #pragma to only include once
		outStream << "#pragma once" << std::endl << std::endl;
		//add includes for C++
		if (!useCConstructs) {
			outStream << "#include <string>" << std::endl;
			if (addUtilityFunctions) {
			    outStream << "#include <map>" << std::endl;
            }
            outStream << std::endl;
		}
		//add all files
		for (auto fdIt = fileList.cbegin(); fdIt != fileList.cend(); ++fdIt) {
			//add size and data variable
			outStream << "extern const size_t " << fdIt->sizeVariableName << ";" << std::endl;
			outStream << "extern const unsigned char " << fdIt->dataVariableName << "[];" << std::endl << std::endl;
		}
		//if we want utilities, add array
		if (addUtilityFunctions) {
			//add resource struct
			outStream << "struct Res2hEntry {" << std::endl;
			if (useCConstructs) {
				outStream << "    const char * relativeFileName;" << std::endl;
			}
			else {
				outStream << "    const std::string relativeFileName;" << std::endl;
			}
			outStream << "    const size_t size;" << std::endl;
			outStream << "    const unsigned char * data;" << std::endl;
			outStream << "};" << std::endl << std::endl;
			//add list holding files
            outStream << "extern const size_t res2hNrOfFiles;" << std::endl;
			outStream << "extern const Res2hEntry res2hFiles[];" << std::endl << std::endl;
			if (!useCConstructs) {
                //add additional std::map if C++
                outStream << "typedef const std::map<const std::string, const Res2hEntry> res2hMapType;" << std::endl;
				outStream << "extern res2hMapType res2hMap;" << std::endl;
			}
		}
		//close file
		outStream.close();
		if (beVerbose) {
			std::cout << " - succeeded." << std::endl;
		}
		return true;
	}
	else {
		std::cout << "Error: Failed to open file \"" << commonHeaderPath << "\" for writing!" << std::endl;
	}
	return true;
}

bool createUtilities(std::vector<FileData> & fileList, const boost::filesystem::path & utilitiesPath, const boost::filesystem::path & commonHeaderPath, bool useCConstructs = false, bool addFileData = false)
{
	//try opening the output file. truncate it when it exists
	std::ofstream outStream;
	outStream.open(utilitiesPath.generic_string(), std::ofstream::out | std::ofstream::trunc);
	if (outStream.is_open() && outStream.good()) {
		if (beVerbose) {
			std::cout << std::endl << "Creating utilities file " << utilitiesPath;
		}
		//add message
		outStream << "//this file was auto-generated by res2h" << std::endl << std::endl;
		//create path to include file RELATIVE to this file
		boost::filesystem::path relativePath = naiveUncomplete(commonHeaderPath, utilitiesPath);
		//include header file
		outStream << "#include \"" << relativePath.string() << "\"" << std::endl << std::endl;
        //if the data should go to this file too, add it
        if (addFileData) {
            for (auto fdIt = fileList.begin(); fdIt != fileList.cend(); ++fdIt) {
                if (!convertFile(*fdIt, commonHeaderFilePath, outStream, false)) {
                    std::cout << "Error: Failed to convert all files. Aborting!" << std::endl;
                    outStream.close();
                    return false;
                }
            }
        }
		//begin data arrays. switch depending wether C or C++
        outStream << "const size_t res2hNrOfFiles = " << fileList.size() << ";" << std::endl;
        //add files
        outStream << "const Res2hEntry res2hFiles[res2hNrOfFiles] = {" << std::endl;
        outStream << "    "; //first indent
        for (auto fdIt = fileList.cbegin(); fdIt != fileList.cend();) {
            outStream << "{\"" << fdIt->internalName << "\", " << fdIt->sizeVariableName << ", " << fdIt->dataVariableName << "}";
            //was this the last entry?
            ++fdIt;
            if (fdIt != fileList.cend()) {
                //no. add comma.
                outStream << ",";
                //add break after every entry and add indent again
                outStream << std::endl << "    ";
            }
        }
        outStream << std::endl << "};" << std::endl;
		if (!useCConstructs) {
			//add files to map
			outStream << std::endl << "res2hMapType::value_type mapTemp[] = {" << std::endl;
			outStream << "    ";
			for (auto fdIt = fileList.cbegin(); fdIt != fileList.cend();) {
				outStream << "std::make_pair(\"" << fdIt->internalName << "\", res2hFiles[" << (fdIt - fileList.cbegin()) << "])";
				//was this the last entry?
				++fdIt;
				if (fdIt != fileList.cend()) {
					//no. add comma.
					outStream << ",";
					//add break after every entry and add indent again
					outStream << std::endl << "    ";
				}
			}
			outStream << std::endl << "};" << std::endl << std::endl;
            //create map
            outStream << "res2hMapType res2hMap(mapTemp, mapTemp + sizeof mapTemp / sizeof mapTemp[0]);" << std::endl;
		}
		//close file
		outStream.close();
		if (beVerbose) {
			std::cout << " - succeeded." << std::endl;
		}
		return true;
	}
	else {
		std::cout << "Error: Failed to open file \"" << utilitiesPath << "\" for writing!" << std::endl;
	}
	return true;
}

//Blob file format:
//Offset         | Type     | Description
//---------------+----------+-------------------------------------------
//START          | char[8]  | magic number string "res2hbin"
//08             | uint32_t | file format version number (currently 1)
//12             | uint32_t | format flags or other crap for file (currently 0)
//16             | uint32_t | size of whole archive including checksum in bytes
//20             | uint32_t | number of directory and file entries following
//Then follows the directory:
//24 + 00        | uint32_t | file entry #0, size of internal name INCLUDING null-terminating character
//24 + 04        | char[]   | file entry #0, internal name (null-terminated)
//24 + 04 + name | uint32_t | file entry #0, format flags for entry (currently 0)
//24 + 08 + name | uint32_t | file entry #0, size of data
//24 + 12 + name | uint32_t | file entry #0, absolute offset of data in file
//24 + 16 + name | uint32_t | file entry #0, Adler-32 (RFC1950) checksum of data
//Then follow the other directory entries.
//Directly after the directory the data blocks begin.
//END - 04       | uint32_t | Adler-32 (RFC1950) checksum of whole file up to this point
//Obviously this limits you to ~4GB for the whole binary file and ~4GB per data entry. Go cry about it...
//There is some redundant information here, but that's for reading stuff faster.
//Also the version and dummy fields might be needed in later versions...
bool createBlob(const std::vector<FileData> & fileList, const boost::filesystem::path & filePath)
{
    //try opening the output file. truncate it when it exists
    std::fstream outStream;
    outStream.open(filePath.string(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if (outStream.is_open() && outStream.good()) {
		if (beVerbose) {
			std::cout << std::endl << "Creating binary archive " << filePath << std::endl;
		}
        //add magic number
        const unsigned char magicBytes[9] = RES2H_MAGIC_BYTES;
        outStream.write(reinterpret_cast<const char *>(&magicBytes), sizeof(magicBytes) - 1);
		//add version and format flag
		const uint32_t fileVersion = RES2H_ARCHIVE_VERSION;
		const uint32_t fileFlags = 0;
		outStream.write(reinterpret_cast<const char *>(&fileVersion), sizeof(uint32_t));
		outStream.write(reinterpret_cast<const char *>(&fileFlags), sizeof(uint32_t));
		//add dummy archive size
		uint32_t archiveSize = 0;
		outStream.write(reinterpret_cast<const char *>(&archiveSize), sizeof(uint32_t));
		//add number of directory entries
		const uint32_t nrOfEntries = fileList.size();
		outStream.write(reinterpret_cast<const char *>(&nrOfEntries), sizeof(uint32_t));
		//skip through files calculating data start offset behind directory
		size_t dataStart = RES2H_OFFSET_DIR_START;
		for (auto fdIt = fileList.cbegin(); fdIt != fileList.cend(); ++fdIt) {
			//calculate size of entry and to entry start adress
			dataStart += 20 + fdIt->internalName.size() + 1;
		}
        //add directory for all files
		for (auto fdIt = fileList.cbegin(); fdIt != fileList.cend(); ++fdIt) {
			//add size of name
			const uint32_t nameSize = fdIt->internalName.size() + 1;
			outStream.write(reinterpret_cast<const char *>(&nameSize), sizeof(uint32_t));
			//add name and null-termination
			outStream << fdIt->internalName << '\0';
			//add flags
			const uint32_t entryFlags = 0;
			outStream.write(reinterpret_cast<const char *>(&entryFlags), sizeof(uint32_t));
			//add data size
			outStream.write(reinterpret_cast<const char *>(&fdIt->size), sizeof(uint32_t));
			//add offset from file start to start of data
			outStream.write(reinterpret_cast<const char *>(&dataStart), sizeof(uint32_t));
			//add checksum of data
			const uint32_t checksum = calculateAdler32(fdIt->inPath.string());
			outStream.write(reinterpret_cast<const char *>(&checksum), sizeof(uint32_t));
			if (beVerbose) {
				std::cout << "Creating directory entry for \"" << fdIt->internalName << "\"" << std::endl;
				std::cout << "Size is " << fdIt->size << " bytes." << std::endl;
				std::cout << "Data starts at " << std::hex << std::showbase << dataStart << std::endl;
				std::cout << "Adler-32 checksum is " << std::hex << std::showbase << checksum << std::endl;
			}
			//now add size of this entrys data to start offset for next data block
			dataStart += fdIt->size;
		}
		//add data for all files		
		for (auto fdIt = fileList.cbegin(); fdIt != fileList.cend(); ++fdIt) {
			//try to open file
			std::ifstream inStream;
			inStream.open(fdIt->inPath.string(), std::ifstream::in | std::ifstream::binary);
			if (inStream.is_open() && inStream.good()) {
				if (beVerbose) {
					std::cout << "Adding data for \"" << fdIt->internalName << "\"" << std::endl;
				}
				std::streamsize overallDataSize = 0;
				//copy data from input to output file
				while (!inStream.eof() && inStream.good()) {
					unsigned char buffer[1024];
					std::streamsize readSize = sizeof(buffer);
					try {
						//try reading data from input file
						inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
					}
					catch (std::ios_base::failure) { /*ignore read failure. salvage what we can.*/ }
					//store how many bytes were actually read
					readSize = inStream.gcount();
					//write to output file
					outStream.write(reinterpret_cast<const char *>(&buffer), readSize);
					//increate size of overall data read
					overallDataSize += readSize;
				}
				//close input file
				inStream.close();
				//check if the file was completely read
				if (overallDataSize != fdIt->size) {
					std::cout << "Error: Failed to completely copy file \"" << fdIt->inPath.string() << "\" to binary data!" << std::endl;
					outStream.close();
					return false;
				}
			}
			else {
				std::cout << "Error: Failed to open file \"" << fdIt->inPath.string() << "\" for reading!" << std::endl;
				outStream.close();
				return false;
			}
		}
		//final archive size is current size + checksum. write size to the header now
		archiveSize = (uint32_t)outStream.tellg() + sizeof(uint32_t);
		outStream.seekg(RES2H_OFFSET_ARCHIVE_SIZE);
		outStream.write(reinterpret_cast<const char *>(&archiveSize), sizeof(uint32_t));
        //close file
        outStream.close();
		if (beVerbose) {
			std::cout << "Binary archive creation succeeded." << std::endl;
		}
		//calculate checksum of whole file
		const uint32_t adler32 = calculateAdler32(filePath.string());
		//open file again, move to end of file and append checksum
		outStream.open(filePath.string(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);
		if (outStream.is_open() && outStream.good()) {
			outStream.seekg(0, std::ios::end);
			outStream.write(reinterpret_cast<const char *>(&adler32), sizeof(uint32_t));
			//close file
			outStream.close();
		}
		else {
			std::cout << "Error: Failed to open file \"" << filePath.string() << "\" for writing!" << std::endl;
			return false;
		}
		if (beVerbose) {
			std::cout << "Archive checksum is " << std::hex << std::showbase << adler32 << "." << std::endl;
		}
        return true;
    }
    else {
        std::cout << "Error: Failed to open file \"" << filePath.string() << "\" for writing!" << std::endl;
        return false;
    }
	return false;
}

bool appendAtoB(const boost::filesystem::path & destinationPath, const boost::filesystem::path & sourcePath)
{
	//try opening the output file.
    std::fstream outStream;
    outStream.open(destinationPath.string(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);
    if (outStream.is_open() && outStream.good()) {
		if (beVerbose) {
			std::cout << std::endl << "Opened output file " << destinationPath << std::endl;
		}
		//seek to the end
		outStream.seekg(0, std::ios::end);
		//open input file
		std::ifstream inStream;
		inStream.open(sourcePath.string(), std::ifstream::in | std::ifstream::binary);
		if (inStream.is_open() && inStream.good()) {
			if (beVerbose) {
				std::cout << "Opened input file \"" << sourcePath << "\". Appending data to output." << std::endl;
			}
			//copy data from input to output file
			while (!inStream.eof() && inStream.good()) {
				unsigned char buffer[1024];
				std::streamsize readSize = sizeof(buffer);
				try {
					//try reading data from input file
					inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				}
				catch (std::ios_base::failure) { /*ignore read failure. salvage what we can.*/ }
				//store how many bytes were actually read
				readSize = inStream.gcount();
				//write to output file
				outStream.write(reinterpret_cast<const char *>(&buffer), readSize);
			}
			//close input file
			inStream.close();
		}
		else {
			std::cout << "Error: Failed to open input file \"" << sourcePath.string() << "\" for reading!" << std::endl;
			outStream.close();
			return false;
		}
		//close output file
		outStream.close();
		return true;
	}
	else {
		std::cout << "Error: Failed to open output file \"" << destinationPath.string() << "\" for writing!" << std::endl;
	}
	return false;
}

//-----------------------------------------------------------------------------

int main(int argc, const char * argv[])
{
	printVersion();

	//check number of arguments and if all arguments can be read
	if(argc < 3 || !readArguments(argc, argv)) {
		printUsage();
		return -1;
	}

	//check if the input path exist
	if (!boost::filesystem::exists(inFilePath)) {
		std::cout << "Error: Invalid input file/directory \"" << inFilePath.string() << "\"!" << std::endl;
		return -2;
	}

    if (createBinary) {
        //check if argument 2 is a file
        if (boost::filesystem::is_directory(outFilePath)) {
            std::cout << "Error: Output must be a file if -b is used!" << std::endl;
            return -2;
        }
    }
	else if (appendFile) {
		//check if argument 2 is a file
        if (boost::filesystem::is_directory(outFilePath)) {
            std::cout << "Error: Output must be a file if -a is used!" << std::endl;
            return -2;
        }
	}
    else if (boost::filesystem::is_directory(inFilePath) != boost::filesystem::is_directory(outFilePath)) {
        //check if output directory exists
        if (boost::filesystem::is_directory(outFilePath) && !boost::filesystem::exists(outFilePath)) {
            std::cout << "Error: Invalid output directory \"" << outFilePath.string() << "\"!" << std::endl;
            return -2;
        }
        //check if arguments 1 and 2 are both files or both directories
		std::cout << "Error: Input and output file must be both either a file or a directory!" << std::endl;
		return -2;
	}

	if (appendFile) {
		//append file a to b
		if (!appendAtoB(outFilePath, inFilePath)) {
			std::cout << "Error: Failed to append data to executable!" << std::endl;
			return -3;
		}
	}
	else {
		//build list of files to process
		std::vector<FileData> fileList;
		if (boost::filesystem::is_directory(inFilePath) && boost::filesystem::is_directory(inFilePath)) {
			//both files are directories, build file ist
			fileList = getFileDataFrom(inFilePath, outFilePath, inFilePath, useRecursion);
			if (fileList.empty()) {
				std::cout << "Error: No files to convert!" << std::endl;
				return -3;
			}
		}
		else {
			//just add single input/output file
			FileData temp;
			temp.inPath = inFilePath;
			temp.outPath = outFilePath;
			temp.internalName = inFilePath.filename().string(); //remove all, but the file name and extension
			if (beVerbose) {
				std::cout << "Found input file " << inFilePath << std::endl;
				std::cout << "Internal name will be \"" << temp.internalName << "\"" << std::endl;
				std::cout << "Output path is " << temp.outPath << std::endl;
			}
			//get file size
			try {
				temp.size = (size_t)boost::filesystem::file_size(inFilePath);
				if (beVerbose) {
					std::cout << "Size is " << temp.size << " bytes." << std::endl;
				}
			}
			catch(...) {
				std::cout << "Error: Failed to get size of " << inFilePath << "!" << std::endl;
				temp.size = 0;
			}
			fileList.push_back(temp);
		}

		//does the user want an binary file?
		if (createBinary) {
			//yes. build it.
			if (!createBlob(fileList, outFilePath)) {
				std::cout << "Error: Failed to convert to binary file!" << std::endl;
				return -4;
			}
		}
		else {
			//no. convert files to .c/.cpp. loop through list, converting files
			for (auto fdIt = fileList.begin(); fdIt != fileList.cend(); ++fdIt) {
				if (!convertFile(*fdIt, commonHeaderFilePath)) {
					std::cout << "Error: Failed to convert all files. Aborting!" << std::endl;
					return -4;
				}
			}
			//do we need to write a header file?
			if (!commonHeaderFilePath.empty()) {
				if (!createCommonHeader(fileList, commonHeaderFilePath, !utilitiesFilePath.empty(), useC)) {
					return -5;
				}
				//do we need to create utilities?
				if (!utilitiesFilePath.empty()) {
                    if (!createUtilities(fileList, utilitiesFilePath, commonHeaderFilePath, useC, combineResults)) {
						return -6;
					}
				}
			}
		}
	} //if (!appendFile) {

	//profit!!!
	std::cout << "res2h succeeded." << std::endl;
	
	return 0;
}
