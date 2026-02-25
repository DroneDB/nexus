#ifndef NX_NEXUSFILE_IO_H
#define NX_NEXUSFILE_IO_H

#include "nexusfile.h"
#include "mmap_file.h"
#include <string>

namespace nx {
	class NexusFileIO
		: public NexusFile {
	private:
		MappedFile file_;
		std::string filename_;
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

#endif // NX_NEXUSFILE_IO_H
