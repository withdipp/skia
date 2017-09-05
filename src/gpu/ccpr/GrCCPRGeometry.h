/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGrCCPRGeometry_DEFINED
#define GrGrCCPRGeometry_DEFINED

#include "SkNx.h"
#include "SkPoint.h"
#include "SkTArray.h"

struct SkDCubic;
enum class SkCubicType;

/**
 * This class chops device-space contours up into a series of segments that CCPR knows how to
 * render. (See GrCCPRGeometry::Verb.)
 *
 * NOTE: This must be done in device space, since an affine transformation can change whether a
 * curve is monotonic.
 */
class GrCCPRGeometry {
public:
    // These are the verbs that CCPR knows how to draw. If a path has any segments that don't map to
    // this list, then they are chopped into smaller ones that do. A list of these comprise a
    // compact representation of what can later be expanded into GPU instance data.
    enum class Verb : uint8_t {
        kBeginPath, // Included only for caller convenience.
        kBeginContour,
        kLineTo,
        kMonotonicQuadraticTo, // Monotonic relative to the vector between its endpoints [P2 - P0].
        kConvexSerpentineTo,
        kConvexLoopTo,
        kEndClosedContour, // endPt == startPt.
        kEndOpenContour // endPt != startPt.
    };

    // These tallies track numbers of CCPR primitives are required to draw a contour.
    struct PrimitiveTallies {
        int fTriangles; // Number of triangles in the contour's fan.
        int fQuadratics;
        int fSerpentines;
        int fLoops;

        void operator+=(const PrimitiveTallies&);
        PrimitiveTallies operator-(const PrimitiveTallies&) const;
    };

    GrCCPRGeometry(int numSkPoints = 0, int numSkVerbs = 0)
            : fPoints(numSkPoints * 3) // Reserve for a 3x expansion in points and verbs.
            , fVerbs(numSkVerbs * 3) {}

    const SkTArray<SkPoint, true>& points() const { SkASSERT(!fBuildingContour); return fPoints; }
    const SkTArray<Verb, true>& verbs() const { SkASSERT(!fBuildingContour); return fVerbs; }

    void reset() {
        SkASSERT(!fBuildingContour);
        fPoints.reset();
        fVerbs.reset();
    }

    // This is included in case the caller needs to discard previously added contours. It is up to
    // the caller to track counts and ensure we don't pop back into the middle of a different
    // contour.
    void resize_back(int numPoints, int numVerbs) {
        SkASSERT(!fBuildingContour);
        fPoints.resize_back(numPoints);
        fVerbs.resize_back(numVerbs);
        SkASSERT(fVerbs.empty() || fVerbs.back() == Verb::kEndOpenContour ||
                 fVerbs.back() == Verb::kEndClosedContour);
    }

    void beginPath();
    void beginContour(const SkPoint& devPt);
    void lineTo(const SkPoint& devPt);
    void quadraticTo(const SkPoint& devP1, const SkPoint& devP2);
    void cubicTo(const SkPoint& devP1, const SkPoint& devP2, const SkPoint& devP3);
    PrimitiveTallies endContour(); // Returns the numbers of primitives needed to draw the contour.

private:
    inline void appendMonotonicQuadratic(const Sk2f& p1, const Sk2f& p2);
    inline void appendConvexCubic(SkCubicType, const SkDCubic&);

    // Transient state used while building a contour.
    SkPoint                         fCurrAnchorPoint;
    SkPoint                         fCurrFanPoint;
    PrimitiveTallies                fCurrContourTallies;
    SkDEBUGCODE(bool                fBuildingContour = false);

    // TODO: These points could eventually be written directly to block-allocated GPU buffers.
    SkSTArray<128, SkPoint, true>   fPoints;
    SkSTArray<128, Verb, true>      fVerbs;
};

inline void GrCCPRGeometry::PrimitiveTallies::operator+=(const PrimitiveTallies& b) {
    fTriangles += b.fTriangles;
    fQuadratics += b.fQuadratics;
    fSerpentines += b.fSerpentines;
    fLoops += b.fLoops;
}

GrCCPRGeometry::PrimitiveTallies
inline GrCCPRGeometry::PrimitiveTallies::operator-(const PrimitiveTallies& b) const {
    return {fTriangles - b.fTriangles,
            fQuadratics - b.fQuadratics,
            fSerpentines - b.fSerpentines,
            fLoops - b.fLoops};
}

#endif
