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
 * Size2Raw. Size 2 Raw (integer). Is intended as a handy POD structure
 * for easy manipulation of two dimensional sizes with integer precison.
 */
typedef struct Size2Raw_s {
    union {
        struct {
            int width;
            int height;
        };
        int widthHeight[2];
    };
} Size2Raw;

/**
 * Size2. Class for manipulating two dimensional sizes using integer
 * precision.
 */
struct size2_s; // The Size2 instance (opaque).
typedef struct size2_s Size2;

Size2* Size2_New(void);
Size2* Size2_NewWithDimensions(int width, int height);
Size2* Size2_NewFromRaw(const Size2Raw* rawSize);
void Size2_Delete(Size2* size);

boolean Size2_IsNull(const Size2* size);

const Size2Raw* Size2_ToRaw(const Size2* size);

int Size2_Width(const Size2* size);
int Size2_Height(const Size2* size);

void Size2_SetWidth(Size2* size, int width);
void Size2_SetHeight(Size2* size, int height);

const int* Size2_WidthHeight(const Size2* size);
void Size2_SetWidthHeight(Size2* size, int width, int height);

void Size2_Sum(Size2* size, const Size2* other);

boolean Size2_Equality(const Size2* point, const Size2* other);

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
