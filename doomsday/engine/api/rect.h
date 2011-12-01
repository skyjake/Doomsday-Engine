/**\file rect.h
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

#ifndef LIBDENG_DATA_RECT_H
#define LIBDENG_DATA_RECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "point.h"
#include "size.h"

/**
 * RectRawi. Rectangle Raw (i)integer. Is intended as a handy POD
 * structure for easy manipulation of rectangles using integer precison.
 */
typedef struct RectRawi_s {
    Point2Rawi origin;
    Size2Rawi size;
} RectRawi;

/**
 * Recti. Class for manipulating rectangles using integer precision.
 */
struct recti_s; // The Recti instance (opaque).
typedef struct recti_s Recti;

Recti* Recti_New(void);
Recti* Recti_NewWithOriginSize(const Point2i* origin, const Size2i* size);
Recti* Recti_NewWithOriginSize2(int x, int y, int width, int height);
Recti* Recti_NewFromRaw(const RectRawi* rawRect);

void Recti_Delete(Recti* rect);

boolean Recti_IsNull(const Recti* rect);

int Recti_Width(const Recti* rect);

int Recti_Height(const Recti* rect);

void Recti_SetWidth(Recti* rect, int width);

void Recti_SetHeight(Recti* rect, int height);

const Point2i* Recti_Origin(const Recti* rect);

void Recti_SetOrigin(Recti* r, const Point2i* origin);
void Recti_SetOrigin2(Recti* r, int x, int y);

const Size2i* Recti_Size(const Recti* rect);

void Recti_SetSize(Recti* rect, const Size2i* size);
void Recti_SetSize2(Recti* rect, int width, int height);

boolean Recti_Equality(const Recti* rect, const Recti* other);

/**
 * RectRawf. Rectangle Raw (f)loating point. Is intended as a handy
 * POD structure for easy manipulation of rectangles using floating
 * point precison.
 */
typedef struct RectRawf_s {
    Point2Rawf origin;
    Size2Rawf size;
} RectRawf;

/**
 * Rectf. Class for manipulating rectangles using floating point precision.
 */
struct rectf_s; // The Rectf instance (opaque).
typedef struct rectf_s Rectf;

Rectf* Rectf_New(void);
Rectf* Rectf_NewWithOriginSize(const Point2f* origin, const Size2f* size);
Rectf* Rectf_NewFromRaw(const RectRawf* rawRect);

void Rectf_Delete(Rectf* rect);

boolean Rectf_IsNull(const Rectf* rect);

double Rectf_Width(const Rectf* rect);

double Rectf_Height(const Rectf* rect);

void Rectf_SetWidth(Rectf* rect, double width);

void Rectf_SetHeight(Rectf* rect, double height);

const Point2f* Rectf_Origin(const Rectf* rect);

void Rectf_SetOrigin(Rectf* rect, const Point2f* origin);
void Rectf_SetOrigin2(Rectf* r, double x, double y);

const Size2f* Rectf_Size(const Rectf* rect);
void Rectf_SetSize2(Rectf* rect, double width, double height);

void Rectf_SetSize(Rectf* rect, const Size2f* size);

boolean Rectf_Equality(const Rectf* rect, const Rectf* other);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_RECT_H */
