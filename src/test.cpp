#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <boost/filesystem.hpp>

#include <stdlib.h> //for system()

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

#ifdef WIN32
    boost::filesystem::path inDir = "../test";
    boost::filesystem::path outDir = "../results";
    const boost::filesystem::path res2hPath = "..\\Release\\res2h.exe";
    const boost::filesystem::path res2hdumpPath = "..\\Release\\res2hdump.exe";
#else
    boost::filesystem::path inDir = "./test";
    boost::filesystem::path outDir = "./results";
    const boost::filesystem::path res2hPath = "./res2h";
    const boost::filesystem::path res2hdumpPath = "./res2hdump";
#endif

const boost::filesystem::path outFile = "test.bin";
const std::string res2hdumpOptions = "-f"; //dump using full paths
const std::string res2hOptions = "-s -b"; //recurse and build binary archive

//-----------------------------------------------------------------------------

void printHeader()
{
	std::cout << "res2h unit test " << RES2H_VERSION_STRING << std::endl;
    std::cout << "Reading all files from " << inDir << std::endl;
    std::cout << "and packing them to " << outDir / outFile << "." << std::endl;
    std::cout << "Then unpacking all files again and comparing binary data." << std::endl << std::endl;
}

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
			//add file to list
			FileData temp;
			temp.inPath = filePath;
			//replace dots in file name with '_' and add a .c/.cpp extension
			std::string newFileName = filePath.filename().generic_string();
/*			std::replace(newFileName.begin(), newFileName.end(), '.', '_');
			if (useC) {
				newFileName.append(".c");
			}
			else {
				newFileName.append(".cpp");
			}*/
            //remove parent directory of file from path for internal name. This could surely be done in a safer way
            boost::filesystem::path subPath(filePath.generic_string().substr(parentDir.generic_string().size() + 1));
            //add a ":/" before the name to mark internal resources (Yes. Hello Qt!)
            temp.internalName = ":/" + subPath.generic_string();
            //add subdir below parent path to name to enable multiple files with the same name
            std::string subDirString(subPath.remove_filename().generic_string());
            /*if (!subDirString.empty()) {
                //replace dir separators by underscores
                std::replace(subDirString.begin(), subDirString.end(), '/', '_');
                //add in front of file name
                newFileName = subDirString + "_" + newFileName;
            }*/
            //build new output file name
            temp.outPath = outPath / subDirString / newFileName;
			//get file size
			try {
				temp.size = (size_t)boost::filesystem::file_size(filePath);
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

bool compareAtoB(const boost::filesystem::path & a, const boost::filesystem::path & b)
{
	//try opening the first file.
    std::ifstream aStream;
    aStream.open(a.string(), std::ofstream::binary);
    if (aStream.is_open() && aStream.good()) {
		//open second file
		std::ifstream bStream;
        bStream.open(b.string(), std::ifstream::binary);
		if (bStream.is_open() && bStream.good()) {
			//copy data from input to output file
			while (!aStream.eof() && aStream.good()) {
				unsigned char bufferA[1024];
                unsigned char bufferB[1024];
				std::streamsize readSizeA = sizeof(bufferA);
                std::streamsize readSizeB = sizeof(bufferB);
				try {
					//try reading data from input file
					aStream.read(reinterpret_cast<char *>(&bufferA), sizeof(bufferA));
				}
				catch (std::ios_base::failure) { /*ignore read failure. salvage what we can.*/ }
				//store how many bytes were actually read
				readSizeA = aStream.gcount();
                //try reading second file
                try {
					//try reading data from input file
					bStream.read(reinterpret_cast<char *>(&bufferB), sizeof(bufferB));
				}
				catch (std::ios_base::failure) { /*ignore read failure. salvage what we can.*/ }
				//store how many bytes were actually read
				readSizeB = bStream.gcount();
                //check if size is the same
                if (readSizeA == readSizeB) {
                    //check if data block is the same
                    for (int i = 0; i < readSizeA; ++i) {
                        if (bufferA[i] != bufferB[i]) {
                            std::cout << "Error: File data does not match!" << std::endl;
                            aStream.close();
                            bStream.close();
                            return false;
                        }
                    }
                }
                else {
                    std::cout << "Error: Different file size!" << std::endl;
                    aStream.close();
                    bStream.close();
                    return false;
                }
			}
			//close first file
			aStream.close();
		}
		else {
			std::cout << "Error: Failed to open file " << b << " for reading!" << std::endl;
            aStream.close();
			return false;
		}
		//close second file
	    bStream.close();
		return true;
	}
	else {
		std::cout << "Error: Failed to open file " << a << " for reading!" << std::endl;
	}
	return false;
}

int main(int argc, const char * argv[])
{
	printHeader();

    std::cout << "TEST: Running in " << boost::filesystem::current_path() << std::endl;

    //remove all files in results directory
    std::cout << "TEST: Deleting and re-creating " << outDir << "." << std::endl;
    try {
        boost::filesystem::remove_all(outDir);
    }
    catch (boost::filesystem::filesystem_error e) {
        //directory was probably not there...
        std::cout << "Warning: " << e.what() << std::endl;
    }
    //and re-create the directory
    boost::filesystem::create_directory(outDir);

    //check if the input/output directories exist now
    try {
        inDir = boost::filesystem::canonical(inDir);
        outDir = boost::filesystem::canonical(outDir);
    }
    catch (boost::filesystem::filesystem_error e) {
        //directory was probably not there...
        std::cout << "Error: " << e.what() << std::endl;
        return -1;
    }

    //get all files from source directory
    std::vector<FileData> fileList = getFileDataFrom(inDir, outDir, inDir, true);

    std::stringstream command;
    //run res2h creating binary archive
    std::cout << "TEST: Running res2h to create binary archive." << std::endl << std::endl;    
    command << res2hPath.string() << " " << inDir << " " << (outDir / outFile) << " " << res2hOptions;
    if (system(command.str().c_str()) != 0) {
        //an error occured running res2h
        std::cout << "The call \"" << command.str() << "\" failed!" << std::endl;
        return -2;
    }

    command.str(std::string());
    //run res2hdump, unpacking all files
    std::cout << "TEST: Running res2hdump to unpack binary archive." << std::endl << std::endl;
    command << res2hdumpPath.string() << " " << (outDir / outFile) << " " << outDir << " " << res2hdumpOptions;
    if (system(command.str().c_str()) != 0) {
        //an error occured running res2h
        std::cout << "The call \"" << command.str() << "\" failed!" << std::endl;
        return -3;
    }

    //compare binary
    std::cout << std::endl << "TEST: Comparing files binary." << std::endl << std::endl;
    for (auto fdIt = fileList.begin(); fdIt != fileList.cend(); ++fdIt) {
        if (!compareAtoB(fdIt->inPath, fdIt->outPath)) {
            std::cout << "Error: Binary comparison of " << fdIt->inPath << " to " << fdIt->outPath << " failed!" << std::endl;
            return -4;
        }
    }

    std::cout << "unittest succeeded!" << std::endl;

    return 0;
}
