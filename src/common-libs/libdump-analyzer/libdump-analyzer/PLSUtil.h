#include <QString>
#include "PLSProcessInfo.h"

namespace pls {
QString get_app_run_dir(QString name);
QString get_app_data_dir(QString name);

std::string get_os_version();

std::string get_plugin_version(std::string &path);

#if __APPLE__
std::string get_device_model();
std::string get_device_name();
std::string generate_dump_file(std::string info, std::string message);
#endif
bool is_third_party_plugin(std::string &module_path);
}