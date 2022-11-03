#pragma once

#include <string_view>

namespace trust::evm {
   template <typename Address>
   inline uint64_t get_reserved_address(Address&& address) {
      const auto bytes = address.bytes;
      return static_cast<uint64_t>(bytes[0]) << 7 |
             static_cast<uint64_t>(bytes[1]) << 6 |
             static_cast<uint64_t>(bytes[2]) << 5 |
             static_cast<uint64_t>(bytes[3]) << 4 |
             static_cast<uint64_t>(bytes[4]) << 3 |
             static_cast<uint64_t>(bytes[5]) << 2 |
             static_cast<uint64_t>(bytes[6]) << 1 |
             static_cast<uint64_t>(bytes[7]);
   }
   static inline uint8_t reserved_region[12] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

   template <typename Address>
   inline bool is_reserved_address(Address&& address) {
      const auto bytes = address.bytes;
      return std::memcmp(bytes+8, reserved_region, sizeof(reserved_region)) == 0;
   }

   template <typename Address>
   inline Address construct_reserved_address(uint64_t addr) {
      Address new_address = {0};
      auto bytes = new_address.bytes;
      std::memcpy(bytes+8, reserved_region, sizeof(reserved_region));
      bytes[0] = 0xFF & (addr >> 7);
      bytes[1] = 0xFF & (addr >> 6);
      bytes[2] = 0xFF & (addr >> 5);
      bytes[3] = 0xFF & (addr >> 4);
      bytes[4] = 0xFF & (addr >> 3);
      bytes[5] = 0xFF & (addr >> 2);
      bytes[6] = 0xFF & (addr >> 1);
      bytes[7] = 0xFF & (addr);
      return new_address;
   }

} // ns trust::evm