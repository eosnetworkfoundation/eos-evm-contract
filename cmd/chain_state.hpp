#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

// classes and utilies for keeping some external (parent) chain state for resumption

struct chain_state {
   static inline constexpr std::string_view state_file_name = "chain_state.bin";

   chain_state() = default;
   inline explicit chain_state(std::string fn) : filename(std::move(fn)+"/"+std::string(state_file_name.data(), state_file_name.size())) {}

   std::string filename;
   uint32_t    head_block_num       = 0;
   uint32_t    head_trust_block_num = 0;
   int64_t     genesis_time         = 0;

   inline void write() const {
      std::ofstream f(filename, std::ios::binary);
      f.write((const char*)&head_block_num, sizeof(uint32_t));
      f.write((const char*)&head_trust_block_num, sizeof(uint32_t));
      f.write((const char*)&genesis_time, sizeof(int64_t));
   }

   inline void read() {
      std::ifstream f(filename, std::ios::binary);
      f.read((char*)&head_block_num, sizeof(uint32_t));
      f.read((char*)&head_trust_block_num, sizeof(uint32_t));
      f.read((char*)&genesis_time, sizeof(int64_t));
   }

   inline bool exists() const {
      std::filesystem::path cs = {filename};
      return std::filesystem::exists(cs);
   }
};