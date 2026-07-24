#ifndef PLSMACPRODUCTPROCESSMONITOR_H
#define PLSMACPRODUCTPROCESSMONITOR_H

#include <cstdint>

#include <libutils-api.h>

namespace pls {
namespace mac {

bool is_product_process_running(pls_product_type_t product);
bool is_product_process_running_for_pid(std::uint32_t pid);
bool pls_is_app_exited(pls_process_t *process);

}
}

#endif
