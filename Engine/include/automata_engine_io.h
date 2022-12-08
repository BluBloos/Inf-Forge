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
#define RIFF_CODE(a,b,c,d) (((int)(a << 0)) | ((int)(b << 8)) | ((int)(c << 16)) | ((int)(d << 24)))
        typedef struct wav_header {
            int chunkID;
            int fileSize;
            int waveID;
        } wav_header_t;
        typedef struct wav_chunk_header {
            int chunkID;
            int chunkSize;
        } wav_chunk_header_t;
        enum {
            Wav_ChunkID_fmt = RIFF_CODE('f','m','t',' '),
            Wav_ChunkID_WAVE = RIFF_CODE('W','A','V','E'),
            Wav_ChunkID_RIFF = RIFF_CODE('R','I','F','F'),
            Wav_ChunkID_data = RIFF_CODE('d','a','t','a')
        };
        typedef struct wav_file_cursor {
            char *cursor;
            char *endOfFile;
        } wav_file_cursor_t;
        typedef struct wav_fmt {
            short wFormatTag; // Format code
            short nChannels; // Number of interleaved channels
            int nSamplesPerSec; // Sampling rate (blocks per second)
            int nAvgBytesPerSec; // Data rate
            short nBlockAlign; // Data block size (bytes)
            short wBitsPerSample; // Bits per sample
            short cbSize; // Size of the extension (0 or 22)
            short wValidBitsPerSample; // Number of valid bits
            int dwChannelMask; // Speaker position mask
            char SubFormat[16]; // GUID, including the data format code
        } wav_fmt_t;
    }
}