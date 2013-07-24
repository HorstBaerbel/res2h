#include <boost/filesystem.hpp>

#include "res2hinterface.hpp"


bool beVerbose = false;
bool useFullPaths = false;
bool informationOnly = false;
boost::filesystem::path inFilePath;
boost::filesystem::path outFilePath;


//-----------------------------------------------------------------------------

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
		if (argument == "-i") {
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

bool dumpArchive(boost::filesystem::path & destination, boost::filesystem::path & archive, bool createPaths = true, bool dontExtract = false)
{
	return true;
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
	if (!boost::filesystem::exists(inFilePath)) {
		std::cout << "Error: Invalid input file \"" << inFilePath.string() << "\"!" << std::endl;
		return -2;
	}
	//check if argument 1 is a file
	if (!boost::filesystem::is_directory(inFilePath)) {
		std::cout << "Error: Input must be a file!" << std::endl;
		return -2;
	}
	
	if (!informationOnly) {
		//check if argument 2 is a directory
		if (!boost::filesystem::is_directory(outFilePath)) {
			std::cout << "Error: Output must be a directory!" << std::endl;
			return -2;
		}
	}

	if (!dumpArchive(outFilePath, inFilePath, useFullPaths, informationOnly)) {
			std::cout << "Failed to dump archive!" << std::endl;
			return -3;
	}

	//profit!!!
	std::cout << "Succeeded." << std::endl;
	
	return 0;
}
