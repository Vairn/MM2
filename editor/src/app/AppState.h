#pragma once
#include <string>

namespace mm2 {

// Shared, cross-panel state. Sections own their own decoded data; this holds
// only the things that several sections need.
struct AppState {
    std::string dataDir;  // directory containing the .dat files
    std::string status = "Open a data folder to begin (File > Open Data Folder...).";
};

}  // namespace mm2
