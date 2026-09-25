#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
class HalFile {
  std::unique_ptr<std::fstream> file;
  std::filesystem::path path;

 public:
  HalFile() = default;
  HalFile(HalFile&&) = default;
  HalFile& operator=(HalFile&&) = default;
  bool open(const std::filesystem::path& p, bool write) {
    path = p;
    file = std::make_unique<std::fstream>(
        p, std::ios::binary | (write ? std::ios::in | std::ios::out | std::ios::trunc : std::ios::in));
    return bool(*file);
  }
  explicit operator bool() const { return file && bool(*file); }
  int read(void* p, size_t n) {
    file->read(static_cast<char*>(p), n);
    return file->gcount();
  }
  int read() { return file->get(); }
  size_t write(const void* p, size_t n) {
    file->write(static_cast<const char*>(p), n);
    return *file ? n : 0;
  }
  size_t size() { return std::filesystem::file_size(path); }
  bool seek(size_t n) {
    file->clear();
    file->seekg(n);
    file->seekp(n);
    return bool(*file);
  }
  void flush() {
    if (file) file->flush();
  }
  bool close() {
    if (file) file->close();
    file.reset();
    return true;
  }
};
class StorageStub {
 public:
  std::filesystem::path root;
  bool exists(const char* p) { return std::filesystem::exists(root / std::string(p).substr(1)); }
  bool remove(const char* p) { return std::filesystem::remove(root / std::string(p).substr(1)); }
  bool openFileForRead(const char*, const char* p, HalFile& f) {
    return f.open(root / std::string(p).substr(1), false);
  }
  bool openFileForWrite(const char*, const char* p, HalFile& f) {
    return f.open(root / std::string(p).substr(1), true);
  }
  bool ensureDirectoryExists(const char* p) {
    std::filesystem::create_directories(root / std::string(p).substr(1));
    return true;
  }
};
inline StorageStub storageStub;
#define Storage storageStub
