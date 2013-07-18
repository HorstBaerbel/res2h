#include <string>
#include <iostream>
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

bool useRecursion = false;
bool useC = false;
std::string commonHeaderFile;
std::string utilitiesFile;
std::string inFile;
std::string outFile;

//-----------------------------------------------------------------------------

//This is from: https://svn.boost.org/trac/boost/ticket/1976
//The function still seems to be missing in boost as of 1.53.0
boost::filesystem::path naive_uncomplete(boost::filesystem::path const path, boost::filesystem::path const base)
{
    if (path.has_root_path()) {
        if (path.root_path() != base.root_path()) {
            return path;
        } else {
            return naive_uncomplete(path.relative_path(), base.relative_path());
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
            for (; base_it != base.end(); ++base_it) {
                result /= "..";
            }
            for (; path_it != path.end(); ++path_it) {
                result /= *path_it;
            }
            return result;
        }
    }
	return path;
}

//-----------------------------------------------------------------------------

void printVersion()
{
	std::cout << "res2h " << PROGRAM_VERSION_STRING << " - Load plain binary data and dump to a raw C/C++ array." << std::endl << std::endl;
}

void printUsage()
{
	std::cout << "Usage: res2h <infile/directory> <outfile/directory> [options]" << std::endl << std::endl;
	std::cout << "If infile is a directory, outfile must be a directory too. All contained files will be converted." << std::endl;
	std::cout << "Valid options:" << std::endl;
	std::cout << "-s Recurse into subdirectories." << std::endl;
	std::cout << "-c Use .c files and arrays for storing the data definitions, else uses .cpp files." << std::endl;
	std::cout << "-h <headerfile> Puts all declarations in a common \"headerfile\" using \"extern\" and includes that header file in the source files." << std::endl;
	std::cout << "-u <sourcefile> Create utility functions and arrays in a .c/.cpp file. Only makes sense with -h" << std::endl;
}

bool readArguments(int argc, const char * argv[])
{
	bool pastFiles = false;
	for(int i = 0; i < argc; ++i) {
		//read argument from list
		std::string argument = argv[i];
		//check what it is
		if (argument == "-s") {
			useRecursion = true;
			pastFiles = true;
		}
		if (argument == "-c") {
			useC = true;
			pastFiles = true;
		}
		else if (argument == "-h") {
			//try getting next argument as header file name
			i++;
			if (i < argc && argv[i] != nullptr) {
				commonHeaderFile = argv[i];
			}
			else {
				std::cout << "Error: -h specified, but no file name found!" << std::endl;
				return false;
			}
			pastFiles = true;
		}
		else if (argument == "-u") {
			//try getting next argument as utility file name
			i++;
			if (i < argc && argv[i] != nullptr) {
				utilitiesFile = argv[i];
			}
			else {
				std::cout << "Error: -u specified, but no file name found!" << std::endl;
				return false;
			}
			if (!utilitiesFile.empty() && commonHeaderFile.empty()) {
				std::cout << "Warning: -u makes not much sense without -h..." << std::endl;
			}
			pastFiles = true;
		}
		else if (!pastFiles) {
			//if no files/directories have been found yet this is probably a file/directory
			if (inFile.empty()) {
				inFile = argument;
			}
			else if (outFile.empty()) {
				outFile = argument;
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
			std::cout << "Warning: Path \"" << inPath.string() << "\" contains recursive symlink! Skipping." << std::endl;
			return files;
		}
	}
	//iterate through source directory searching for files
	const boost::filesystem::directory_iterator dirEnd;
	boost::filesystem::directory_iterator fileIt(inPath);
	while (fileIt != dirEnd) {
		boost::filesystem::path filePath = (*fileIt).path();
		if (!boost::filesystem::is_directory(filePath)) {
			//add file to list
			FileData temp;
			temp.inPath = filePath;
			//replace dots in file name with '_' and add a .c/.cpp extension
			std::string newFileName = filePath.filename().string();
			std::replace(newFileName.begin(), newFileName.end(), '.', '_');
			if (useC) {
				newFileName.append(".c");
			}
			else {
				newFileName.append(".cpp");
			}
			temp.outPath = outPath / newFileName;
			//remove parent directory of file from path for internal name. This could surely be done in a safer way
			temp.internalName = filePath.string().substr(parentDir.string().size() + 1);
			temp.size = 0;
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

bool convertFile(FileData & fileData, const std::string & commonHeaderPath)
{
	if (boost::filesystem::exists(fileData.inPath)) {
		//try to open the input file
		std::ifstream inStream;
		inStream.open(fileData.inPath.string(), std::ios::in | std::ios::binary);
		if (inStream.is_open() && inStream.good()) {
			//try getting size of data
			inStream.seekg(0, std::ios::end);
			fileData.size = (size_t)inStream.tellg();
			inStream.seekg(0, std::ios::beg);
			//try opening the output file. truncate it when it exists
			std::ofstream outStream;
			outStream.open(fileData.outPath.string(), std::ios::out | std::ios::trunc);
			if (outStream.is_open() && outStream.good()) {
				//add message 
				outStream << "//this file was auto-generated from \"" << fileData.inPath.filename().string() << "\" by res2h" << std::endl << std::endl;
				//add header include
				if (!commonHeaderPath.empty()) {
					outStream << "#include \"" << commonHeaderPath << "\"" << std::endl << std::endl;
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
					//write to destination in hex
					outStream << std::hex << dataByte;
					//was this the last character?
					if (!inStream.eof()) {
						//no. add comma.
						outStream << ",";
						//add break after 10 bytes and add indent again
						if (breakCounter++ % 10 == 0) {
							outStream << std::endl << "    ";
						}
					}
				}
				//close curly braces
				outStream << std::endl << "};"<< std::endl;
				//close files
				outStream.close();
				inStream.close();
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

bool createCommonHeader(const std::vector<FileData> & fileList, const std::string & commonHeaderPath, bool addUtilityFunctions = false, bool useCConstructs = false)
{
	//try opening the output file. truncate it when it exists
	std::ofstream outStream;
	outStream.open(commonHeaderPath, std::ios::out | std::ios::trunc);
	if (outStream.is_open() && outStream.good()) {
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
				outStream << "extern const Res2hEntry res2hVector[];" << std::endl << std::endl;
			}
			else {
				outStream << "extern const std::vector<const Res2hEntry> res2hVector;" << std::endl;
				outStream << "extern const std::map<const std::string, const Res2hEntry> res2hMap;" << std::endl << std::endl;
			}
		}
		//close file
		outStream.close();
		return true;
	}
	else {
		std::cout << "Error: Failed to open file \"" << commonHeaderPath << "\" for writing!" << std::endl;
	}
	return true;
}

bool createUtilities(const std::vector<FileData> & fileList, const std::string & utilitiesPath, const std::string & commonHeaderPath, bool useCConstructs = false)
{
	//try opening the output file. truncate it when it exists
	std::ofstream outStream;
	outStream.open(utilitiesPath, std::ios::out | std::ios::trunc);
	if (outStream.is_open() && outStream.good()) {
		//add message
		outStream << "//this file was auto-generated by res2h" << std::endl << std::endl;
		//create path to include file RELATIVE to this file
		boost::filesystem::path relativePath = naive_uncomplete(commonHeaderPath, utilitiesPath);
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
			outStream << "};" << std::endl << std::endl;
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
			outStream << "};" << std::endl << std::endl;
			//add files to map
			outStream << "const std::map<const std::string, const Res2hEntry> res2hMap = {" << std::endl << std::endl;
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
			outStream << "};" << std::endl << std::endl;
		}
		//close file
		outStream.close();
		return true;
	}
	else {
		std::cout << "Error: Failed to open file \"" << utilitiesPath << "\" for writing!" << std::endl;
	}
	return true;
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

	//create file paths
	boost::filesystem::path inFilePath(inFile);
	boost::filesystem::path outFilePath(outFile);
	//check if they exist
	if (boost::filesystem::exists(inFilePath)) {
		std::cout << "Error: Invalid input file/directory \"" << inFile << "\"!" << std::endl;
		return -2;
	}
	if (boost::filesystem::exists(outFilePath)) {
		std::cout << "Error: Invalid output file/directory \"" << outFile << "\"!" << std::endl;
		return -2;
	}

	//check if arguments 1 and 2 are both files or both directories
	if (boost::filesystem::is_directory(inFilePath) != boost::filesystem::is_directory(inFilePath)) {
		std::cout << "Error: Input and output file must be both either a file or a directory!" << std::endl;
		return -2;
	}

	const bool useDirectories = boost::filesystem::is_directory(inFilePath) && boost::filesystem::is_directory(inFilePath);

	//build list of files to process
	std::vector<FileData> fileList;
	if (useDirectories) {
		fileList = getFileDataFrom(inFilePath, outFilePath, inFilePath, useRecursion);
	}
	else {
		FileData temp;
		temp.inPath = inFilePath;
		temp.outPath = outFilePath;
		temp.internalName = inFilePath.filename().string(); //remove all, but the file name and extension
		temp.size = 0;
		fileList.push_back(temp);
	}

	//loop through list, converting files
	std::vector<FileData>::iterator fdIt = fileList.begin();
	while (fdIt != fileList.cend()) {
		if (!convertFile(*fdIt, commonHeaderFile)) {
			std::cout << "Error: Failed to convert all files. Aborting!" << std::endl;
			return -3;
		}
		++fdIt;
	}

	//do we want to write a header file?
	if (!commonHeaderFile.empty()) {
		if (!createCommonHeader(fileList, commonHeaderFile, !utilitiesFile.empty(), useC)) {
			return -4;
		}
		//do we want to create utilities?
		if (!utilitiesFile.empty()) {
			if (!createUtilities(fileList, utilitiesFile, commonHeaderFile, useC)) {
				return -5;
			}
		}
	}
	
	return 0;
}

