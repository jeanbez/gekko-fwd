#ifndef IFS_INTCP_FUNCTIONS_HPP
#define IFS_INTCP_FUNCTIONS_HPP

#include <dirent.h>

extern "C" {

# define weak_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((weak, alias (#name)));

# define strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));

/**
 * In the glibc headers the following two functions (readdir & opendir)
 * marks the @dirp parameter with a non-null attribute.
 * If we try to implement them directly instead of the weak aliased function,
 * the compiler will assume  that the parameter is actually null and
 * will optimized expression like `(dirp == nullptr)`.
*/

struct dirent* intcp_readdir(DIR* dirp);
weak_alias(intcp_readdir, readdir)

int intcp_closedir(DIR* dirp);
weak_alias(intcp_closedir, closedir)

size_t intcp_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
strong_alias(intcp_fread, fread)
strong_alias(intcp_fread, fread_unlocked)
size_t intcp_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
strong_alias(intcp_fwrite, fwrite)
strong_alias(intcp_fwrite, fwrite_unlocked)

#endif // IFS_INTCP_FUNCTIONS_HPP

} // extern C
