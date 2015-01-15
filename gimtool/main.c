#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "lodepng.h"
#include "pngquant/libimagequant.h"

#define LONIBBLE(x)  ((uint8_t) ((uint8_t) (x) & (uint8_t) 0x0F))
#define HINIBBLE(x)  ((uint8_t) ((uint8_t) (x) >> (uint8_t) 4))

#define TYPE_8BPP 0x13
#define TYPE_4BPP 0x14

typedef struct gimHeader {
	char magic[16];
	char title[16];

	uint16_t unk1;
	uint16_t unk2;
	uint16_t unk3;

	uint8_t padding[10];

	uint16_t width;
	uint16_t height;
	uint16_t type;

	uint16_t unk4;
	uint16_t unk5;
	uint16_t unk6;

	uint32_t dataOffset;

	uint16_t unk7;
	uint16_t unk8;
	uint16_t unk9;
	uint16_t unk10;
	uint16_t unk11;
	uint16_t unk12;

	uint32_t paletteOffset;
} gimHeader_t;

// Credit: RTFTool by Shaun Thompson
// https://github.com/neko68k/rtftool/blob/master/RTFTool/rtfview/p6t_v2.cpp
// Unknown license
void deswizzle_8bpp(uint8_t *inpixels, uint8_t *outpixels, size_t width, size_t height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int block_location = (y & (~0xf)) * width + (x & (~0xf)) * 2;
            int swap_selector = (((y + 2) >> 2) & 0x1) * 4;
            int posY = (((y & (~3)) >> 1) + (y & 1)) & 0x7;
            int column_location = posY * width * 2 + ((x + swap_selector) & 0x7) * 4;

            int byte_num = ((y >> 1) & 1) + ((x >> 2) & 2);     // 0,1,2,3

            outpixels[(y * width) + x] = inpixels[block_location + column_location + byte_num];
        }
    }

}

// Source: http://ps2linux.no-ip.info/playstation2-linux.com/docs/howto/display_docef7c.html?docid=75
// Unknown license
void swizzle_8bpp(uint8_t *inpixels, uint8_t *outpixels, size_t width, size_t height)
{
   // this function works for the following resolutions
   // Width:       any multiple of 16 smaller then or equal to 4096
   // Height:      any multiple of 4 smaller then or equal to 4096

   // the texels must be uploaded as a 32bit texture
   // width_32bit = width_8bit / 2
   // height_32bit = height_8bit / 2
   // remember to adjust the mapping coordinates when
   // using a dimension which is not a power of two

   for(int y=0; y<height; y++)
      for(int x=0; x<width; x++)
      {
         const uint8_t uPen = ((const uint8_t *) inpixels)[y*width+x];

         const int32_t block_location = (y&(~0xf))*width + (x&(~0xf))*2;
         const uint32_t swap_selector = (((y+2)>>2)&0x1)*4;
         const int32_t posY = (((y&(~3))>>1) + (y&1))&0x7;
         const int32_t column_location = posY*width*2 + ((x+swap_selector)&0x7)*4;

         const int32_t byte_num = ((y>>1)&1) + ((x>>2)&2);     // 0,1,2,3

         ((uint8_t *) outpixels)[block_location + column_location + byte_num] = uPen;
      }
}

// Credit: Rainbow by Marco Calautti
// Source: https://github.com/marco-calautti/Rainbow/blob/master/Rainbow.ImgLib/ImgLib/Filters/TIM2PaletteFilter.cs
// Licensed under GPL 2
void PaletteFilter(uint32_t *unfiltered, uint32_t *filtered, int length)
{
	int parts = length / 32;
	int stripes = 2;
	int colors = 8;
	int blocks = 2;

	int i = 0;
	for (int part = 0; part < parts; part++)
	{
		for (int block = 0; block < blocks; block++)
		{
			for (int stripe = 0; stripe < stripes; stripe++)
			{

				for (int color = 0; color < colors; color++)
				{
					filtered[i++] = unfiltered[ part * colors * stripes * blocks + block * colors + stripe * stripes * colors + color ];
				}
			}
		}
	}
}

void printGimHeader(gimHeader_t *header)
{
	/*
	printf("magic:         %s\n", header->magic);
	printf("title:         %s\n", header->title);
	printf("width:         %d\n", header->width);
	printf("height:        %d\n", header->height);
	printf("dataOffset:    0x%X\n", header->dataOffset);
	printf("paletteOffset: 0x%X\n", header->paletteOffset);

	printf("unk1:          0x%02X\n", header->unk1);
	printf("unk2:          0x%02X\n", header->unk2);
	printf("unk3:          0x%02X\n", header->unk3);
	printf("unk4:          0x%02X\n", header->unk4);
	printf("unk5:          0x%02X\n", header->unk5);
	printf("unk6:          0x%02X\n", header->unk6);
	printf("unk7:          0x%02X\n", header->unk7);
	printf("unk8:          0x%02X\n", header->unk8);
	printf("unk9:          0x%02X\n", header->unk9);
	printf("unk10:         0x%02X\n", header->unk10);
	printf("unk11:         0x%02X\n", header->unk11);
	printf("unk12:         0x%02X\n\n", header->unk12);
	*/
	printf("%s, ", header->magic);
	printf("%s, ", header->title);
	printf("%02x, ", header->type);
	printf("%02X, ", header->width);
	printf("%02X, ", header->height);
	printf("%X, ", header->dataOffset);
	printf("%X, ", header->paletteOffset);

	printf("%02X, ", header->unk1);
	printf("%02X, ", header->unk2);
	printf("%02X, ", header->unk3);
	printf("%02X, ", header->unk4);
	printf("%02X, ", header->unk5);
	printf("%02X, ", header->unk6);
	printf("%02X, ", header->unk7);
	printf("%02X, ", header->unk8);
	printf("%02X, ", header->unk9);
	printf("%02X, ", header->unk10);
	printf("%02X, ", header->unk11);
	printf("%02X\n", header->unk12);
}

void printcsvheader()
{
	printf("magic, title, type, width, height, dataOffset, paletteOffset, unk1, unk2, unk3, unk4, unk5, unk6, unk7, unk8, unk9, unk10, unk11, unk12\n");
}

int main(int argc, char* argv[])
{
	char			mode;
	char*			gimPath;
	char			pngPath[260];
	FILE* 			fGim = NULL;
	uint8_t*		gimData;
	uint8_t*		pngData;
	uint32_t*		imageData;
	size_t 			gimLength;
	gimHeader_t*	header;

	if( argc < 3 ) {
		printf("gim2png - Converts Initial D Special Stage GIM textures to/from PNG files.\n");
		printf("Usage:\n");
		printf("Extract: %s e <file.gim>\n", argv[0]);
		printf("Inject: %s i <file.gim>\n", argv[0]);
		return EXIT_FAILURE;
	}

	mode = tolower(argv[1][0]);
	//mode = 'i';
	gimPath = argv[2];
	//gimPath = "AEND_TEX_08.gim";
	strcpy(pngPath, gimPath);
	strcat(pngPath, ".png");
    fGim = fopen(gimPath, "rb");
    if(!fGim)
	{
		printf("Error opening %s.\n", gimPath);
		return EXIT_FAILURE;
	}

    fseek(fGim, 0, SEEK_END);
    gimLength = ftell(fGim);
    fseek(fGim, 0, SEEK_SET);
    gimData = malloc(gimLength);
    if(!gimData)
	{
		printf("malloc error\n");
		fclose(fGim);
		return EXIT_FAILURE;
	}
    fread(gimData, 1, gimLength, fGim);
    fclose(fGim);

	/* Adjust offsets so we can actually use them */
	header = (gimHeader_t *)gimData;
	header->dataOffset += (uint32_t) gimData;
	header->paletteOffset += (uint32_t) gimData;

	if(mode == 'e') // convert gim to png
	{
		printGimHeader(header);

		if(header->type == TYPE_8BPP) // 8bpp has to be deswizzled and filtered
		{
			unsigned char *original = malloc(header->width * header->height);
			memcpy(original, (uint8_t *)header->dataOffset, header->width * header->height);
			deswizzle_8bpp(original, (uint8_t *)header->dataOffset, header->width, header->height);
			free(original);

			uint32_t *filtered = malloc(256 * 4);
			PaletteFilter((uint32_t *) header->paletteOffset, filtered, 256);
			memcpy((void *) header->paletteOffset, filtered, 256 * 4);
			free(filtered);
		}

		imageData = malloc(header->width * header->height * 4);
		for(int i = 0; i < (header->width * header->height); i++)
		{
			if(header->type == TYPE_8BPP)
			{
				uint8_t pixel = *((uint8_t*)header->dataOffset + i);
				uint8_t alpha = (*((uint32_t*)header->paletteOffset + pixel) >> 24);

				if(alpha > 0)
					alpha = (alpha <<1)-1;  // scale from 0-128 to 0-255 range

				imageData[i] = *((uint32_t *)header->paletteOffset + pixel) | (alpha << 24);
			}
			else if(header->type == TYPE_4BPP)
			{
				uint8_t pixel = *((uint8_t*)header->dataOffset + i/2);

				if(i%2 != 0)
					pixel = (pixel >> 4) & 0x0f; // first 4 bits
				else
					pixel &= 0x0f; // last 4 bits


				uint8_t alpha = (*((uint32_t*)header->paletteOffset + pixel) >> 24);

				if(alpha > 0)
					alpha = (alpha <<1)-1;  // scale from 0-128 to 0-255 range

				imageData[i] = *((uint32_t *)header->paletteOffset + pixel) | (alpha << 24);
			}
			else
				printf("Unknown image type 0x%02X\n", header->type);
		}

		int error = lodepng_encode32_file(pngPath, (unsigned char*)imageData, header->width, header->height);
		if (error)
		{
			printf("Error saving PNG...\n");
			printf("%u: %s\n", error, lodepng_error_text(error));
			free(gimData);
			free(imageData);
			return EXIT_FAILURE;
		}
		else
		{
			printf("Saved %s\n", pngPath);
			free(gimData);
			free(imageData);
			return EXIT_SUCCESS;
		}
	}

	else if(mode == 'i') // inject png into gim
	{
        printf("Injecting %s into %s\n", pngPath, gimPath);
        unsigned int width, height;

        int err = lodepng_decode32_file(&pngData, &width, &height, pngPath);
        if(err)
		{
			printf("PNG Error %u: %s\n", err, lodepng_error_text(err));
			free(gimData);
			return EXIT_FAILURE;
		}

		if(width != header->width || height != header->height)
		{
			printf("Image size mismatch. PNG dimensions are %ux%u, but it needs to be %ux%u to be injected into the GIM.\n", width, height, header->width, header->height);
			return EXIT_FAILURE;
		}

        liq_attr *attr = liq_attr_create();

        if(header->type == TYPE_4BPP) // 4-bit color
		{
			printf("Reducing to 16 colors...\n");
			liq_set_max_colors(attr, 16);
		}
		else if (header->type == TYPE_8BPP)
		{
			printf("Reducing to 256 colors...\n");
		}
		else
		{
			printf("Unsupported image type 0x%02X.\n", header->type);
			return EXIT_FAILURE;
		}

		/* Quantize image and create new image map and palette */
		liq_image *image = liq_image_create_rgba(attr, pngData, width, height, 0);
		liq_result *res = liq_quantize_image(attr, image);

		if(header->type == TYPE_8BPP)
		{
			liq_write_remapped_image(res, image, (uint32_t *)header->dataOffset, width*height);
		}
		else if (header->type == TYPE_4BPP)
		{
			uint8_t *fulldata = malloc(width*height);
			liq_write_remapped_image(res, image, (uint32_t *)fulldata, width*height);

			// Convert to 4-bit.
			for(int i = 0; i < (width*height)/2; i++)
			{
				uint8_t pixel1 = *((uint8_t *)fulldata + i*2);
				uint8_t pixel2 = *((uint8_t *)fulldata + i*2 + 1);

				*(uint8_t *)(header->dataOffset+i) = pixel1 | (pixel2 << 4);
			}

			free(fulldata);
		}

		/* Update palette in GIM */
		const liq_palette *pal = liq_get_palette(res);
		memcpy((void *)header->paletteOffset, pal->entries, pal->count*4);

		for(int i = 0; i < pal->count; i++)
		{
			uint8_t alpha = (*((uint8_t *)header->paletteOffset + i*4) >> 24);
			if(alpha > 0)
				alpha = (alpha>>1)+1; // scale down to 0-128 range
		}

		if(header->type == TYPE_8BPP) // Swizzle image data and filter palette
		{
			unsigned char *original = malloc(width * height);
			memcpy(original, (uint8_t *)header->dataOffset, width * height);
			swizzle_8bpp(original, (uint8_t *)header->dataOffset, width, height);
			free(original);

			uint32_t *filtered = malloc(256 * 4);
			PaletteFilter((uint32_t *) header->paletteOffset, filtered, 256);
			memcpy((void *) header->paletteOffset, filtered, 256 * 4);
			free(filtered);
		}

        /* Return offsets to original values */
        header->paletteOffset -= (uint32_t)gimData;
        header->dataOffset -= (uint32_t)gimData;

		printf("Saving changes\n", gimData);
		fGim = fopen(gimPath, "wb");
        if(!fGim)
			printf("Failed to open gim...\n");
        fwrite(gimData, 1, gimLength, fGim);
        fclose(fGim);

        liq_attr_destroy(attr);
        liq_image_destroy(image);
        liq_result_destroy(res);
        free(pngData);
        free(gimData);
	}
	else
		printf("Invalid mode '%c'. Valid modes are e and i.\n", mode);

    return 0;
}
