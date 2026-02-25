/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include "testfs.h"

#include <cpr/cpr.h>
#include <minizip-ng/mz.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_zip.h>
#include <minizip-ng/mz_zip_rw.h>

#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {

/**
 * @brief Extracts all entries from a zip archive to the destination directory using minizip-ng.
 */
void extractAll(const std::string &zipPath, const std::string &destDir) {
    void *reader = mz_zip_reader_create();
    if (!reader)
        throw std::runtime_error("Failed to create zip reader");

    int32_t err = mz_zip_reader_open_file(reader, zipPath.c_str());
    if (err != MZ_OK) {
        mz_zip_reader_delete(&reader);
        throw std::runtime_error("Failed to open zip: " + zipPath);
    }

    err = mz_zip_reader_goto_first_entry(reader);
    while (err == MZ_OK) {
        mz_zip_file *fileInfo = nullptr;
        if (mz_zip_reader_entry_get_info(reader, &fileInfo) != MZ_OK)
            break;

        std::filesystem::path outPath = std::filesystem::path(destDir) / fileInfo->filename;

        if (mz_zip_reader_entry_is_dir(reader) == MZ_OK) {
            std::filesystem::create_directories(outPath);
        } else {
            std::filesystem::create_directories(outPath.parent_path());

            if (mz_zip_reader_entry_open(reader) == MZ_OK) {
                std::ofstream ofs(outPath.string(), std::ios::binary);
                char buf[8192];
                int32_t bytesRead;
                while ((bytesRead = mz_zip_reader_entry_read(reader, buf, sizeof(buf))) > 0) {
                    ofs.write(buf, bytesRead);
                }
                ofs.close();
                mz_zip_reader_entry_close(reader);
            }
        }

        err = mz_zip_reader_goto_next_entry(reader);
    }

    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
}

} // anonymous namespace

TestFS::TestFS(const std::string &testArchivePath, const std::string &baseTestFolder, bool setCurrentDirectory)
    : testArchivePath(testArchivePath), baseTestFolder(baseTestFolder), oldCurrentDirectory_(std::filesystem::current_path())
{
    // Generate a random test folder path
    testFolder = (std::filesystem::temp_directory_path() / baseTestFolder / randomString(16)).string();
    std::filesystem::create_directories(testFolder);

    if (!isLocalPath(testArchivePath))
    {
        // Handle remote URL
        auto tempPath = std::filesystem::temp_directory_path() / baseTestFolder / extractFileName(testArchivePath);
        if (!std::filesystem::exists(tempPath))
        {
            std::cout << "Downloading archive...\n";
            downloadTestAsset(testArchivePath, tempPath.string(), true);
        }
        else
        {
            std::cout << "Using cached archive...\n";
        }
        extractAll(tempPath.string(), testFolder);
    }
    else
    {
        extractAll(testArchivePath, testFolder);
    }

    std::cout << "Created test FS '" << testArchivePath << "' in '" << testFolder << "'\n";

    if (setCurrentDirectory)
    {
        std::filesystem::current_path(testFolder);
        std::cout << "Set current directory to '" << testFolder << "'\n";
    }
}

/**
 * @brief Destructor for TestFS. Cleans up the test folder and restores the previous working directory.
 */
TestFS::~TestFS()
{
    if (!oldCurrentDirectory_.empty())
    {
        std::filesystem::current_path(oldCurrentDirectory_);
        std::cout << "Restored current directory to '" << oldCurrentDirectory_.string() << "'\n";
    }

    try
    {
        std::filesystem::remove_all(testFolder);
        std::cout << "Deleted test folder '" << testFolder << "'\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error deleting test folder: " << e.what() << "\n";
    }
}

/**
 * @brief Clears the cache by deleting the base test folder and its contents.
 * @param baseTestFolder The base test folder to be cleared.
 */
void TestFS::clearCache(const std::string &baseTestFolder)
{
    auto folder = std::filesystem::temp_directory_path() / baseTestFolder;
    if (std::filesystem::exists(folder))
    {
        std::filesystem::remove_all(folder);
    }
}

/**
 * @brief Generates a random alphanumeric string.
 * @param length The length of the random string to generate.
 * @return A randomly generated string of the specified length.
 */
std::string TestFS::randomString(size_t length)
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i)
        result += charset[dist(gen)];
    return result;
}

/**
 * @brief Determines if the given path is a local file path.
 * @param path The path to evaluate.
 * @return True if the path is local, otherwise false.
 */
bool TestFS::isLocalPath(const std::string &path)
{
    return path.rfind("file:/", 0) == 0 || (path.rfind("http://", 0) != 0 && path.rfind("https://", 0) != 0 && path.rfind("ftp://", 0) != 0);
}

/**
 * @brief Extracts the file name from a given path.
 * @param path The full path to extract the file name from.
 * @return The file name extracted from the path.
 */
std::string TestFS::extractFileName(const std::string &path)
{
    size_t pos = path.find_last_of("/");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

/**
 * @brief Downloads a test asset from a URL to a specified destination.
 * @param url The URL of the file to download.
 * @param destination The destination path to save the downloaded file.
 * @param overwrite If true, overwrites the file if it already exists.
 * @return The path of the downloaded file.
 */
std::filesystem::path TestFS::downloadTestAsset(const std::string &url, const std::string &destination, bool overwrite)
{
    std::filesystem::path destPath = destination;

    if (std::filesystem::exists(destPath))
    {
        if (!overwrite)
        {
            return destPath;
        }
        else
        {
            std::filesystem::remove(destPath);
        }
    }

    std::ofstream ofs(destPath.string(), std::ios::binary);
    auto request = cpr::Download(ofs, cpr::Url{url}, cpr::VerifySsl(false));

    if (request.error)
        throw std::runtime_error("Failed to download " + url + " to " + destPath.string());

    return destPath;
}
