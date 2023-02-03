#include <automata_engine.h>

#define NC_STR_IMPL
#include <gist/github/nc_strings.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

namespace ae = automata_engine;

namespace automata_engine {
  namespace io {

    // NOTE: These structs are here and not in a separated header because they are internal
    // details of how various file formats are read.
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

    void freeWav(loaded_wav_t wavFile) {
      ae::platform::freeLoadedFile(wavFile.parentFile);
    }
    static wav_file_cursor LoadWav_ParseChunkAt(void *bytePointer, void *endOfFile) {
      wav_file_cursor result;
      result.cursor = (char *)bytePointer;
      result.endOfFile = (char *)endOfFile;
      return result;
    }
    // NOTE(Noah): Again. Consistent naming convention. Here, we have the master function
    // LoadWav. All these other functions are "sub" functions. They exist literally to make
    // the code more readable. But these "sub" functions only exist FOR the master function.
    static bool LoadWav_IsFileCursorValid(wav_file_cursor fileCursor) {
      return fileCursor.cursor < fileCursor.endOfFile;
    }
    static void *LoadWav_GetChunkData(wav_file_cursor fileCursor) {
      void *result = fileCursor.cursor + sizeof(wav_chunk_header);
      return result;
    }
    static int LoadWav_GetChunkSize(wav_file_cursor fileCursor) {
      wav_chunk_header *wavChunkHeader = (wav_chunk_header *)fileCursor.cursor;
      return wavChunkHeader->chunkSize;	
    }
    static wav_file_cursor LoadWav_NextChunk(wav_file_cursor fileCursor) {
      wav_chunk_header_t *wavChunkHeader = (wav_chunk_header *)fileCursor.cursor;
      int chunkSize = wavChunkHeader->chunkSize;
      if(chunkSize & 1) {
        chunkSize +=1 ;
      }
      fileCursor.cursor += sizeof(wav_chunk_header) + chunkSize;
      return fileCursor;
    }
    static int LoadWav_GetType(wav_file_cursor fileCursor) {
      wav_chunk_header_t *wavChunkHeader = (wav_chunk_header *)fileCursor.cursor;
      int result = wavChunkHeader->chunkID;
      return result;
    }
    // TODO(Noah): Use stb_vorbis for .ogg file parsing. Prob going to be better (compressed?)
    // TODO(Noah): Think about failure cases for load file err.
    loaded_wav_t loadWav(const char *fileName) {
      loaded_wav_t wavFile = {};
      loaded_file_t fileResult = ae::platform::readEntireFile(fileName);
      wavFile.parentFile = fileResult;
      if (fileResult.contentSize != 0 ) {
        wav_header *wavHeader = (wav_header *)fileResult.contents;
        assert(wavHeader->chunkID == Wav_ChunkID_RIFF);
        assert(wavHeader->waveID == Wav_ChunkID_WAVE);
        // NOTE(Noah): The end of file computation explained: We go ahead by the initial header size, 
        // then add wavHeader->fileSize, which excludes the 4-byte value after it, so we subtract 4 bytes.
        short *samples = 0;
        int channels = 0;
        int sampleDataSize = 0;
        for(    
          wav_file_cursor fileCursor = LoadWav_ParseChunkAt(wavHeader + 1, 
            (char *)(wavHeader + 1) + wavHeader->fileSize - 4);
          LoadWav_IsFileCursorValid(fileCursor);
          fileCursor = LoadWav_NextChunk(fileCursor) 
        ) {
          switch(LoadWav_GetType(fileCursor)) {
            case Wav_ChunkID_fmt: {
              wav_fmt *wavfmt = (wav_fmt *)LoadWav_GetChunkData(fileCursor);
              assert(wavfmt->wFormatTag == 1);
              assert(wavfmt->nSamplesPerSec == ENGINE_DESIRED_SAMPLES_PER_SECOND);
              assert(wavfmt->wBitsPerSample == 16);
              channels = wavfmt->nChannels;
            } break;
            case Wav_ChunkID_data: {
              samples = (short *)LoadWav_GetChunkData(fileCursor);
              sampleDataSize = LoadWav_GetChunkSize(fileCursor);
            } break;
            default:
            // do nothing I guess, lol.
            break;
          }
        }
        assert((channels && samples && sampleDataSize));
        wavFile.sampleCount = sampleDataSize / (channels * sizeof(short));
        wavFile.channels = channels;
        if (channels != 2) {
            assert(!"Unsupported Channel type!");
        } else {
          wavFile.sampleData = samples;
        }
      }
      return wavFile;
    }

    loaded_image_t loadBMP(const char *path) {
      loaded_image_t bitmap = {};
      // bitmap.scale = 1;
      loaded_file_t fileResult = ae::platform::readEntireFile(path);
      if (fileResult.contentSize != 0) {
        bitmap_header_t *header = (bitmap_header *)fileResult.contents;
        if (header->BitsPerPixel == 32) {
          uint32_t imgSize = header->Height * header->Width * sizeof(uint32_t);
          uint32_t *newData = (uint32_t *)ae::platform::alloc(imgSize);
          if (newData != nullptr) {
            bitmap.pixelPointer = (unsigned int *) ((unsigned char *)fileResult.contents + header->BitmapOffset);
            memcpy(newData, bitmap.pixelPointer, imgSize);
            bitmap.pixelPointer = newData;
            bitmap.height = header->Height;
            bitmap.width = header->Width;
            // Colors in .bmp are stored as ARGB (MSB -> LSB), i.e. 0xAARRGGBB
            // below code to convert to 0xRRGGBBAA.
            unsigned int *SourceDest = bitmap.pixelPointer;
            for (int y = 0; y < bitmap.height; ++y) {
              for (int x = 0; x < bitmap.width; ++x) {
                unsigned int pixel_val = *SourceDest;
                unsigned int R = (pixel_val >> 16) & 0x000000FF;
                unsigned int G = (pixel_val >> 8) & 0x000000FF;
                unsigned int B = (pixel_val >> 0) & 0x000000FF;
                unsigned int A = (pixel_val >> 24) & 0x000000FF;
                *SourceDest++ = (A << 24) | (B << 16) | (G << 8) | (R << 0);
              }
            }
            ae::platform::freeLoadedFile(fileResult);
          } else {
            AELoggerError("loadBMP failed to alloc.");
          }
        } else {
          AELoggerError("%s is %d bpp, not %d", path, header->BitsPerPixel, 32);
        }
      }
      return bitmap;
    }

    void freeObj(raw_model_t obj) {
      StretchyBufferFree(obj.vertexData);
      StretchyBufferFree(obj.indexData);
    }
    
    void freeLoadedImage(loaded_image_t img) {
      ae::platform::free(img.pixelPointer);
    }

    // TODO(Noah): We probably even want unit tests for this sort of thing.
    //
    // TODO(Noah): currently this does not work for any sort of OBJ that has no tex coords.
    // for example the face format might look like: 375//363, and we cannot parse this.
    //
    // TODO(Noah): It's prob the case that we could parse OBJ files better. But, I just wanted
    // to get something working ...
    raw_model_t loadObj(const char *filePath) {
      loaded_file_t loadedFile = ae::platform::readEntireFile(filePath);
      // NOTE(Noah): init the rawModel to null is important because we are
      // depending on the modelName to have null-terminating char.
      raw_model_t rawModel = {};
      if (loadedFile.contents != nullptr) {
        float *uvData = nullptr; // stretchy buf
        float *normalData = nullptr; // stretchy buf
        char *line = (char *)loadedFile.contents;
        uint32_t lineLen = 0;
        uint32_t MAGIC_NUM = 0xFFFFFFFF;
        struct { uint64_t key; uint32_t value; } *vertex_uv_pair_map = NULL;
        // TODO(Noah): Current impl of this is very slow as we end up parsing all chars in the obj
        // like three time. First we go forward to find the end line. Then we go through to delimit.
        // and finally we parse the individual tokens to convert to an int. We could prob do better, yeah?
        //
        // It's interesting how this happened, too. We started with a top down approach. We abstracted first.
        // If we just did the bottom up approach we might have came out with a faster algo, from the get go ...
        uint32_t linesProcessed = 0;
        while(nc::str::getLine(&line, &lineLen) != nc::str::NC_EOF) {
          switch(line[0]) {
            case 'o': {
              line += 2; lineLen -= 2;
              memcpy(rawModel.modelName, line, (lineLen > 12) ? 12 : lineLen);
            } break;
            case 'v': {
              switch(line[1]) {
                case ' ': {
                  line += 2; lineLen -= 2;
                  nc::str::splitFloat(line, lineLen, ' ', [&](float f, uint32_t index){
                    if (index < 3)
                      StretchyBufferPush(rawModel.vertexData, f);
                  });
                  // Ensure there are at least 5 more spaces in underlying buffer (2 for UV, 3 for normal).
                  StretchyBuffer_MaybeGrow(rawModel.vertexData, 5);
                  for (uint32_t i = 0; i < 5; i++) {
                    *(uint32_t *)(rawModel.vertexData + StretchyBufferCount(rawModel.vertexData)) = MAGIC_NUM;
                    StretchyBuffer_GetCount(rawModel.vertexData) += 1;
                  }
                } break;
                case 't': {
                  line += 3; lineLen -= 3;
                  nc::str::splitFloat(line, lineLen, ' ', [&](float f, uint32_t index){
                    if (index < 2)
                      StretchyBufferPush(uvData, f);
                  });
                } break;
                case 'n': {
                  line += 3; lineLen -= 3;
                  nc::str::splitFloat(line, lineLen, ' ', [&](float f, uint32_t index){
                    if (index < 3)
                      StretchyBufferPush(normalData, f);
                  });
                } break;
              }
            } break;
            case 'f': { // face
              line += 2; lineLen -= 2;
              nc::str::split(line, lineLen, ' ', [&](char *token, uint32_t tokenIndex){
                if (tokenIndex < 3) {
                  uint32_t vIndex;
                  uint32_t remapped_vIndex;
                  nc::str::splitInt(token, -1, '/', [&](int num, uint32_t index) {  
                    switch(index) {
                      case 0: {
                        vIndex = num - 1;
                        remapped_vIndex = vIndex;
                      } break;
                      case 1: {
                        uint32_t uvIndex = num - 1;
                        uint64_t i_vertexUV = ((uint64_t)uvIndex << 32) | vIndex;
                        if (stbds_hmgeti(vertex_uv_pair_map, i_vertexUV) == -1) {
                          // not in hash map
                          // pop hash map and get the data going
                          if (*(uint32_t *)(rawModel.vertexData + vIndex * 8 + 3) != MAGIC_NUM) {
                            // Dup vertex with new entry. Copy over pos and normal data.
                            remapped_vIndex = StretchyBufferCount(rawModel.vertexData) / 8;
                            StretchyBuffer_MaybeGrow(rawModel.vertexData, 8);
                            StretchyBuffer_GetCount(rawModel.vertexData) += 8;
                            memcpy((rawModel.vertexData + remapped_vIndex * 8), 
                              (rawModel.vertexData + (vIndex) * 8), sizeof(float) * 8);
                          }
                          stbds_hmput(vertex_uv_pair_map, i_vertexUV, remapped_vIndex);
                          // NOTE(Noah): each vertex is 8 floats (vertex stride),
                          // Uv = 2 floats,
                          // normal = 3 floats.
                          memcpy((rawModel.vertexData + remapped_vIndex * 8 + 3), 
                            (uvData + (uvIndex) * 2), sizeof(float) * 2);
                        } else {
                          // in hash map
                          remapped_vIndex = stbds_hmget(vertex_uv_pair_map, i_vertexUV); 
                        }
                      } break;
                      case 2: {
                        memcpy((rawModel.vertexData + remapped_vIndex * 8 + 5), 
                          (normalData + (num - 1) * 3), sizeof(float) * 3);
                      }
                    }
                  });
                  StretchyBufferPush(rawModel.indexData, remapped_vIndex);
                }
              });
            } break;
            case '#': // a comment in .obj format
            default:
            break;
          }
          line += lineLen;
          linesProcessed++;
        }
        AELoggerLog("loadObj: %d lines processed", linesProcessed);
        StretchyBufferFree(uvData);
        StretchyBufferFree(normalData);
        stbds_hmfree(vertex_uv_pair_map);
        // debug scan on .obj        
#if defined(_DEBUG)
        for (uint32_t i = 0; i < StretchyBufferCount(rawModel.vertexData) / 8; i++) {
          float uvX = rawModel.vertexData[i * 8 + 3];
          float uvY = rawModel.vertexData[i * 8 + 4];
          if (uvX < 0.0f || uvX > 1.0f) {
            AELoggerWarn("uvX out of bounds: %f", uvX);
          }
          if (uvY < 0.0f || uvY > 1.0f) {
            AELoggerWarn("uvY out of bounds: %f", uvY);
          }
        }
#endif
      } else {
        AELoggerError("unable to open %s", filePath);
      }     
      return rawModel;
    }
  }
}