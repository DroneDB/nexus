#ifndef NX_STDNEXUSFILE_H
#define NX_STDNEXUSFILE_H

#include <string>

#include "mappedfile.h"
#include "nexusfile.h"
#include "textfile.h"

namespace nx {
	//A Nexus file on the local disk, opened read only: read sequentially, or mapped into memory.
	class StdNexusFile
		: public NexusFile {
	private:
		InputFile file;
		MappedFile mapping;   //opened by the first map()
		std::string name;     //UTF-8
		char *readDZFile(const std::string &path);
	public:
		void setFileName(const char* uri) override;
		const char *fileName() override;
		bool open(OpenMode openmode) override;
		long long int read(char* where, size_t length) override;
		long long int write(char* from, size_t length) override;
		size_t size() override;
		void* map(size_t from, size_t size) override;
		bool unmap(void* mapped) override;
		bool seek(size_t to) override;

		char *loadDZNode(uint32_t n) override;
		void dropDZNode(char *data) override;
		char *loadDZTex(uint32_t n) override;
		void dropDZTex(char *data) override;

	};
}

#endif // NX_STDNEXUSFILE_H
