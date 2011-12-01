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
 * RectRaw. Rectangle Raw (i)integer. Is intended as a handy POD
 * structure for easy manipulation of rectangles using integer precison.
 */
typedef struct RectRaw_s {
    Point2Raw origin;
    Size2Raw size;
} RectRaw;

/**
 * Rect. Class for manipulating rectangles using integer precision.
 */
struct rect_s; // The Rect instance (opaque).
typedef struct rect_s Rect;

Rect* Rect_New(void);
Rect* Rect_NewWithOriginSize(const Point2* origin, const Size2* size);
Rect* Rect_NewWithOriginSize2(int x, int y, int width, int height);
Rect* Rect_NewFromRaw(const RectRaw* rawRect);

void Rect_Delete(Rect* rect);

boolean Rect_IsNull(const Rect* rect);

int Rect_Width(const Rect* rect);

int Rect_Height(const Rect* rect);

void Rect_SetWidth(Rect* rect, int width);

void Rect_SetHeight(Rect* rect, int height);

const Point2* Rect_Origin(const Rect* rect);

void Rect_SetOrigin(Rect* r, const Point2* origin);
void Rect_SetOrigin2(Rect* r, int x, int y);

const Size2* Rect_Size(const Rect* rect);

void Rect_SetSize(Rect* rect, const Size2* size);
void Rect_SetSize2(Rect* rect, int width, int height);

boolean Rect_Equality(const Rect* rect, const Rect* other);

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
