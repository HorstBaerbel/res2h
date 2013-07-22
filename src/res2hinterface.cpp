#include "res2hinterface.hpp"


std::weak_ptr<Res2h> Res2h::res2hInstance;

std::shared_ptr<Res2h> & Res2h::getInstance()
{
	static std::shared_ptr<Res2h> sharedInstance = res2hInstance.lock();
	if (sharedInstance == nullptr) {
		sharedInstance.reset(new Res2h);
		res2hInstance = sharedInstance;
	}
	return sharedInstance;
}

bool Res2h::loadArchive(const std::string & archivePath)
{
	return false;
}

Res2h::ResourceEntry Res2h::loadFile(const std::string & filePath, bool keepInCache)
{
	ResourceEntry temp;
	return temp;
}

void Res2h::releaseCache()
{
}
