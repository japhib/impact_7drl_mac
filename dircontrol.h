/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:	dircontrol.h ( Directory Control Library, C++)
 *
 * COMMENTS:
 *	This is the control routines to handle directories in a platform
 *	independent manner.
 */

#ifdef LINUX
#include <dirent.h>

    #ifdef __APPLE__
        #include <sys/syslimits.h>
        #define PATH_MAX_LENGTH PATH_MAX
    #else
        #define PATH_MAX_LENGTH NAME_MAX
    #endif

#else
#include <windows.h>
#endif

class DIRECTORY
{
public:
    DIRECTORY();
    ~DIRECTORY();
    
    // Open the directory, returns 0 for success.
    int		opendir(const char *name);

    // Multiple calls of readdir will return successive files
    // in the specified directory.
    // . and .. are ignored.
    // The const char is stored in myLastPath, so will not be
    // preserved across calls.
    const char *readdir();
    void	closedir();
    
protected:
#ifdef LINUX
    DIR			*myDir;
    char		 myLastPath[PATH_MAX_LENGTH + 1];

#else
    WIN32_FIND_DATA	myData;
    HANDLE		myHandle;
    char		myLastPath[MAX_PATH+1];
#endif
};
