#pragma once

#include <string>
#include <vector>

namespace mm2 {

constexpr int kRecentFoldersMax = 10;

std::vector<std::string> LoadRecentFolders();
void SaveRecentFolders(const std::vector<std::string>& paths);
void RememberRecentFolder(const std::string& folderPath);
void ClearRecentFolders();

bool IsMm2DataFolder(const std::string& folderPath);
std::string RecentFolderLabel(const std::string& folderPath);

}  // namespace mm2
