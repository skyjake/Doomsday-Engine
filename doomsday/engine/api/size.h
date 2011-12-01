/**\file size.h
 *\section License
 * License: GPL
 * Online License Link: http://www.trolltech.com/gpl/
 *
 *\author Daniel Swanson <danij@dengine.net>
 *\author Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
 *
 * This class is based on the semantics defined for the QUrl class, a component
 * of the Qt GUI Toolkit.
 *
 * \todo Derive from Qt::QUrl
 */

#ifndef LIBDENG_DATA_SIZE_H
#define LIBDENG_DATA_SIZE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Size2Rawi. Size 2 Raw (i)integer. Is intended as a handy POD structure
 * for easy manipulation of two dimensional sizes with integer precison.
 */
typedef struct Size2Rawi_s {
    union {
        struct {
            int width;
            int height;
        };
        int widthHeight[2];
    };
} Size2Rawi;

/**
 * Size2i. Class for manipulating two dimensional sizes using integer
 * precision.
 */
struct size2i_s; // The Size2i instance (opaque).
typedef struct size2i_s Size2i;

Size2i* Size2i_New(void);
Size2i* Size2i_NewWithDimensions(int width, int height);
Size2i* Size2i_NewFromRaw(const Size2Rawi* rawSize);
void Size2i_Delete(Size2i* size);

boolean Size2i_IsNull(const Size2i* size);

const Size2Rawi* Size2i_ToRaw(const Size2i* size);

int Size2i_Width(const Size2i* size);
int Size2i_Height(const Size2i* size);

void Size2i_SetWidth(Size2i* size, int width);
void Size2i_SetHeight(Size2i* size, int height);

const int* Size2i_WidthHeight(const Size2i* size);
void Size2i_SetWidthHeight(Size2i* size, int width, int height);

void Size2i_Sum(Size2i* size, const Size2i* other);

boolean Size2i_Equality(const Size2i* point, const Size2i* other);

/**
 * Size2Rawf. Size 2 Raw (f)loating point. Is intended as a handy POD
 * structure for easy manipulation of two dimensional sizes with floating
 * point precison.
 */
typedef struct Size2Rawf_s {
    union {
        struct {
            double width;
            double height;
        };
        double widthHeight[2];
    };
} Size2Rawf;

/**
 * Size2f. Class for manipulating two dimensional sizes using floating
 * point precision.
 */
struct size2f_s; // The Size2f instance (opaque).
typedef struct size2f_s Size2f;

Size2f* Size2f_New(void);
Size2f* Size2f_NewWithDimensions(double width, double height);
Size2f* Size2f_NewFromRaw(const Size2Rawf* rawSize);
void Size2f_Delete(Size2f* size);

boolean Size2f_IsNull(const Size2f* size);

const Size2Rawf* Size2f_ToRaw(const Size2f* size);

double Size2f_Width(const Size2f* size);
double Size2f_Height(const Size2f* size);

void Size2f_SetWidth(Size2f* size, double width);
void Size2f_SetHeight(Size2f* size, double height);

const double* Size2f_WidthHeight(const Size2f* size);
void Size2f_SetWidthHeight(Size2f* size, double width, double height);

void Size2f_Sum(Size2f* size, const Size2f* other);

boolean Size2f_Equality(const Size2f* size, const Size2f* other);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_SIZE_H */
