#pragma once

#include <automata_engine.h>

namespace automata_engine {
    namespace io {
        #pragma pack(push, 1)
        typedef struct bitmap_header {
            unsigned short FileType;          /* File type, always 4D42h ("BM") */
            unsigned int FileSize;            /* Size of the file in bytes */
            unsigned short Reserved1;         /* Always 0 */
            unsigned short Reserved2;         /* Always 0 */
            unsigned int BitmapOffset;        /* Starting position of image data in bytes */
            unsigned int size;                /* Size of this header in bytes */
            int Width;                        /* Image width in pixels */
            int Height;                       /* Image height in pixels */
            unsigned short Planes;            /* Number of color planes */
            unsigned short BitsPerPixel;      /* Number of bits per pixel */
            unsigned int biCompression;       /* specfies type of compression to be used */
            unsigned int biSizeImage;         /* can be set to 0 for uncompressed RGB bitmaps */
            int biXPelsPerMeter;              /* horizontal resolution in pixels per meter */
            int biYPelsPerMeter;              /* vertical resoltion in pixels per meter */
            unsigned int biClrUsed;           /* specifies color indices used in color table */
            unsigned int biClrImportant;      /* specifies color indices that are important */
            char rgbBlue;
            char rgbGreen;
            char rgbRed;
            char rgbReserved;
        } bitmap_header_t;
        #pragma pack(pop)
    }
}