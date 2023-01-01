#include <automata_engine.h>
#include <automata_engine_io.h>

#define NC_STR_IMPL
#include <gist/github/nc_strings.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

namespace ae = automata_engine;

namespace automata_engine {
  namespace io {
    loaded_image_t loadBMP(char *path) {
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
            PlatformLoggerError("loadBMP failed to alloc.");
          }
        } else {
          PlatformLoggerError("%s is %d bpp, not %d", path, header->BitsPerPixel, 32);
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

    // TODO(Noah): It's prob the case that we could parse OBJ files better. But, I just wanted
    // to get something working ...
    raw_model_t loadObj(const char *filePath) {
      loaded_file_t loadedFile = ae::platform::readEntireFile((char *)filePath);
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
        PlatformLoggerLog("loadObj: %d lines processed", linesProcessed);
        StretchyBufferFree(uvData);
        StretchyBufferFree(normalData);
        stbds_hmfree(vertex_uv_pair_map);
        // debug scan on .obj        
#if !defined(RELEASE)
        for (uint32_t i = 0; i < StretchyBufferCount(rawModel.vertexData) / 8; i++) {
          float uvX = rawModel.vertexData[i * 8 + 3];
          float uvY = rawModel.vertexData[i * 8 + 4];
          if (uvX < 0.0f || uvX > 1.0f) {
            PlatformLoggerWarn("uvX out of bounds: %f", uvX);
          }
          if (uvY < 0.0f || uvY > 1.0f) {
            PlatformLoggerWarn("uvX out of bounds: %f", uvY);
          }
        }
#endif
      } else {
        PlatformLoggerError("unable to open %s", filePath);
      }     
      return rawModel;
    }
  }
}