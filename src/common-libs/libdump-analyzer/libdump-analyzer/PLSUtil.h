#include <QString>

namespace pls {
    QString get_app_run_dir(QString name);
    QString get_app_data_dir(QString name);
    
    std::string get_os_version();
    
#if __APPLE__
    std::string get_device_model();
    std::string get_device_name();
    std::string get_threads();
#endif
}
