#include "tkk-gw/adapters/s7adapter.hpp"
/**
 * @brief main entry
 *
 * @return int
 */
int main(void)
{
    S7Adapter a1{"../test/config/S7testConfig_area.json"};
    a1.init();
}