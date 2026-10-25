#include "stdnexusfile.h"

#include <filesystem>

#include "pathutil.h"

namespace nx {
	void StdNexusFile::setFileName(const char* uri)
	{
		name = uri;
	}
	const char *StdNexusFile::fileName() {
		return name.c_str();
	}

	//only reading: the files are written by the builder and the extractor
	bool StdNexusFile::open(OpenMode openmode)
	{
		if (openmode != OpenMode::Read)
			return false;
		return file.open(name);
	}

	long long int StdNexusFile::read(char* where, size_t length)
	{
		return file.read(where, length);
	}


	long long int StdNexusFile::write(char*, size_t)
	{
		return -1;
	}

	size_t StdNexusFile::size()
	{
		std::error_code ec;
		const auto size = std::filesystem::file_size(nativePath(name), ec);
		return ec ? 0 : size_t(size);
	}

	void* StdNexusFile::map(size_t from, size_t size)
	{
		if (!mapping.isOpen()) {
			try {
				mapping.openReadOnly(name);
			} catch (const Error &) {
				return nullptr;
			}
		}
		return mapping.map(from, size);
	}

	bool StdNexusFile::unmap(void* mapped)
	{
		return mapping.unmap(mapped);
	}

	bool StdNexusFile::seek(size_t to)
	{
		file.seek(to);
		return true;
	}

	//the companion folder "<base>_files" the saver produces, base being the name without its
	//four-character extension
	char *StdNexusFile::loadDZNode(uint32_t n) {
		if(name.size() < 5)
			return nullptr;
		const std::string basename = name.substr(0, name.size() - 4) + "_files";
		return readDZFile(basename + "/" + std::to_string(n) + ".nxn");
	}

	void StdNexusFile::dropDZNode(char *data) {
		delete [] data;
	}

	char *StdNexusFile::loadDZTex(uint32_t n) {
		if(name.size() < 5)
			return nullptr;
		const std::string basename = name.substr(0, name.size() - 4) + "_files";
		return readDZFile(basename + "/" + std::to_string(n) + ".jpg");
	}

	void StdNexusFile::dropDZTex(char *data) {
		delete [] data;
	}

	char *StdNexusFile::readDZFile(const std::string &path) {
		std::error_code ec;
		const auto size = std::filesystem::file_size(nativePath(path), ec);
		InputFile f;
		if(ec || !f.open(path))
			return nullptr;
		char *buf = new char[size_t(size)];
		f.read(buf, size_t(size));
		return buf;
	}

}
