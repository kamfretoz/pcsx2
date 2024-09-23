/*
 * Copyright (c) 2020 Nwutobo Samuel Ugochukwu <sammycageagle@gmail.com>
 * Copyright (c) 2024 Connor McLaughlin <stenzek@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LUNASVG_C_H
#define LUNASVG_C_H

#include <stdbool.h>
#include <stdint.h>

#if !defined(LUNASVG_BUILD_STATIC) && (defined(_WIN32) || defined(__CYGWIN__))
#define LUNASVG_EXPORT __declspec(dllexport)
#define LUNASVG_IMPORT __declspec(dllimport)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define LUNASVG_EXPORT __attribute__((__visibility__("default")))
#define LUNASVG_IMPORT
#else
#define LUNASVG_EXPORT
#define LUNASVG_IMPORT
#endif

#ifdef LUNASVG_BUILD
#include "lunasvg.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LUNASVG_BUILD
typedef lunasvg::Box lunasvg_box;
typedef lunasvg::Matrix lunasvg_matrix;
typedef lunasvg::Bitmap lunasvg_bitmap;
typedef lunasvg::Document lunasvg_document;

#define LUNASVG_API LUNASVG_EXPORT
#else
typedef void* lunasvg_box;
typedef void* lunasvg_matrix;
typedef void* lunasvg_bitmap;
typedef void* lunasvg_document;
#define LUNASVG_API LUNASVG_IMPORT
#endif

LUNASVG_API lunasvg_box* lunasvg_box_create();
LUNASVG_API lunasvg_box* lunasvg_box_create_with_values(double x, double y, double w, double h);
LUNASVG_API lunasvg_box* lunasvg_box_create_from_rect(const lunasvg_box* rect);
LUNASVG_API void lunasvg_box_destroy(lunasvg_box* box);
LUNASVG_API void lunasvg_box_transform(lunasvg_box* box, const lunasvg_matrix* matrix);
LUNASVG_API lunasvg_box* lunasvg_box_transformed(const lunasvg_box* box, const lunasvg_matrix* matrix);
LUNASVG_API void lunasvg_box_get_values(const lunasvg_box* box, double* x, double* y, double* w, double* h);
LUNASVG_API double lunasvg_box_get_x(const lunasvg_box* box);
LUNASVG_API double lunasvg_box_get_y(const lunasvg_box* box);
LUNASVG_API double lunasvg_box_get_width(const lunasvg_box* box);
LUNASVG_API double lunasvg_box_get_height(const lunasvg_box* box);
LUNASVG_API void lunasvg_box_set_x(lunasvg_box* box, double x);
LUNASVG_API void lunasvg_box_set_y(lunasvg_box* box, double y);
LUNASVG_API void lunasvg_box_set_width(lunasvg_box* box, double width);
LUNASVG_API void lunasvg_box_set_height(lunasvg_box* box, double height);

LUNASVG_API lunasvg_matrix* lunasvg_matrix_create();
LUNASVG_API lunasvg_matrix* lunasvg_matrix_create_with_values(double a, double b, double c, double d, double e,
                                                              double f);
LUNASVG_API void lunasvg_matrix_destroy(lunasvg_matrix* matrix);
LUNASVG_API void lunasvg_matrix_rotate(lunasvg_matrix* matrix, double angle);
LUNASVG_API void lunasvg_matrix_rotate_around_point(lunasvg_matrix* matrix, double angle, double cx, double cy);
LUNASVG_API void lunasvg_matrix_scale(lunasvg_matrix* matrix, double sx, double sy);
LUNASVG_API void lunasvg_matrix_shear(lunasvg_matrix* matrix, double shx, double shy);
LUNASVG_API void lunasvg_matrix_translate(lunasvg_matrix* matrix, double tx, double ty);
LUNASVG_API void lunasvg_matrix_transform(lunasvg_matrix* matrix, double a, double b, double c, double d, double e,
                                          double f);
LUNASVG_API void lunasvg_matrix_identity(lunasvg_matrix* matrix);
LUNASVG_API void lunasvg_matrix_invert(lunasvg_matrix* matrix);
LUNASVG_API void lunasvg_matrix_multiply(lunasvg_matrix* matrix, const lunasvg_matrix* rhs);
LUNASVG_API void lunasvg_matrix_premultiply(lunasvg_matrix* matrix, const lunasvg_matrix* rhs);
LUNASVG_API void lunasvg_matrix_postmultiply(lunasvg_matrix* matrix, const lunasvg_matrix* rhs);
LUNASVG_API lunasvg_matrix* lunasvg_matrix_inverted(const lunasvg_matrix* matrix);
LUNASVG_API lunasvg_matrix* lunasvg_matrix_rotated(double angle);
LUNASVG_API lunasvg_matrix* lunasvg_matrix_rotated_around_point(double angle, double cx, double cy);
LUNASVG_API lunasvg_matrix* lunasvg_matrix_scaled(double sx, double sy);
LUNASVG_API lunasvg_matrix* lunasvg_matrix_sheared(double shx, double shy);
LUNASVG_API lunasvg_matrix* lunasvg_matrix_translated(double tx, double ty);
LUNASVG_API void lunasvg_matrix_get_values(const lunasvg_matrix* matrix, double* a, double* b, double* c, double* d,
                                           double* e, double* f);

LUNASVG_API lunasvg_bitmap* lunasvg_bitmap_create();
LUNASVG_API lunasvg_bitmap* lunasvg_bitmap_create_with_data(uint8_t* data, uint32_t width, uint32_t height,
                                                            uint32_t stride);
LUNASVG_API lunasvg_bitmap* lunasvg_bitmap_create_with_size(uint32_t width, uint32_t height);
LUNASVG_API void lunasvg_bitmap_destroy(lunasvg_bitmap* bitmap);
LUNASVG_API void lunasvg_bitmap_reset_with_data(lunasvg_bitmap* bitmap, uint8_t* data, uint32_t width, uint32_t height,
                                                uint32_t stride);
LUNASVG_API void lunasvg_bitmap_reset_with_size(lunasvg_bitmap* bitmap, uint32_t width, uint32_t height);
LUNASVG_API uint8_t* lunasvg_bitmap_data(const lunasvg_bitmap* bitmap);
LUNASVG_API uint32_t lunasvg_bitmap_width(const lunasvg_bitmap* bitmap);
LUNASVG_API uint32_t lunasvg_bitmap_height(const lunasvg_bitmap* bitmap);
LUNASVG_API uint32_t lunasvg_bitmap_stride(const lunasvg_bitmap* bitmap);
LUNASVG_API void lunasvg_bitmap_clear(lunasvg_bitmap* bitmap, uint32_t color);
LUNASVG_API void lunasvg_bitmap_convert(lunasvg_bitmap* bitmap, int ri, int gi, int bi, int ai, int unpremultiply);
LUNASVG_API void lunasvg_bitmap_convert_to_rgba(lunasvg_bitmap* bitmap);
LUNASVG_API bool lunasvg_bitmap_valid(const lunasvg_bitmap* bitmap);

LUNASVG_API lunasvg_document* lunasvg_document_load_from_file(const char* filename);
LUNASVG_API lunasvg_document* lunasvg_document_load_from_data(const void* data, size_t size);
LUNASVG_API lunasvg_document* lunasvg_document_load_from_string(const char* data);
LUNASVG_API void lunasvg_document_destroy(lunasvg_document* document);
LUNASVG_API void lunasvg_document_set_matrix(lunasvg_document* document, const lunasvg_matrix* matrix);
LUNASVG_API void lunasvg_document_set_identity_matrix(lunasvg_document* document);
LUNASVG_API void lunasvg_document_get_matrix(const lunasvg_document* document, lunasvg_matrix* matrix);
LUNASVG_API void lunasvg_document_get_box(const lunasvg_document* document, lunasvg_box* box);
LUNASVG_API double lunasvg_document_get_width(const lunasvg_document* document);
LUNASVG_API double lunasvg_document_get_height(const lunasvg_document* document);
LUNASVG_API void lunasvg_document_render(const lunasvg_document* document, lunasvg_bitmap* bitmap,
                                         const lunasvg_matrix* matrix);
LUNASVG_API lunasvg_bitmap* lunasvg_document_render_to_bitmap(const lunasvg_document* document, uint32_t width,
                                                              uint32_t height, uint32_t background_color);
LUNASVG_API void lunasvg_document_update_layout(lunasvg_document* document);

#ifdef __cplusplus
}
#endif

#endif // LUNASVG_H
