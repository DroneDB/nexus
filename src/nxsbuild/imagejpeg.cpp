/*
Nexus

Copyright (C) 2026 Digipa

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#include "imageio.h"

#include <csetjmp>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include <jpeglib.h>
}

#include "../common/error.h"

namespace nx {

namespace {

//The pixels of an Image are 0xffRRGGBB integers: the bytes in memory are B, G, R, X on a little
//endian machine, and X, R, G, B on a big endian one. libjpeg-turbo converts from and to either.
#if defined(_MSC_VER) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
const J_COLOR_SPACE PixelColorSpace = JCS_EXT_BGRX;
#else
const J_COLOR_SPACE PixelColorSpace = JCS_EXT_XRGB;
#endif

//libjpeg reports an error by calling error_exit, which must not return: it jumps back to the
//setjmp of the function that called into libjpeg. No object with a destructor is created
//between that setjmp and a libjpeg call, so the jump skips none.
struct JpegErrors: jpeg_error_mgr {
	jmp_buf jump;
	char message[JMSG_LENGTH_MAX];
};

extern "C" void jpegErrorExit(j_common_ptr cinfo) {
	JpegErrors *errors = static_cast<JpegErrors *>(cinfo->err);
	(*cinfo->err->format_message)(cinfo, errors->message);
	longjmp(errors->jump, 1);
}

JpegErrors *useErrors(JpegErrors &errors) {
	jpeg_std_error(&errors);
	errors.error_exit = jpegErrorExit;
	errors.message[0] = 0;
	return &errors;
}

//Destroys a libjpeg object at the end of the scope, the way out of the function included.
struct Decompressor {
	jpeg_decompress_struct info;
	bool created = false;
	~Decompressor() { if(created) jpeg_destroy_decompress(&info); }
};

struct Compressor {
	jpeg_compress_struct info;
	unsigned char *output = nullptr; //malloc'ed by libjpeg's memory destination
	bool created = false;
	~Compressor() {
		if(created)
			jpeg_destroy_compress(&info);
		std::free(output);
	}
};

//the first scanline of a 4 component image, Adobe style: components are stored inverted, so a
//color is the component scaled by the black one
void cmykToPixels(const unsigned char *in, uint32_t *out, unsigned width) {
	for(unsigned i = 0; i < width; i++, in += 4) {
		const unsigned k = in[3];
		out[i] = Image::rgb(k * in[0] / 255, k * in[1] / 255, k * in[2] / 255);
	}
}

//false, and the reason in error, when the data is not a JPEG that can be decoded
bool decodeInto(const unsigned char *data, size_t size, Image &image, std::string &error) {
	Decompressor decompressor;
	JpegErrors errors;
	decompressor.info.err = useErrors(errors);
	if(setjmp(errors.jump)) {
		error = errors.message;
		return false;
	}
	jpeg_create_decompress(&decompressor.info);
	decompressor.created = true;
	jpeg_decompress_struct &info = decompressor.info;
	jpeg_mem_src(&info, data, static_cast<unsigned long>(size));
	jpeg_read_header(&info, TRUE);
	//Whatever the color space of the file, the decoder hands out opaque RGB; only 4 component
	//(CMYK) files keep their components, which are combined below.
	const bool fourComponents = info.num_components == 4;
	if(!fourComponents)
		info.out_color_space = PixelColorSpace;

	jpeg_calc_output_dimensions(&info);
	if(info.output_width == 0 || info.output_height == 0) {
		error = "the image is empty";
		return false;
	}
	image = Image(static_cast<int>(info.output_width), static_cast<int>(info.output_height));
	JSAMPARRAY row = nullptr;
	if(fourComponents)
		row = (*info.mem->alloc_sarray)(reinterpret_cast<j_common_ptr>(&info), JPOOL_IMAGE, info.output_width * 4, 1);

	jpeg_start_decompress(&info);
	while(info.output_scanline < info.output_height) {
		uint32_t *pixels = image.scanLine(static_cast<int>(info.output_scanline));
		if(fourComponents) {
			jpeg_read_scanlines(&info, row, 1);
			cmykToPixels(row[0], pixels, info.output_width);
		} else {
			JSAMPROW target = reinterpret_cast<JSAMPROW>(pixels);
			jpeg_read_scanlines(&info, &target, 1);
		}
	}
	jpeg_finish_decompress(&info);
	return true;
}

//false, and the reason in error, when the image cannot be encoded
bool encodeInto(const Image &image, int quality, bool optimize, bool progressive,
                std::vector<unsigned char> &jpeg, std::string &error) {
	Compressor compressor;
	JpegErrors errors;
	compressor.info.err = useErrors(errors);
	if(setjmp(errors.jump)) {
		error = errors.message;
		return false;
	}
	jpeg_create_compress(&compressor.info);
	compressor.created = true;
	jpeg_compress_struct &info = compressor.info;
	unsigned long outputSize = 0;
	jpeg_mem_dest(&info, &compressor.output, &outputSize);

	info.image_width = static_cast<JDIMENSION>(image.width());
	info.image_height = static_cast<JDIMENSION>(image.height());
	info.input_components = 4;
	info.in_color_space = PixelColorSpace;
	jpeg_set_defaults(&info);
	//the density Qt wrote: 100 dots per inch
	info.density_unit = 1;
	info.X_density = 100;
	info.Y_density = 100;
	if(optimize)
		info.optimize_coding = TRUE;
	if(progressive)
		jpeg_simple_progression(&info);
	jpeg_set_quality(&info, quality >= 0 ? (quality < 100 ? quality : 100) : 75, TRUE);

	jpeg_start_compress(&info, TRUE);
	while(info.next_scanline < info.image_height) {
		JSAMPROW row = reinterpret_cast<JSAMPROW>(const_cast<uint32_t *>(image.scanLine(static_cast<int>(info.next_scanline))));
		jpeg_write_scanlines(&info, &row, 1);
	}
	jpeg_finish_compress(&info);
	jpeg.assign(compressor.output, compressor.output + outputSize);
	return true;
}

} //namespace

Image decodeJpeg(const unsigned char *data, size_t size, std::string &error) {
	Image image;
	if(!decodeInto(data, size, image, error))
		return Image();
	return image;
}

std::vector<unsigned char> encodeJpeg(const Image &image, int quality, bool optimize, bool progressive) {
	if(image.isNull())
		throw Error("there is no image to encode");
	std::vector<unsigned char> jpeg;
	std::string error;
	if(!encodeInto(image, quality, optimize, progressive, jpeg, error))
		throw Error("could not encode a JPEG image: " + error);
	return jpeg;
}

} //namespace
