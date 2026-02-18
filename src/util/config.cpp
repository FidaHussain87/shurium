// SHURIUM - Configuration File Parser Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include "shurium/util/config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace shurium {
namespace util {

// ============================================================================
// ConfigEntry Implementation
// ============================================================================

bool ConfigEntry::IsTrue() const {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "yes" || lower == "on" || lower == "1";
}

bool ConfigEntry::IsFalse() const {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "false" || lower == "no" || lower == "off" || lower == "0";
}

// ============================================================================
// ConfigManager Implementation
// ============================================================================

ConfigManager::ConfigManager() = default;

ConfigManager::~ConfigManager() = default;

// ============================================================================
// Static Helper Functions
// ============================================================================

std::string ConfigManager::Trim(const std::string& str) {
    const char* whitespace = " \t\r\n";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

std::string ConfigManager::Unquote(const std::string& str) {
    if (str.length() < 2) {
        return str;
    }
    
    char first = str.front();
    char last = str.back();
    
    // Check for matching quotes
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        std::string result = str.substr(1, str.length() - 2);
        
        // Handle escape sequences in double-quoted strings
        if (first == '"') {
            std::string unescaped;
            unescaped.reserve(result.length());
            
            for (size_t i = 0; i < result.length(); ++i) {
                if (result[i] == '\\' && i + 1 < result.length()) {
                    char next = result[i + 1];
                    switch (next) {
                        case 'n': unescaped += '\n'; ++i; break;
                        case 't': unescaped += '\t'; ++i; break;
                        case 'r': unescaped += '\r'; ++i; break;
                        case '\\': unescaped += '\\'; ++i; break;
                        case '"': unescaped += '"'; ++i; break;
                        default: unescaped += result[i]; break;
                    }
                } else {
                    unescaped += result[i];
                }
            }
            return unescaped;
        }
        
        return result;
    }
    
    return str;
}

std::optional<bool> ConfigManager::ParseBool(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") {
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "off" || lower == "0") {
        return false;
    }
    
    return std::nullopt;
}

std::string ConfigManager::ExpandEnvVars(const std::string& value) {
    std::string result;
    result.reserve(value.length());
    
    size_t i = 0;
    while (i < value.length()) {
        // Check for ${VAR} or $VAR pattern
        if (value[i] == '$') {
            if (i + 1 < value.length() && value[i + 1] == '{') {
                // ${VAR} pattern
                size_t end = value.find('}', i + 2);
                if (end != std::string::npos) {
                    std::string varName = value.substr(i + 2, end - i - 2);
                    const char* envValue = std::getenv(varName.c_str());
                    if (envValue) {
                        result += envValue;
                    }
                    i = end + 1;
                    continue;
                }
            } else if (i + 1 < value.length()) {
                // $VAR pattern - read until non-alphanumeric/underscore
                size_t start = i + 1;
                size_t end = start;
                while (end < value.length() && 
                       (std::isalnum(value[end]) || value[end] == '_')) {
                    ++end;
                }
                if (end > start) {
                    std::string varName = value.substr(start, end - start);
                    const char* envValue = std::getenv(varName.c_str());
                    if (envValue) {
                        result += envValue;
                    }
                    i = end;
                    continue;
                }
            }
        }
        
        result += value[i];
        ++i;
    }
    
    return result;
}

std::string ConfigManager::ExpandTilde(const std::string& path) {
    if (path.empty() || path[0] != '~') {
        return path;
    }
    
    // Only expand ~ at the start
    if (path.length() == 1 || path[1] == '/' || path[1] == '\\') {
        std::string home;
        
#ifdef _WIN32
        const char* userProfile = std::getenv("USERPROFILE");
        if (userProfile) {
            home = userProfile;
        } else {
            const char* homeDrive = std::getenv("HOMEDRIVE");
            const char* homePath = std::getenv("HOMEPATH");
            if (homeDrive && homePath) {
                home = std::string(homeDrive) + homePath;
            }
        }
#else
        const char* homeEnv = std::getenv("HOME");
        if (homeEnv) {
            home = homeEnv;
        } else {
            struct passwd* pw = getpwuid(getuid());
            if (pw) {
                home = pw->pw_dir;
            }
        }
#endif
        
        if (!home.empty()) {
            return home + path.substr(1);
        }
    }
    
    return path;
}

std::string ConfigManager::GetDefaultDataDir() {
    std::string path;
    
#ifdef _WIN32
    char appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        path = std::string(appData) + "\\" + DEFAULT_DATADIR_NAME;
    }
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        path = std::string(home) + "/Library/Application Support/" + DEFAULT_DATADIR_NAME;
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        path = std::string(home) + "/" + DEFAULT_DATADIR_NAME;
    }
#endif
    
    return path;
}

// ============================================================================
// Internal Key Management
// ============================================================================

std::string ConfigManager::MakeKey(const std::string& key, const std::string& section) const {
    if (section.empty()) {
        return key;
    }
    return section + ":" + key;
}

// ============================================================================
// File Parsing
// ============================================================================

bool ConfigManager::ParseLine(const std::string& line, const std::string& source, 
                               int lineNum, std::string& currentSection, 
                               ConfigParseResult& result) {
    std::string trimmed = Trim(line);
    
    // Empty line or comment
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
        return true;
    }
    
    // Section header [section]
    if (trimmed[0] == '[') {
        size_t end = trimmed.find(']');
        if (end == std::string::npos) {
            result = ConfigParseResult::Error(
                "Missing closing bracket in section header", source, lineNum);
            return false;
        }
        currentSection = Trim(trimmed.substr(1, end - 1));
        return true;
    }
    
    // Include directive
    if (trimmed.substr(0, 8) == "include ") {
        if (includeDepth_ >= MAX_INCLUDE_DEPTH) {
            result = ConfigParseResult::Error(
                "Maximum include depth exceeded", source, lineNum);
            return false;
        }
        
        std::string includePath = Trim(trimmed.substr(8));
        includePath = Unquote(includePath);
        includePath = ExpandTilde(includePath);
        includePath = ExpandEnvVars(includePath);
        
        ++includeDepth_;
        ConfigParseResult includeResult = ParseFile(includePath, true);
        --includeDepth_;
        
        if (!includeResult.success) {
            result = includeResult;
            return false;
        }
        return true;
    }
    
    // Key=value pair
    size_t eqPos = trimmed.find('=');
    if (eqPos == std::string::npos) {
        // Could be a flag (key with no value) - treat as boolean true
        std::string key = Trim(trimmed);
        
        // Check for negation (nokey)
        bool negated = false;
        if (key.length() > 2 && key.substr(0, 2) == "no") {
            std::string baseKey = key.substr(2);
            // Only treat as negation if it looks like a flag
            if (!baseKey.empty() && std::islower(baseKey[0])) {
                key = baseKey;
                negated = true;
            }
        }
        
        // Validate key
        if (key.empty()) {
            result = ConfigParseResult::Error("Empty key", source, lineNum);
            return false;
        }
        
        for (char c : key) {
            if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') {
                result = ConfigParseResult::Error(
                    "Invalid character in key: " + std::string(1, c), source, lineNum);
                return false;
            }
        }
        
        std::string fullKey = MakeKey(key, currentSection);
        
        ConfigEntry entry;
        entry.key = key;
        entry.value = negated ? "false" : "true";
        entry.section = currentSection;
        entry.source = source;
        entry.lineNumber = lineNum;
        entry.isDefault = false;
        
        entries_[fullKey] = entry;
        return true;
    }
    
    // Parse key=value
    std::string key = Trim(trimmed.substr(0, eqPos));
    std::string value = Trim(trimmed.substr(eqPos + 1));
    
    // Validate key
    if (key.empty()) {
        result = ConfigParseResult::Error("Empty key", source, lineNum);
        return false;
    }
    
    for (char c : key) {
        if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') {
            result = ConfigParseResult::Error(
                "Invalid character in key: " + std::string(1, c), source, lineNum);
            return false;
        }
    }
    
    // Handle quoted values
    value = Unquote(value);
    
    // Expand environment variables
    value = ExpandEnvVars(value);
    
    std::string fullKey = MakeKey(key, currentSection);
    
    // Check if this is a list key (multiple entries with same key)
    auto it = entries_.find(fullKey);
    if (it != entries_.end() && !it->second.isDefault) {
        // Add to list
        lists_[fullKey].push_back(value);
    } else {
        // Create new entry
        ConfigEntry entry;
        entry.key = key;
        entry.value = value;
        entry.section = currentSection;
        entry.source = source;
        entry.lineNumber = lineNum;
        entry.isDefault = false;
        
        entries_[fullKey] = entry;
    }
    
    return true;
}

ConfigParseResult ConfigManager::ParseFile(const std::string& filePath, bool overwrite) {
    // Expand path
    std::string expandedPath = ExpandTilde(filePath);
    expandedPath = ExpandEnvVars(expandedPath);
    
    // Open file
    std::ifstream file(expandedPath);
    if (!file.is_open()) {
        return ConfigParseResult::Error("Cannot open file: " + expandedPath);
    }
    
    // Check file size
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (fileSize > static_cast<std::streampos>(MAX_CONFIG_SIZE)) {
        return ConfigParseResult::Error(
            "Config file too large (max " + std::to_string(MAX_CONFIG_SIZE) + " bytes)",
            expandedPath);
    }
    
    // Read and parse
    std::string currentSection;
    std::string line;
    std::string continuationLine;
    int lineNum = 0;
    
    ConfigParseResult result = ConfigParseResult::Success();
    
    while (std::getline(file, line)) {
        ++lineNum;
        
        // Check line length
        if (line.length() > MAX_LINE_LENGTH) {
            return ConfigParseResult::Error(
                "Line too long (max " + std::to_string(MAX_LINE_LENGTH) + " characters)",
                expandedPath, lineNum);
        }
        
        // Handle line continuation
        if (!line.empty() && line.back() == '\\') {
            continuationLine += line.substr(0, line.length() - 1);
            continue;
        }
        
        if (!continuationLine.empty()) {
            line = continuationLine + line;
            continuationLine.clear();
        }
        
        // Parse the line
        if (!ParseLine(line, expandedPath, lineNum, currentSection, result)) {
            return result;
        }
    }
    
    // Handle trailing continuation
    if (!continuationLine.empty()) {
        if (!ParseLine(continuationLine, expandedPath, lineNum, currentSection, result)) {
            return result;
        }
    }
    
    return ConfigParseResult::Success();
}

ConfigParseResult ConfigManager::ParseString(const std::string& content,
                                              const std::string& sourceName,
                                              bool overwrite) {
    std::istringstream stream(content);
    std::string currentSection;
    std::string line;
    std::string continuationLine;
    int lineNum = 0;
    
    ConfigParseResult result = ConfigParseResult::Success();
    
    while (std::getline(stream, line)) {
        ++lineNum;
        
        // Check line length
        if (line.length() > MAX_LINE_LENGTH) {
            return ConfigParseResult::Error(
                "Line too long (max " + std::to_string(MAX_LINE_LENGTH) + " characters)",
                sourceName, lineNum);
        }
        
        // Handle line continuation
        if (!line.empty() && line.back() == '\\') {
            continuationLine += line.substr(0, line.length() - 1);
            continue;
        }
        
        if (!continuationLine.empty()) {
            line = continuationLine + line;
            continuationLine.clear();
        }
        
        // Parse the line
        if (!ParseLine(line, sourceName, lineNum, currentSection, result)) {
            return result;
        }
    }
    
    // Handle trailing continuation
    if (!continuationLine.empty()) {
        if (!ParseLine(continuationLine, sourceName, lineNum, currentSection, result)) {
            return result;
        }
    }
    
    return ConfigParseResult::Success();
}

ConfigParseResult ConfigManager::ParseCommandLine(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Skip non-option arguments
        if (arg.empty() || arg[0] != '-') {
            continue;
        }
        
        // Remove leading dashes
        while (!arg.empty() && arg[0] == '-') {
            arg = arg.substr(1);
        }
        
        if (arg.empty()) {
            continue;
        }
        
        std::string key;
        std::string value;
        bool hasValue = false;
        
        // Check for = in argument
        size_t eqPos = arg.find('=');
        if (eqPos != std::string::npos) {
            key = arg.substr(0, eqPos);
            value = arg.substr(eqPos + 1);
            hasValue = true;
        } else {
            key = arg;
            
            // Check for negation (no prefix)
            if (key.length() > 2 && key.substr(0, 2) == "no") {
                std::string baseKey = key.substr(2);
                if (!baseKey.empty() && std::islower(baseKey[0])) {
                    key = baseKey;
                    value = "false";
                    hasValue = true;
                }
            }
            
            // Check if next argument is the value
            if (!hasValue && i + 1 < argc && argv[i + 1][0] != '-') {
                value = argv[i + 1];
                hasValue = true;
                ++i;
            }
        }
        
        // If still no value, treat as boolean flag
        if (!hasValue) {
            value = "true";
        }
        
        // Command line args go in global section with highest priority
        ConfigEntry entry;
        entry.key = key;
        entry.value = value;
        entry.section = "";
        entry.source = "<command-line>";
        entry.lineNumber = 0;
        entry.isDefault = false;
        
        // Command line always overwrites
        entries_[key] = entry;
    }
    
    return ConfigParseResult::Success();
}

ConfigParseResult ConfigManager::LoadAllConfigs(const std::string& dataDir) {
    std::vector<std::string> warnings;
    
    // Set data directory
    if (!dataDir.empty()) {
        dataDir_ = dataDir;
    } else if (dataDir_.empty()) {
        dataDir_ = GetDefaultDataDir();
    }
    
    // System config (lowest priority)
#ifndef _WIN32
    std::string systemConfig = "/etc/nexus/nexus.conf";
    std::ifstream testSystem(systemConfig);
    if (testSystem.is_open()) {
        testSystem.close();
        auto result = ParseFile(systemConfig, false);
        if (!result.success) {
            warnings.push_back("Warning: Failed to parse system config: " + result.errorMessage);
        }
    }
#endif
    
    // User config
    std::string userConfig = ExpandTilde("~/" + std::string(DEFAULT_DATADIR_NAME) + "/" + 
                                          DEFAULT_CONFIG_FILENAME);
    std::ifstream testUser(userConfig);
    if (testUser.is_open()) {
        testUser.close();
        auto result = ParseFile(userConfig, false);
        if (!result.success) {
            warnings.push_back("Warning: Failed to parse user config: " + result.errorMessage);
        }
    }
    
    // Data directory config (highest priority for files)
    std::string dataConfig = dataDir_ + "/" + DEFAULT_CONFIG_FILENAME;
    std::ifstream testData(dataConfig);
    if (testData.is_open()) {
        testData.close();
        auto result = ParseFile(dataConfig, true);
        if (!result.success) {
            return result;
        }
    }
    
    ConfigParseResult result = ConfigParseResult::Success();
    result.warnings = warnings;
    return result;
}

// ============================================================================
// Value Retrieval
// ============================================================================

bool ConfigManager::HasKey(const std::string& key, const std::string& section) const {
    std::string fullKey = MakeKey(key, section);
    return entries_.find(fullKey) != entries_.end();
}

std::optional<std::string> ConfigManager::TryGetString(const std::string& key,
                                                        const std::string& section) const {
    std::string fullKey = MakeKey(key, section);
    auto it = entries_.find(fullKey);
    if (it != entries_.end()) {
        return it->second.value;
    }
    return std::nullopt;
}

std::string ConfigManager::GetString(const std::string& key,
                                      const std::string& defaultValue,
                                      const std::string& section) const {
    std::string fullKey = MakeKey(key, section);
    auto it = entries_.find(fullKey);
    if (it != entries_.end()) {
        return it->second.value;
    }
    return defaultValue;
}

std::optional<int64_t> ConfigManager::TryGetInt(const std::string& key,
                                                 const std::string& section) const {
    // Direct lookup
    std::string fullKey = MakeKey(key, section);
    auto it = entries_.find(fullKey);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    const std::string& strValue = it->second.value;
    
    try {
        size_t pos;
        int64_t value = std::stoll(strValue, &pos);
        
        // Check for unit suffixes
        std::string suffix = Trim(strValue.substr(pos));
        if (!suffix.empty()) {
            char unit = std::tolower(suffix[0]);
            switch (unit) {
                case 'k': value *= 1024; break;
                case 'm': value *= 1024 * 1024; break;
                case 'g': value *= 1024LL * 1024 * 1024; break;
                case 't': value *= 1024LL * 1024 * 1024 * 1024; break;
                default: break;
            }
        }
        
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

int64_t ConfigManager::GetInt(const std::string& key,
                               int64_t defaultValue,
                               const std::string& section) const {
    auto value = TryGetInt(key, section);
    return value.value_or(defaultValue);
}

std::optional<uint64_t> ConfigManager::TryGetUInt(const std::string& key,
                                                   const std::string& section) const {
    auto intValue = TryGetInt(key, section);
    if (!intValue || *intValue < 0) {
        return std::nullopt;
    }
    return static_cast<uint64_t>(*intValue);
}

uint64_t ConfigManager::GetUInt(const std::string& key,
                                 uint64_t defaultValue,
                                 const std::string& section) const {
    auto value = TryGetUInt(key, section);
    return value.value_or(defaultValue);
}

std::optional<bool> ConfigManager::TryGetBool(const std::string& key,
                                               const std::string& section) const {
    // Direct lookup
    std::string fullKey = MakeKey(key, section);
    auto it = entries_.find(fullKey);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return ParseBool(it->second.value);
}

bool ConfigManager::GetBool(const std::string& key,
                             bool defaultValue,
                             const std::string& section) const {
    auto value = TryGetBool(key, section);
    return value.value_or(defaultValue);
}

std::optional<double> ConfigManager::TryGetDouble(const std::string& key,
                                                   const std::string& section) const {
    // Direct lookup
    std::string fullKey = MakeKey(key, section);
    auto it = entries_.find(fullKey);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    
    try {
        return std::stod(it->second.value);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

double ConfigManager::GetDouble(const std::string& key,
                                 double defaultValue,
                                 const std::string& section) const {
    auto value = TryGetDouble(key, section);
    return value.value_or(defaultValue);
}

std::vector<std::string> ConfigManager::GetList(const std::string& key,
                                                 const std::string& section) const {
    std::string fullKey = MakeKey(key, section);
    std::vector<std::string> result;
    
    // Check for multiple entries
    auto listIt = lists_.find(fullKey);
    if (listIt != lists_.end()) {
        result = listIt->second;
    }
    
    // Add the primary value if it exists
    auto entryIt = entries_.find(fullKey);
    if (entryIt != entries_.end()) {
        // Check if it's a comma-separated list
        const std::string& value = entryIt->second.value;
        if (value.find(',') != std::string::npos) {
            std::istringstream ss(value);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = Trim(item);
                if (!item.empty()) {
                    result.push_back(item);
                }
            }
        } else if (!value.empty()) {
            result.insert(result.begin(), value);
        }
    }
    
    return result;
}

std::string ConfigManager::GetPath(const std::string& key,
                                    const std::string& defaultValue,
                                    const std::string& section) const {
    std::string value = GetString(key, defaultValue, section);
    value = ExpandTilde(value);
    value = ExpandEnvVars(value);
    return value;
}

// ============================================================================
// Value Setting
// ============================================================================

void ConfigManager::Set(const std::string& key, const std::string& value,
                         const std::string& section) {
    std::string fullKey = MakeKey(key, section);
    
    ConfigEntry entry;
    entry.key = key;
    entry.value = value;
    entry.section = section;
    entry.source = "<programmatic>";
    entry.lineNumber = 0;
    entry.isDefault = false;
    
    entries_[fullKey] = entry;
}

void ConfigManager::SetDefault(const std::string& key, const std::string& value,
                                const std::string& section) {
    std::string fullKey = MakeKey(key, section);
    
    // Only set if not already present
    if (entries_.find(fullKey) == entries_.end()) {
        ConfigEntry entry;
        entry.key = key;
        entry.value = value;
        entry.section = section;
        entry.source = "<default>";
        entry.lineNumber = 0;
        entry.isDefault = true;
        
        entries_[fullKey] = entry;
    }
}

void ConfigManager::AddToList(const std::string& key, const std::string& value,
                               const std::string& section) {
    std::string fullKey = MakeKey(key, section);
    lists_[fullKey].push_back(value);
}

// ============================================================================
// Sections
// ============================================================================

std::vector<std::string> ConfigManager::GetSections() const {
    std::set<std::string> sections;
    
    for (const auto& [key, entry] : entries_) {
        if (!entry.section.empty()) {
            sections.insert(entry.section);
        }
    }
    
    return std::vector<std::string>(sections.begin(), sections.end());
}

std::vector<std::string> ConfigManager::GetKeys(const std::string& section) const {
    std::vector<std::string> keys;
    
    for (const auto& [fullKey, entry] : entries_) {
        if (entry.section == section) {
            keys.push_back(entry.key);
        }
    }
    
    return keys;
}

std::vector<ConfigEntry> ConfigManager::GetEntries(const std::string& section) const {
    std::vector<ConfigEntry> result;
    
    for (const auto& [fullKey, entry] : entries_) {
        if (entry.section == section) {
            result.push_back(entry);
        }
    }
    
    return result;
}

// ============================================================================
// Validation
// ============================================================================

void ConfigManager::RequireKey(const std::string& key, const std::string& section) {
    requiredKeys_.insert(MakeKey(key, section));
}

void ConfigManager::AllowKey(const std::string& key, const std::string& section) {
    allowedKeys_.insert(MakeKey(key, section));
}

std::vector<std::string> ConfigManager::Validate() const {
    std::vector<std::string> errors;
    
    // Check required keys
    for (const auto& requiredKey : requiredKeys_) {
        if (entries_.find(requiredKey) == entries_.end()) {
            errors.push_back("Required key missing: " + requiredKey);
        }
    }
    
    // Warn about unknown keys (if allowed keys are specified)
    if (!allowedKeys_.empty()) {
        for (const auto& [fullKey, entry] : entries_) {
            if (allowedKeys_.find(fullKey) == allowedKeys_.end() &&
                requiredKeys_.find(fullKey) == requiredKeys_.end()) {
                errors.push_back("Unknown key: " + fullKey + 
                                " (defined in " + entry.source + ")");
            }
        }
    }
    
    return errors;
}

// ============================================================================
// Utilities
// ============================================================================

void ConfigManager::Clear() {
    entries_.clear();
    lists_.clear();
    requiredKeys_.clear();
    allowedKeys_.clear();
    dataDir_.clear();
    includeDepth_ = 0;
}

size_t ConfigManager::Size() const {
    return entries_.size();
}

std::string ConfigManager::GetDataDir() const {
    return dataDir_.empty() ? GetDefaultDataDir() : dataDir_;
}

void ConfigManager::SetDataDir(const std::string& dir) {
    dataDir_ = ExpandTilde(dir);
    dataDir_ = ExpandEnvVars(dataDir_);
}

std::string ConfigManager::GenerateSampleConfig() const {
    std::ostringstream oss;
    
    oss << "# SHURIUM Configuration File\n";
    oss << "# See documentation for full list of options\n\n";
    
    oss << "# ============================================================================\n";
    oss << "# General Settings\n";
    oss << "# ============================================================================\n\n";
    
    oss << "# Data directory (default: ~/.shurium)\n";
    oss << "#datadir=~/.shurium\n\n";
    
    oss << "# Run on testnet\n";
    oss << "#testnet=0\n\n";
    
    oss << "# Run in regression test mode\n";
    oss << "#regtest=0\n\n";
    
    oss << "# Enable debug output\n";
    oss << "#debug=0\n\n";
    
    oss << "# ============================================================================\n";
    oss << "# Network Settings\n";
    oss << "# ============================================================================\n\n";
    
    oss << "# Accept incoming connections\n";
    oss << "#listen=1\n\n";
    
    oss << "# Port to listen on (default: 8333)\n";
    oss << "#port=8333\n\n";
    
    oss << "# Maximum peer connections\n";
    oss << "#maxconnections=125\n\n";
    
    oss << "# Connect only to specific nodes\n";
    oss << "#connect=ip:port\n\n";
    
    oss << "# Add a node to connect to\n";
    oss << "#addnode=ip:port\n\n";
    
    oss << "# ============================================================================\n";
    oss << "# RPC Settings\n";
    oss << "# ============================================================================\n\n";
    
    oss << "# Enable RPC server\n";
    oss << "#server=1\n\n";
    
    oss << "# RPC username (CHANGE THIS!)\n";
    oss << "#rpcuser=nexusrpc\n\n";
    
    oss << "# RPC password (CHANGE THIS!)\n";
    oss << "#rpcpassword=CHANGE_THIS_PASSWORD\n\n";
    
    oss << "# RPC port (default: 8332)\n";
    oss << "#rpcport=8332\n\n";
    
    oss << "# Allow RPC connections from (default: 127.0.0.1)\n";
    oss << "#rpcallowip=127.0.0.1\n\n";
    
    oss << "# ============================================================================\n";
    oss << "# Wallet Settings\n";
    oss << "# ============================================================================\n\n";
    
    oss << "# Wallet file path\n";
    oss << "#wallet=wallet.dat\n\n";
    
    oss << "# Disable wallet\n";
    oss << "#disablewallet=0\n\n";
    
    oss << "# ============================================================================\n";
    oss << "# Mining/Staking Settings\n";
    oss << "# ============================================================================\n\n";
    
    oss << "# Enable mining\n";
    oss << "#gen=0\n\n";
    
    oss << "# Number of mining threads\n";
    oss << "#genproclimit=1\n\n";
    
    oss << "# Mining payout address\n";
    oss << "#miningaddress=\n\n";
    
    oss << "# Enable staking\n";
    oss << "#staking=0\n\n";
    
    return oss.str();
}

std::string ConfigManager::Dump() const {
    std::ostringstream oss;
    
    oss << "# Configuration Dump (" << entries_.size() << " entries)\n\n";
    
    // Group by section
    std::map<std::string, std::vector<const ConfigEntry*>> bySection;
    
    for (const auto& [key, entry] : entries_) {
        bySection[entry.section].push_back(&entry);
    }
    
    // Output global section first
    if (bySection.find("") != bySection.end()) {
        oss << "# Global Section\n";
        for (const ConfigEntry* entry : bySection[""]) {
            oss << entry->key << "=" << entry->value;
            if (entry->isDefault) {
                oss << "  # (default)";
            } else {
                oss << "  # " << entry->source;
                if (entry->lineNumber > 0) {
                    oss << ":" << entry->lineNumber;
                }
            }
            oss << "\n";
        }
        oss << "\n";
    }
    
    // Output other sections
    for (const auto& [section, entries] : bySection) {
        if (section.empty()) continue;
        
        oss << "[" << section << "]\n";
        for (const ConfigEntry* entry : entries) {
            oss << entry->key << "=" << entry->value;
            if (entry->isDefault) {
                oss << "  # (default)";
            } else {
                oss << "  # " << entry->source;
                if (entry->lineNumber > 0) {
                    oss << ":" << entry->lineNumber;
                }
            }
            oss << "\n";
        }
        oss << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// Global Configuration
// ============================================================================

static ConfigManager* g_config = nullptr;

ConfigManager& GetConfig() {
    if (!g_config) {
        g_config = new ConfigManager();
    }
    return *g_config;
}

ConfigParseResult InitConfig(int argc, char* argv[]) {
    ConfigManager& config = GetConfig();
    
    // Parse command line first (to get datadir)
    auto cmdResult = config.ParseCommandLine(argc, argv);
    if (!cmdResult.success) {
        return cmdResult;
    }
    
    // Get data directory from command line or use default
    std::string dataDir;
    auto dataDirOpt = config.TryGetString(ConfigKeys::DATADIR);
    if (dataDirOpt) {
        dataDir = ConfigManager::ExpandTilde(*dataDirOpt);
        dataDir = ConfigManager::ExpandEnvVars(dataDir);
    }
    
    // Load all config files
    auto loadResult = config.LoadAllConfigs(dataDir);
    if (!loadResult.success) {
        return loadResult;
    }
    
    // Re-parse command line (command line takes priority over config files)
    return config.ParseCommandLine(argc, argv);
}

} // namespace util
} // namespace shurium
