// Local replacement for plaits/user_data.h (the original programs STM32 flash). This firmware has
// no user data: every slot is empty, so the six-op engines use Plaits' built-in FM patch banks.
#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_

#include "stmlib/stmlib.h"

namespace plaits {

class UserData {
 public:
  enum { SIZE = 0x1000 };
  UserData() { }
  ~UserData() { }
  inline const uint8_t* ptr(int slot) const { (void)slot; return NULL; }
  inline bool Save(uint8_t* rx_buffer, int slot) { (void)rx_buffer; (void)slot; return false; }
};

}  // namespace plaits

#endif  // PLAITS_USER_DATA_H_
