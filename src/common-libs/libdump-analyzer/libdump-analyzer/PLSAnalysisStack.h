#pragma once
#include <string>
#include <map>
#include <QString>
#include "PLSProcessInfo.h"

namespace pls {

/*Analyze the stack through the dump file, and then send the dump data to nelo.*/
bool analysis_stack_and_send_dump(ProcessInfo info, bool analysis_stack = true);
void catch_unhandled_exceptions(const std::string &process_name, const std::string &dump_path = "");
void catch_unhandled_exceptions_and_send_dump(const ProcessInfo &info);

void set_prism_user_id(const std::string &user_id);
void set_prism_session(const std::string &prism_session);
void set_prism_video_adapter(const std::string &video_adapter);
void set_setup_session(const std::string &session);
void set_prism_sub_session(const std::string &session);

}

