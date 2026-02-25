#include "nexusfile_io.h"

#include <fstream>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace nx {

void NexusFileIO::setFileName(const char* uri) {
	filename_ = uri;
	file_.setFileName(uri);
}

const char *NexusFileIO::fileName() {
	return filename_.c_str();
}

bool NexusFileIO::open(OpenMode openmode) {
	int mode = 0;
	if (openmode & OpenMode::Read)   mode |= MappedFile::ReadOnly;
	if (openmode & OpenMode::Write)  mode |= MappedFile::WriteOnly;
	// Append is handled by seeking to end after open
	return file_.open(mode);
}

long long int NexusFileIO::read(char* where, size_t length) {
	return file_.read(where, length);
}

long long int NexusFileIO::write(char* from, size_t length) {
	return file_.write(from, length);
}

size_t NexusFileIO::size() {
	return static_cast<size_t>(file_.size());
}

void* NexusFileIO::map(size_t from, size_t sz) {
	return file_.map(from, sz);
}

bool NexusFileIO::unmap(void* mapped) {
	return file_.unmap(static_cast<unsigned char*>(mapped));
}

bool NexusFileIO::seek(size_t to) {
	return file_.seek(to);
}

char *NexusFileIO::loadDZNode(uint32_t n) {
	if (filename_.size() < 5)
		return nullptr;
	std::string basename = filename_.substr(0, filename_.size() - 4) + "_files";
	std::string nodefile = basename + "/" + std::to_string(n) + ".nxn";
	return readDZFile(nodefile);
}

void NexusFileIO::dropDZNode(char *data) {
	delete[] data;
}

char *NexusFileIO::loadDZTex(uint32_t n) {
	if (filename_.size() < 5)
		return nullptr;
	std::string basename = filename_.substr(0, filename_.size() - 4) + "_files";
	std::string texfile = basename + "/" + std::to_string(n) + ".jpg";
	return readDZFile(texfile);
}

void NexusFileIO::dropDZTex(char *data) {
	delete[] data;
}

char *NexusFileIO::readDZFile(const std::string &path) {
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f.is_open())
		return nullptr;
	auto fsize = f.tellg();
	if (fsize <= 0)
		return nullptr;
	f.seekg(0);
	char *buf = new char[static_cast<size_t>(fsize)];
	f.read(buf, fsize);
	if (!f) {
		delete[] buf;
		return nullptr;
	}
	return buf;
}

} // namespace nx
