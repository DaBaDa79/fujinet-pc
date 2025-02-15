#ifndef DEVICE_DISK_H
#define DEVICE_DISK_H

// MinGW DEVICE_TYPE is defined in winioctl.h
#ifdef DEVICE_TYPE
#undef DEVICE_TYPE
#endif

#ifdef BUILD_ATARI
# include "sio/disk.h"
# define DEVICE_TYPE sioDisk
#endif

#ifdef BUILD_RS232
# include "rs232/disk.h"
# define DEVICE_TYPE rs232Disk
#endif

#ifdef BUILD_APPLE
#include "iwm/disk.h"
#define DEVICE_TYPE iwmDisk
#endif

#ifdef BUILD_CBM
# include "iec/disk.h"
# define DEVICE_TYPE iecDisk
#endif

#ifdef BUILD_ADAM
# include "adamnet/disk.h"
# define DEVICE_TYPE adamDisk
#endif

#ifdef BUILD_LYNX
# include "comlynx/disk.h"
# define DEVICE_TYPE lynxDisk
#endif

#ifdef BUILD_S100
# include "s100spi/disk.h"
# define DEVICE_TYPE s100spiDisk
#endif 

#ifdef NEW_TARGET
# include "new/disk.h"
# define DEVICE_TYPE adamDisk
#endif



#endif // DEVICE_DISK_H