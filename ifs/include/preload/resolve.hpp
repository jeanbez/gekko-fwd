#include <string>

bool resolve_path (const std::string& path, std::string& resolved, bool resolve_last_link = true);

std::string get_sys_cwd();
void set_sys_cwd(const std::string& path);

void set_env_cwd(const std::string& path);
void unset_env_cwd();

void init_cwd();
void set_cwd(const std::string& path, bool internal);
