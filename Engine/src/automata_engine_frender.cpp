#include <automata_engine.hpp>

namespace automata_engine {
    namespace frender {

        struct backbuffer_t {
            uint32_t *memory;
            uint32_t  width;
            uint32_t  height;
            uint32_t  bytesPerPixel;
            uint32_t  pitch;
        };

        static int FloatToInt(float Value)
        {
            //TODO: Intrisnic
            //apparently there is a lot more complexity than just this.
            int Result = (int)(Value + 0.5f);
            if (Value < 0.0f) { Result = (int)(Value - 0.5f); }
            return Result;
        }

        static void DrawBitmapScaled(backbuffer_t *buffer,
            ae::loaded_image_t                     Bitmap,
            float                                  RealMinX,
            float                                  RealMaxX,
            float                                  RealMinY,
            float                                  RealMaxY,
            unsigned int                           Scalar)
        {
            if (Scalar < 0.f) return;
            float DrawHeight = (RealMaxY - RealMinY) / (float)Scalar;
            RealMaxY         = RealMinY + DrawHeight;

            int MinX = FloatToInt(RealMinX);
            int MaxX = FloatToInt(RealMaxX);
            int MinY = FloatToInt(RealMinY);
            int MaxY = FloatToInt(RealMaxY);

            unsigned int PitchShift = 0;
            unsigned int PixelShift = 0;

            if (MaxX > buffer->width) { MaxX = buffer->width; }

            if ((MinY + (int)Bitmap.height) < MaxY) { MaxY = MinY + Bitmap.height; }

            if ((MinX + (int)Bitmap.width * (int)Scalar) < MaxX) { MaxX = MinX + Bitmap.width * (int)Scalar; }

            int MaxDrawY = (MaxY - MinY) * (int)Scalar + MinY;
            if (MaxDrawY > buffer->height) { MaxY = (buffer->height - MinY) / (int)Scalar + MinY; }

            if (MinX < 0) {
                PixelShift = (unsigned int)abs((float)MinX / (float)Scalar);
                MinX       = 0;
            }

            if (MinY < 0) {
                PitchShift = (unsigned int)abs((float)MinY / (float)Scalar);
                MaxY += FloatToInt(abs((float)MinY / (float)Scalar));
                MinY = 0;
            }

            unsigned char *Row =
                ((unsigned char *)buffer->memory) + MinX * buffer->bytesPerPixel + MinY * buffer->pitch;

            unsigned int *SourceRow =
                Bitmap.pixelPointer + Bitmap.width * (Bitmap.height - 1 - PitchShift) + PixelShift;
            int Counter = 1;

            for (int Y = MinY; Y < MaxY; ++Y) {
                unsigned int *Pixel       = (unsigned int *)Row;
                unsigned int *SourcePixel = SourceRow;
                for (int X = MinX; X < MaxX; ++X) {
                    if ((*SourcePixel >> 24) & 1) {
                        *Pixel = *SourcePixel;
                        for (unsigned int j = 1; j < (int)Scalar; j++) {
                            unsigned int *PixelBelow = Pixel + buffer->width * j;
                            *PixelBelow              = *SourcePixel;
                        }
                    }

                    Pixel++;

                    if (Counter % (int)Scalar == 0) { SourcePixel += 1; }
                    Counter++;
                }

                SourceRow -= Bitmap.width;
                Row     = Row + buffer->pitch * (int)Scalar;
                Counter = 1;
            }
        }

        // for drawing images loaded from .BMP
        static void DrawBitmap(backbuffer_t *buffer,
            ae::loaded_image_t               Bitmap,
            float                            RealMinX,
            float                            RealMaxX,
            float                            RealMinY,
            float                            RealMaxY)
        {
            int MinX = FloatToInt(RealMinX);
            int MaxX = FloatToInt(RealMaxX);
            int MinY = FloatToInt(RealMinY);
            int MaxY = FloatToInt(RealMaxY);

            if (MinX < 0) { MinX = 0; }
            if (MinY < 0) { MinY = 0; }
            if (MaxX > buffer->width) { MaxX = buffer->width; }
            if (MaxY > buffer->height) { MaxY = buffer->height; }
            if (MinX + (int)Bitmap.width < MaxX) { MaxX = MinX + Bitmap.width; }
            if (MinY + (int)Bitmap.height < MaxY) { MaxY = MinY + Bitmap.height; }

            unsigned char *Row =
                ((unsigned char *)buffer->memory) + MinX * buffer->bytesPerPixel + MinY * buffer->pitch;

            unsigned int *SourceRow = (unsigned int *)Bitmap.pixelPointer + Bitmap.width * (Bitmap.height - 1);
            for (int Y = MinY; Y < MaxY; ++Y) {
                unsigned int *Pixel       = (unsigned int *)Row;
                unsigned int *SourcePixel = SourceRow;
                for (int X = MinX; X < MaxX; ++X) {
                    if ((*SourcePixel >> 24) & 1) { *Pixel = *SourcePixel; }
                    Pixel++;
                    SourcePixel += 1;
                }
                SourceRow -= Bitmap.width;
                Row += buffer->pitch;
            }
        }

        void engineIntroRender(
            uint32_t *pixels, uint32_t width, uint32_t height, float introElapsed, loaded_image_t logo)
        {
            // render solid black.
            uint32_t *pp = (uint32_t *)pixels;
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) { *(pp + x) = 0; }
                pp += width;
            }

            // draw the logo in the middle and scale over time.
            uint32_t scaleFactor     = (1.0f) + 4.f * sqrtf(introElapsed);
            float    scaledImgWidth  = logo.width * scaleFactor;
            float    scaledImgHeight = logo.height * scaleFactor;

            // TODO: factor out.
            auto RandomUINT32 = [](uint32_t lower, uint32_t upper) {
                assert(upper >= lower);
                if (upper == lower) return upper;
                uint64_t diff = upper - lower;
                assert((uint32_t)RAND_MAX >= diff);
                return (uint32_t)((uint64_t)rand() % (diff + 1) + (uint64_t)lower);
            };

            float offsetX = (introElapsed > 0.9f) ? (int)RandomUINT32(0, 100) - 50 : 0;
            float offsetY = (introElapsed > 0.9f) ? (int)RandomUINT32(0, 100) - 50 : 0;

            offsetX /= (0.1f + introElapsed * 2.f);
            offsetY /= (0.1f + introElapsed * 2.f);

            float posX = offsetX + width / 2.f - scaledImgWidth / 2.f;
            float posY = offsetY + height / 2.f - scaledImgHeight / 2.f;

            backbuffer_t backbuffer = {.memory = pixels,
                .width                         = width,
                .height                        = height,
                .bytesPerPixel                 = sizeof(uint32_t),
                .pitch                         = sizeof(uint32_t) * width};

            DrawBitmapScaled(&backbuffer, logo, posX, posX + scaledImgWidth, posY, posY + scaledImgHeight, scaleFactor);
        }

        void engineIntroLoadAssets(loaded_image_t *pLogo, loaded_wav_t *pTheme)
        {
            // TODO: below should be in the IO namespace.
            *pLogo  = ae::platform::stbImageLoad("res\\logo.png");
            *pTheme = ae::io::loadWav("res\\engine.wav");
        }
    }  // namespace frender
}  // namespace automata_engine