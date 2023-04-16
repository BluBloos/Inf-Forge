#ifndef AUTOMATA_ENGINE_UTILS_HPP
#define AUTOMATA_ENGINE_UTILS_HPP

#include <stdlib.h>
#include <cassert>
#include <cstdint>
#include <functional>
// TODO(Noah): Do we trust cstdint?
#include <cstdint>

// taken from https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/

// How does code work?
// call defer macro to evaluate our code as a defer statement.
// code into defer macro can be an exp or a statement
// defer statement is defining a var (of type privDefer) with some unique autogen name
// code gets executed when var goes out of scope because code is slotted to execute
//    in destructor of privDefer type.

template <typename F>
struct privDefer {
	F f;
	privDefer(F f) : f(f) {}
	~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
	return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})



// NOTE(Noah): Stretchy buffers adapted from the cryptic C code of https://nothings.org/stb/stretchy_buffer.txt
// Stretchy buffers basically work like so: A block of memory is allocated to store the current count, total element size,
// plus all the elements. The array pointer that was passed in originally is modified in place with the new element pointer,
// which is offset by 2 in the allocated block (just after the count and total element size).
// 
// All stretchy buffers must begin as a null pointer.

// Inits the stretchy buffer.
#define StretchyBufferInitWithCount(a,n)  (StretchyBuffer_Grow(a,n), StretchyBuffer_GetCount(a)=n)
#define StretchyBufferInit(a)             (StretchyBuffer_Grow(a,1))
// Frees the strechy buffer. Warning: the array a will be a dangling pointer after this call.
#define StretchyBufferFree(a)             ((a) ? free(StretchyBuffer_GetMetadataPtr(a)), 0 : 0)
// Pushes a new element to the stretchy buffer.
#define StretchyBufferPush(a,v)           (StretchyBuffer_MaybeGrow(a,1), (a)[StretchyBuffer_GetCount(a)++] = (v))
// Returns a reference to the count of the stretchy buffer.
#define StretchyBufferCount(a)            ((a) ? StretchyBuffer_GetCount(a) : 0)
// Returns a reference to the last element of the stretchy buffer.
#define StretchyBufferLast(a)             ((a)[StretchyBuffer_GetCount(a)-1])

// Returns and deletes the last element from inside the stretchy buffer.
#define StretchyBufferPop(a) ((a)[--StretchyBuffer_GetCount(a)])

#define StretchyBuffer_GetMetadataPtr(a)  ((int *) (a) - 2)
#define StretchyBuffer_GetBufferSize(a)   StretchyBuffer_GetMetadataPtr(a)[0]
#define StretchyBuffer_GetCount(a)        StretchyBuffer_GetMetadataPtr(a)[1]

#define StretchyBuffer_NeedGrow(a,n)      ((a) == 0 || StretchyBuffer_GetCount(a) + n >= StretchyBuffer_GetBufferSize(a))
#define StretchyBuffer_MaybeGrow(a,n)     (StretchyBuffer_NeedGrow(a,(n)) ? StretchyBuffer_Grow(a,n) : (void)0)
#define StretchyBuffer_Grow(a,n)          StretchyBuffer_Growf((void **) &(a), (n), sizeof(*(a)))

static void StretchyBuffer_Growf(void **arr, int increment, int itemsize)
{
   int m = *arr ? 2 * StretchyBuffer_GetBufferSize(*arr) + increment : increment + 1;
   void *p = realloc(*arr ? StretchyBuffer_GetMetadataPtr(*arr) : 0, itemsize * m + sizeof(int) * 2);
   assert(p);
   if (p) {
      if (!*arr) ((int *) p)[1] = 0;
      *arr = (void *) ((int *) p + 2);
      StretchyBuffer_GetBufferSize(*arr) = m;
   }
}



// TODO(Noah): Let's do some proper fast string functions, yeah?
// TODO(Noah): Let's also unit test these functions, yeah?

namespace nc {
  namespace str {
    enum get_line_result {
      NC_EOL, NC_EOF
    };
    /**
      * if *pLineLen == 0 and *pLine != nullptr, getLine will presume this is first call to getLine.
      * it will find the lineLen based on current pLine. Subsequent calls advance to the next line!
      */
    get_line_result getLine(char **pLine, uint32_t *pLineLen);
    // for split funcs, if the lineLen is any negative number, char *is presumed to be null terminated.
    void splitFloat(char *line, int32_t lineLen, char delimiter, std::function<void(float, uint32_t)>);
    void splitInt(char *line, int32_t lineLen, char delimiter, std::function<void(int, uint32_t)>);
    void split(char *line, int32_t lineLen, char delimiter, std::function<void(char *, uint32_t)>);
    bool isEOLOrEOF(char c);
    bool isEOL(char c);
  }
}

#if defined (NC_STR_IMPL)
namespace nc {
  namespace str {
    static char _funBuffer[256] = {};
    bool isEOL(char c) { return c == '\n' || c == '\r'; }
    bool isEOLOrEOF(char c) { return isEOL(c) || c == 0; }

    // the way that getLine works is that we expect a pointer to the
    // beginning of a line. We return through the lineLen how much chars
    // there are for the line (excluding the EOL chars).
    // We also advance the pLine if we find EOL chars to begin with.
    // and we expect the next invocation to have been advanced by at least
    // lineLen chars.
    get_line_result getLine(char **pLine, uint32_t *pLineLen) {
      uint32_t lineLen = 0;
      if (pLine != nullptr && *pLine != nullptr && pLineLen != nullptr) {
        while(isEOL((*pLine)[0])) { *pLine += 1; }
        if ((*pLine)[0] == 0) return NC_EOF;
        // LF = \n == 0x0A
        // CR = \r == 0x0D
        // file can be either CRLF or just LF.
        while (!isEOLOrEOF((*pLine)[lineLen])) { lineLen++; }
        *pLineLen = lineLen;
        return NC_EOL;
      } else {
        return NC_EOF;
      }
    }
    void split(char *line, int32_t lineLen, char delimiter, std::function<void(char *, uint32_t)> f) {
      // add mode. use mode to make a while loop that is also a for loop.
      // for last token parse, need lineLen. But since we have just gone
      // thru all chars, lineLen is actually a thing now.
      bool unlimitedMode = false;
      if (lineLen < 0) {
        unlimitedMode = true;
        lineLen = 0;
      }
      // NOTE(Noah): Are you ready for the big brain move? How do we pass null-terminated string to
      // f if we have a contiguous string by line? Set the delimited chars to 0 before invoke, and reset after!
      uint32_t index = 0;
      char *base = line;
      uint32_t i = 0;
      while ((unlimitedMode) ? line[i] != 0 : i < lineLen) {
        if (line[i] == delimiter) {
          char c_cache = line[i];
          line[i] = 0; // override with null-terminator
          f(base, index++);
          base = line + i + 1;
          line[i] = c_cache; // reset override
        }
        i++; if (unlimitedMode) lineLen++;
      }
      // was the very last char a delimited one?
      bool lastCharDelimited = base - line > lineLen;
      if (!lastCharDelimited) {
        // last token to pass.
        uint32_t lastTokenLen = lineLen - (base - line);
        if (lastTokenLen > 255) {
          // ew, why such a long string?
          // TODO(Noah): This path is untested.
          char *myFunBuffer = (char *)malloc(lastTokenLen + 1);
          memcpy(myFunBuffer, base, lastTokenLen);
          myFunBuffer[lastTokenLen] = 0;
          f(myFunBuffer, index);
          free(myFunBuffer);
        } else {
          memcpy(_funBuffer, base, lastTokenLen);
          _funBuffer[lastTokenLen] = 0;
          f(_funBuffer, index);
        }
      }
    }
    void splitInt(char *line, int32_t lineLen, char delimiter, std::function<void(int, uint32_t)> f) {
      split(line, lineLen, delimiter, [=](char *token, uint32_t tokenIndex) {
        f(atoi(token), tokenIndex);
      });
    }
    void splitFloat(char *line, int32_t lineLen, char delimiter, std::function<void(float, uint32_t)> f) {
      split(line, lineLen, delimiter, [=](char *token, uint32_t tokenIndex) {
        f(atof(token), tokenIndex);
      });
    }
  }
}
#endif  // NC_STR_IMPL

typedef float                  float32_t;
typedef double                 float64_t;

namespace automata_engine {
  enum game_key_t;
  // TODO: do this better, please.
  const char *gameKeyToString(game_key_t keyIdx);
}  // namespace automata_engine

#endif  // AUTOMATA_ENGINE_UTILS_HPP