/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include "testarea.h"

#include <algorithm>
#include <cpr/cpr.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

TestArea::TestArea(const std::string& name, bool recreateIfExists) : name(name) {
    if (name.find("..") != std::string::npos)
        throw std::invalid_argument("Cannot use .. in name");

    const fs::path root = fs::temp_directory_path() / "nexus_test_areas" / fs::path(name);

    if (recreateIfExists) {
        if (fs::exists(root)) {
            std::error_code ec;
            fs::remove_all(root, ec);
            if (ec) {
                // Try again with a small delay (Windows file locking issues)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                fs::remove_all(root, ec);
            }
        }
    }
}

fs::path TestArea::getPath(const fs::path& p) {
    const fs::path root = fs::temp_directory_path() / "nexus_test_areas" / fs::path(name);
    if (!fs::exists(root)) {
        fs::create_directories(root);
    }
    return root / p;
}

fs::path TestArea::getFolder(const fs::path& subfolder) {
    const fs::path root = fs::temp_directory_path() / "nexus_test_areas" / fs::path(name);
    auto dir = root;
    if (!subfolder.empty())
        dir = dir / subfolder;

    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
    return dir;
}

fs::path TestArea::downloadTestAsset(const std::string& url,
                                     const std::string& filename,
                                     bool overwrite) {
    fs::path destination = getFolder() / fs::path(filename);

    if (fs::exists(destination)) {
        // Re-download if file is empty (0 bytes)
        if (fs::file_size(destination) == 0) {
            overwrite = true;
        }

        if (!overwrite)
            return destination;
        else
            fs::remove(destination);
    }

    // Retry with exponential backoff: 1s, 2s, 4s
    const int maxRetries = 3;
    const int baseDelayMs = 1000;

    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        std::ofstream ofs(destination.string(), std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Cannot create file: " + destination.string());
        }

        auto request = cpr::Download(ofs, cpr::Url{url}, cpr::VerifySsl(false));
        ofs.close();

        if (request.error) {
            if (fs::exists(destination)) fs::remove(destination);

            if (attempt < maxRetries) {
                int delayMs = baseDelayMs * (1 << (attempt - 1));
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                continue;
            }
            throw std::runtime_error("Failed to download " + url + " to " + destination.string() +
                                     " after " + std::to_string(maxRetries) + " attempts: " +
                                     request.error.message);
        }

        if (request.status_code != 200) {
            if (fs::exists(destination)) fs::remove(destination);

            if (attempt < maxRetries) {
                int delayMs = baseDelayMs * (1 << (attempt - 1));
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                continue;
            }
            throw std::runtime_error("Failed to download " + url + ": HTTP " +
                                     std::to_string(request.status_code));
        }

        return destination;
    }

    throw std::runtime_error("Failed to download " + url + " after " +
                             std::to_string(maxRetries) + " attempts");
}

void TestArea::clearAll() {
    const fs::path testAreasRoot = fs::temp_directory_path() / "nexus_test_areas";
    if (!fs::exists(testAreasRoot)) {
        std::cout << "No test areas to clear\n";
        return;
    }

    auto removedCount = fs::remove_all(testAreasRoot);
    std::cout << "Cleared all test areas (" << removedCount << " items removed)\n";
}
