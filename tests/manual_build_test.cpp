#include <iostream>
#include <filesystem>
#include "common/nxs.h"
namespace fs = std::filesystem;
int main(int argc, char* argv[]) {
    if (argc < 3) { std::cerr << "Usage: test_build <input.obj> <output.nxs>\n"; return 1; }
    std::cout << "Input: " << argv[1] << std::endl;
    std::cout << "Output: " << argv[2] << std::endl;
    std::cout << "Input exists: " << fs::exists(argv[1]) << std::endl;

    NexusBuildOptions opts;
    opts.ram_buffer_mb = 2000;
    opts.node_faces = 4096;
    opts.top_node_faces = 1024;
    opts.scaling = 0.5f;
    opts.disable_texcoords = true;

    char errMsg[512] = {};
    std::cout << "Calling nexusBuildEx..." << std::endl;
    NXSErr err = nexusBuildEx(argv[1], argv[2], opts, errMsg, sizeof(errMsg));
    std::cout << "Result: " << err << std::endl;
    if (err != NXSERR_NONE) { std::cerr << "Error: " << errMsg << std::endl; }
    return err;
}
