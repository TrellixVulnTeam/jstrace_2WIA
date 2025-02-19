// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_INCLUDE_CFX_PATHDATA_H_
#define CORE_FXGE_INCLUDE_CFX_PATHDATA_H_

#include "core/fxcrt/include/fx_coordinates.h"
#include "core/fxcrt/include/fx_system.h"

struct FX_PATHPOINT {
  FX_FLOAT m_PointX;
  FX_FLOAT m_PointY;
  int m_Flag;
};

class CFX_PathData {
 public:
  CFX_PathData();
  CFX_PathData(const CFX_PathData& src);
  ~CFX_PathData();

  int GetPointCount() const { return m_PointCount; }
  int GetFlag(int index) const { return m_pPoints[index].m_Flag; }
  FX_FLOAT GetPointX(int index) const { return m_pPoints[index].m_PointX; }
  FX_FLOAT GetPointY(int index) const { return m_pPoints[index].m_PointY; }
  FX_PATHPOINT* GetPoints() const { return m_pPoints; }

  void SetPointCount(int nPoints);
  void AllocPointCount(int nPoints);
  void AddPointCount(int addPoints);
  CFX_FloatRect GetBoundingBox() const;
  CFX_FloatRect GetBoundingBox(FX_FLOAT line_width, FX_FLOAT miter_limit) const;
  void Transform(const CFX_Matrix* pMatrix);
  FX_BOOL IsRect() const;
  FX_BOOL GetZeroAreaPath(CFX_PathData& NewPath,
                          CFX_Matrix* pMatrix,
                          FX_BOOL& bThin,
                          FX_BOOL bAdjust) const;
  FX_BOOL IsRect(const CFX_Matrix* pMatrix, CFX_FloatRect* rect) const;
  void Append(const CFX_PathData* pSrc, const CFX_Matrix* pMatrix);
  void AppendRect(FX_FLOAT left, FX_FLOAT bottom, FX_FLOAT right, FX_FLOAT top);
  void SetPoint(int index, FX_FLOAT x, FX_FLOAT y, int flag);
  void TrimPoints(int nPoints);
  void Copy(const CFX_PathData& src);

 private:
  friend class CPDF_Path;

  int m_PointCount;
  FX_PATHPOINT* m_pPoints;
  int m_AllocCount;
};

#endif  // CORE_FXGE_INCLUDE_CFX_PATHDATA_H_
