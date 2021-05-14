#ifndef _FN_FILETNFS_
#define _FN_FILETNFS_

#include <stdlib.h>

#include "../TNFSlib/tnfslib.h"
#include "fnFile.h"


class FileHandlerTNFS : public FileHandler
{
protected:
    tnfsMountInfo *_mountinfo = nullptr;
    int _handle = -1;

private:
    uint8_t _bad_fd_recovery();

public:
    FileHandlerTNFS(tnfsMountInfo *mountinfo, int handle);
    virtual ~FileHandlerTNFS() override;

    virtual int close(bool destroy=true) override;
    virtual int seek(long int off, int whence) override;
    virtual long int tell() override;
    virtual size_t read(void *ptr, size_t size, size_t count) override;
    virtual size_t write(const void *ptr, size_t size, size_t count) override;
    virtual int flush() override;
};


#endif //_FN_FILETNFS_
