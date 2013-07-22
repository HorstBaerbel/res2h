#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <boost/filesystem.hpp>

#include "res2h.h"


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
boost::filesystem::path commonHeaderFilePath;
boost::filesystem::path utilitiesFilePath;
boost::filesystem::path inFilePath;
boost::filesystem::path outFilePath;

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
            typedef boost::filesystem::path::const_iterator path_iterator;
            path_iterator path_it = path.begin();
            path_iterator base_it = base.begin();
            while ( path_it != path.end() && base_it != base.end() ) {
                if (*path_it != *base_it) break;
                ++path_it; ++base_it;
            }
            boost::filesystem::path result;
            if (base_it->has_parent_path()) {
                for (; base_it != base.end(); ++base_it) {
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
	std::cout << "res2h " << PROGRAM_VERSION_STRING << " - Load plain binary data and dump to a raw C/C++ array." << std::endl << std::endl;
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
    std::cout << "-b Compile binary archive outfile containing all infile(s). For reading in your" << std::endl << "    software include res2hinterface.h/.c/.cpp (depending on -c) and consult the docs." << std::endl;
	std::cout << "-v Be verbose" << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "res2h ./lenna.png ./resources/lenna_png.cpp -c (convert single file)" << std::endl;
    std::cout << "res2h ./data ./resources -s -h resources.h -u resources.cpp (convert directory)" << std::endl;
    std::cout << "res2h ./data ./resources/data.bin -b (convert directory to binary file)" << std::endl;
}

bool readArguments(int argc, const char * argv[])
{
	bool pastFiles = false;
	for(int i = 1; i < argc; ++i) {
		//read argument from list
		std::string argument = argv[i];
		//check what it is
        if (argument == "-b") {
            if (!commonHeaderFilePath.empty() || !utilitiesFilePath.empty()) {
                std::cout << "Error: -b can not be combined with -h or -u!" << std::endl;
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
                std::cout << "Error: -h can not be combined with -b!" << std::endl;
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
				std::cout << "Error: -h specified, but no file name found!" << std::endl;
				return false;
			}
			pastFiles = true;
		}
		else if (argument == "-u") {
            if (createBinary) {
                std::cout << "Error: -u can not be combined with -b!" << std::endl;
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
				std::cout << "Error: -u specified, but no file name found!" << std::endl;
				return false;
			}
			if (!utilitiesFilePath.empty() && commonHeaderFilePath.empty()) {
				std::cout << "Warning: -u makes not much sense without -h..." << std::endl;
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
	boost::filesystem::directory_iterator fileIt(inPath);
	while (fileIt != dirEnd) {
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
		++fileIt;
	}
	//does the user want subdirectories?
	if (recurse) {
		//iterate through source directory again searching for directories
		boost::filesystem::directory_iterator dirIt(inPath);
		while (dirIt != dirEnd) {
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
            ++dirIt;
		}
	}
	//return result
	return files;
}

bool convertFile(FileData & fileData, const boost::filesystem::path & commonHeaderPath)
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
			//try opening the output file. truncate it when it exists
			std::ofstream outStream;
			outStream.open(fileData.outPath.string(), std::ofstream::out | std::ofstream::trunc);
			if (outStream.is_open() && outStream.good()) {
				//add message 
				outStream << "//this file was auto-generated from \"" << fileData.inPath.filename().string() << "\" by res2h" << std::endl << std::endl;
				//add header include
				if (!commonHeaderPath.empty()) {
                    //common header path must be relative to destination directory
                    boost::filesystem::path relativeHeaderPath = naiveUncomplete(commonHeaderPath, fileData.outPath);
                    outStream << "#include \"" << relativeHeaderPath.generic_string() << "\"" << std::endl << std::endl;
				}
				//create names for variables
				fileData.dataVariableName = fileData.outPath.filename().stem().string() + "_data";
				fileData.sizeVariableName = fileData.outPath.filename().stem().string() + "_size";
				//add size and data variable
				outStream << "const size_t " << fileData.sizeVariableName << " = " << fileData.size << ";" << std::endl;
				outStream << "const unsigned char " << fileData.dataVariableName << "[" << fileData.size << "] = {" << std::endl;
				outStream << "    "; //first indent
				//now add content
				size_t breakCounter = 0;
				while (!inStream.eof()) {
					//read byte from source
					unsigned char dataByte;
					inStream.read((char *)&dataByte, 1);
					//write to destination in hex with a width of 2 and '0' as padding
                    //we do not use showbase as it doesn't work with zero values
                    outStream << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)dataByte;
					//was this the last character?
					if (!inStream.eof()) {
						//no. add comma.
						outStream << ",";
						//add break after 10 bytes and add indent again
						if (++breakCounter % 10 == 0) {
							outStream << std::endl << "    ";
						}
					}
				}
				//close curly braces
				outStream << std::endl << "};"<< std::endl;
				//close files
				outStream.close();
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
			std::cout << "Creating common header " << commonHeaderPath;
		}
		//add message
		outStream << "//this file was auto-generated by res2h" << std::endl << std::endl;
		//add #pragma to only include once
		outStream << "#pragma once" << std::endl << std::endl;
		//add all files
		std::vector<FileData>::const_iterator fdIt = fileList.cbegin();
		while (fdIt != fileList.cend()) {
			//add size and data variable
			outStream << "extern const size_t " << fdIt->sizeVariableName << ";" << std::endl;
			outStream << "extern const unsigned char " << fdIt->dataVariableName << "[];" << std::endl << std::endl;
            ++fdIt;
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
			//add list declaration depending on wether C or C++
			if (useCConstructs) {
				outStream << "extern const size_t res2hVector_size;" << std::endl;
				outStream << "extern const Res2hEntry res2hVector[];" << std::endl;
			}
			else {
				outStream << "extern const std::vector<const Res2hEntry> res2hVector;" << std::endl;
				outStream << "extern const std::map<const std::string, const Res2hEntry> res2hMap;" << std::endl;
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

bool createUtilities(const std::vector<FileData> & fileList, const boost::filesystem::path & utilitiesPath, const boost::filesystem::path & commonHeaderPath, bool useCConstructs = false)
{
	//try opening the output file. truncate it when it exists
	std::ofstream outStream;
	outStream.open(utilitiesPath.generic_string(), std::ofstream::out | std::ofstream::trunc);
	if (outStream.is_open() && outStream.good()) {
		if (beVerbose) {
			std::cout << "Creating utilities file " << utilitiesPath;
		}
		//add message
		outStream << "//this file was auto-generated by res2h" << std::endl << std::endl;
		//create path to include file RELATIVE to this file
		boost::filesystem::path relativePath = naiveUncomplete(commonHeaderPath, utilitiesPath);
		//include header file
		outStream << "#include \"" << relativePath.string() << "\"" << std::endl << std::endl;
		//begin data arrays. switch depending wether C or C++
		if (useCConstructs) {
			//add size
			outStream << "const size_t res2hVector_size = " << fileList.size() << ";" << std::endl;
			//add files
			outStream << "const Res2hEntry res2hVector[res2hVector_size] = {" << std::endl;
			outStream << "    "; //first indent
			std::vector<FileData>::const_iterator fdIt = fileList.cbegin();
			while (fdIt != fileList.cend()) {
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
		}
		else {
			//add files to vector
			outStream << "const std::vector<const Res2hEntry> res2hVector = {" << std::endl;			
			outStream << "    "; //first indent
			std::vector<FileData>::const_iterator fdIt = fileList.cbegin();
			while (fdIt != fileList.cend()) {
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
			outStream << std::endl << "};" << std::endl << std::endl;
			//add files to map
			outStream << "const std::map<const std::string, const Res2hEntry> res2hMap = {" << std::endl;
			outStream << "    ";
			fdIt = fileList.cbegin();
			while (fdIt != fileList.cend()) {
				outStream << "std::pair<const std::string, const Res2hEntry>(\"" << fdIt->internalName << "\", {\"" << fdIt->internalName << "\", " << fdIt->sizeVariableName << ", " << fdIt->dataVariableName << "})";
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

/*
Create Adler-32 checksum from file. Builds checksum from start position till EOF.
\param[in] filePath Path to the file to build the checksum for.
\param[in] adler Optional. Adler checksum from last run if you're using more than one file.
\return Returns the Adler-32 checksum for the file stream or the initial checksum upon failure.
\note Based on the sample code here: https://tools.ietf.org/html/rfc1950. This is not as safe as CRC-32 (see here: https://en.wikipedia.org/wiki/Adler-32), but should be totally sufficient for us.
*/
uint32_t calculateAdler32(const boost::filesystem::path & filePath, uint32_t adler = 1)
{
	//open file
	std::ifstream inStream;
	inStream.open(filePath.string(), std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good()) {
		//build checksum
		uint32_t s1 = adler & 0xffff;
		uint32_t s2 = (adler >> 16) & 0xffff;
		//loop until EOF
		while (!inStream.eof() && inStream.good()) {
			char buffer[1024];
			std::streamsize readSize = sizeof(buffer);
			try {
				//try reading data from input file
				inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
			}
			catch (std::ios_base::failure) {
				//reading didn't work properly. store how many bytes were actually read
				readSize = inStream.gcount();
			}
			//calculate checksum for buffer
			for (std::streamsize n = 0; n < readSize; n++) {
				s1 = (s1 + buffer[n]) % 65521;
				s2 = (s2 + s1) % 65521;
			}
		}
		//close file
		inStream.close();
		//build final checksum
		return (s2 << 16) + s1;
	}
	else {
		std::cout << "Error: Failed to open file \"" << filePath.string() << "\" for reading!" << std::endl;
	}
	return adler;
}

//Blob file format:
//Offset         | Type     | Description
//---------------+----------+-------------------------------------------
//START          | char[8]  | magic number string "res2hbin"
//08             | uint32_t | file format version number (currently 1)
//12             | uint32_t | format flags or other crap for file (currently 0)
//16             | uint32_t | number of directory and file entries following
//Then follows the directory:
//20 + 00        | uint32_t | file entry #0, size of internal name INCLUDING null-terminating character
//20 + 04        | char[]   | file entry #0, internal name (null-terminated)
//20 + 04 + name | uint32_t | file entry #0, format flags for entry (currently 0)
//20 + 08 + name | uint32_t | file entry #0, size of data
//20 + 12 + name | uint32_t | file entry #0, absolute offset of data in file
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
			std::cout << "Creating binary archive " << filePath;
		}
        //add magic number
        const unsigned char magicBytes[8] = {'r', 'e', 's', '2', 'h', 'b', 'i', 'n'};
        outStream.write(reinterpret_cast<const char *>(&magicBytes), sizeof(magicBytes));
		//add version and format flag
		const uint32_t fileVersion = 1;
		const uint32_t fileFlags = 0;
		outStream.write(reinterpret_cast<const char *>(&fileVersion), sizeof(uint32_t));
		outStream.write(reinterpret_cast<const char *>(&fileFlags), sizeof(uint32_t));
		//add number of directory entries
		const uint32_t fileSize = fileList.size();
		outStream.write(reinterpret_cast<const char *>(&fileSize), sizeof(uint32_t));
		//skip through files calculating data start offset behind directory
		size_t dataStart = 20;
		std::vector<FileData>::const_iterator fdIt = fileList.cbegin();
		while (fdIt != fileList.cend()) {
			//calculate size of entry and to entry start adress
			dataStart += 16 + fdIt->internalName.size() + 1;
			++fdIt;
		}
        //add directory for all files
		fdIt = fileList.cbegin();
		while (fdIt != fileList.cend()) {
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
			//now add size of this entrys data to start offset for next data block
			dataStart += fdIt->size;
			++fdIt;
		}
		//add data for all files
		fdIt = fileList.cbegin();
		while (fdIt != fileList.cend()) {
			//try to open file
			std::ifstream inStream;
			inStream.open(fdIt->inPath.string(), std::ifstream::in | std::ifstream::binary);
			if (inStream.is_open() && inStream.good()) {
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
			++fdIt;
		}
        //close file
        outStream.close();
		if (beVerbose) {
			std::cout << " - succeeded." << std::endl;
		}
		//calculate checksum of whole file
		const uint32_t adler32 = calculateAdler32(filePath);
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
			std::cout << "Checksum is " << std::hex << std::showbase << adler32 << "." << std::endl;
		}
        return true;
    }
    else {
        std::cout << "Error: Failed to open file \"" << filePath.string() << "\" for writing!" << std::endl;
        return false;
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
        //check if argument 2 is a directory
        if (boost::filesystem::is_directory(outFilePath)) {
            std::cout << "Error: Output must be a file if -b is used!" << std::endl;
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
        std::vector<FileData>::iterator fdIt = fileList.begin();
        while (fdIt != fileList.cend()) {
            if (!convertFile(*fdIt, commonHeaderFilePath)) {
                std::cout << "Error: Failed to convert all files. Aborting!" << std::endl;
                return -4;
            }
            ++fdIt;
        }
        //do we need to write a header file?
        if (!commonHeaderFilePath.empty()) {
            if (!createCommonHeader(fileList, commonHeaderFilePath, !utilitiesFilePath.empty(), useC)) {
                return -5;
            }
            //do we need to create utilities?
            if (!utilitiesFilePath.empty()) {
                if (!createUtilities(fileList, utilitiesFilePath, commonHeaderFilePath, useC)) {
                    return -6;
                }
            }
        }
    }

	//profit!!!
	std::cout << "Succeeded." << std::endl;
	
	return 0;
}
