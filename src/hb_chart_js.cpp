#pragma region Prolog
/*******************************************************************
$CRT 27 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_chart_js.cpp<< 27 Sep 2024  15:52:20 - (c) proDAD
*******************************************************************/
#pragma endregion
#pragma region Spelling
// Ignore Spelling:
#pragma endregion
#pragma region Includes
#include <Arduino.h>
#include "hb_homekit.h"

namespace HBHomeKit
{
namespace
{
#pragma endregion


#pragma region Chart_JavaScript
} // namespace

/* JavaScript code for drawing a chart.
* The code is used by the CChart class.
* @example
* var Chart = CChart(canvasElement);
*
* @member field LeftMargin
* @member field RightMargin
* @member field TopMargin
* @member field BottomMargin
* @member field GridColor
* @member field NoniusColor
* @member field DataColor
* @member field AxisNumberColor
*
* @param dataPoints Array of data points.
*        Each data point is an array with two values [x, y] or one value y.
* @member function SetDataPoints(dataPoints)
*
* @member function SetHrzAxis(start, end, step)
* @member function SetVrtAxis(start, end, step)
* @member function SwapXYAxis()
* @member function Clear()
* @member function DrawHrzGrid()
* @member function DrawVrtGrid()
*
* @param trackNumber [0 .. 3]
* @member function DrawEvent(trackNumber, x, txt, colorLine, colorBg)
*
* @member function DrawCurve()
* @member function DrawDots()
* @member function DrawHrzAxisNumbers()
* @member function DrawVrtAxisNumbers()
* @member function DrawBoundaryBox()
*
*/
CTextEmitter Chart_JavaScript()
{
  return MakeTextEmitter(F(R"(
class CChart
{
  constructor(canvas)
  {
    this.canvas = canvas;
    this.LeftMargin = 25;
    this.RightMargin = 12;
    this.TopMargin = 12;
    this.BottomMargin = 25;
    this.HrzAxisStart = 0;
    this.HrzAxisEnd = 1;
    this.HrzAxisStep = 1;
    this.VrtAxisStart = 0;
    this.VrtAxisEnd = 10;
    this.VrtAxisStep = 1;
    this.BottomTop = false;
    this.SwapXYDataValues = false;
    this.GridColor = "#444";
    this.NoniusColor = "#aaa";
    this.DataColor = "#ea0";
    this.AxisNumberColor = "#fff";
    this.NoniusFont = "12px sans-serif";
    this.EventFont = "15px sans-serif";

    this.ctx = this.canvas.getContext("2d");
    this.ctx.font = this.NoniusFont;
  }

  SetDataPoints(dataPoints)
  {
    this.DataPoints = dataPoints;

    if(dataPoints == null)
      return;
    if(dataPoints.length == 0)
      return;

    if(dataPoints[0].length < 2)
      this.HrzAxisEnd = dataPoints.length - 1;
  }

  SetHrzAxis(start, end, step)
  {
    this.HrzAxisStart = start;
    this.HrzAxisEnd = end;
    this.HrzAxisStep = step;
  }

  SetVrtAxis(start, end, step)
  {
    this.VrtAxisStart = start;
    this.VrtAxisEnd = end;
    this.VrtAxisStep = step;
  }

  SwapXYAxis()
  {
    this.SwapXYDataValues = !this.SwapXYDataValues;
    var temp = this.HrzAxisStart;
    this.HrzAxisStart = this.VrtAxisStart;
    this.VrtAxisStart = temp;
    temp = this.HrzAxisEnd;
    this.HrzAxisEnd = this.VrtAxisEnd;
    this.VrtAxisEnd = temp;
    temp = this.HrzAxisStep;
    this.HrzAxisStep = this.VrtAxisStep;
    this.VrtAxisStep = temp;

  }

  Clear()
  {
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
  }

  DrawHrzGrid()
  {
    this.ctx.strokeStyle = this.GridColor;
    this.ctx.lineWidth = 1;

    for(var i = this.HrzAxisStart; i <= this.HrzAxisEnd; i += this.HrzAxisStep)
    {
      var x = this.Data2CanvasX(i);
      this.ctx.beginPath();
      this.ctx.moveTo(x, this.TopMargin);
      this.ctx.lineTo(x, this.canvas.height - this.BottomMargin);
      this.ctx.stroke();
    }
  }

  DrawVrtGrid()
  {
    this.ctx.strokeStyle = this.GridColor;
    this.ctx.lineWidth = 1;

    for(var i = this.VrtAxisStart; i <= this.VrtAxisEnd; i += this.VrtAxisStep)
    {
      var y = this.Data2CanvasY(i);
      this.ctx.beginPath();
      this.ctx.moveTo(this.LeftMargin, y);
      this.ctx.lineTo(this.canvas.width - this.RightMargin, y);
      this.ctx.stroke();
    }
  }

  Data2CanvasX(x)
  {
    return this.LeftMargin + (x - this.HrzAxisStart) * (this.canvas.width - (this.LeftMargin + this.RightMargin)) / (this.HrzAxisEnd - this.HrzAxisStart);
  }

  Data2CanvasY(y)
  {
    if(this.BottomTop)
      return this.TopMargin + (y - this.VrtAxisStart) * (this.canvas.height - (this.BottomMargin + this.TopMargin)) / (this.VrtAxisEnd - this.VrtAxisStart);
    else
      return this.canvas.height - this.BottomMargin - (y - this.VrtAxisStart) * (this.canvas.height - (this.BottomMargin + this.TopMargin)) / (this.VrtAxisEnd - this.VrtAxisStart);
  }

  CanvasPointAt(dataIndex)
  {
    var x, y;
    var Entry = this.DataPoints[dataIndex];

    /* Entry unterscheiden in [x,y] oder y */
    if(Entry.length == 2)
    {
      if(this.SwapXYDataValues)
      {
        x = this.Data2CanvasX(Entry[1]);
        y = this.Data2CanvasY(Entry[0]);
      }
      else
      {
        x = this.Data2CanvasX(Entry[0]);
        y = this.Data2CanvasY(Entry[1]);
      }
    }
    else
    {
      if(this.SwapXYDataValues)
      {
        x = this.Data2CanvasX(Entry);
        y = this.Data2CanvasY(dataIndex);
      }
      else
      {
        x = this.Data2CanvasX(dataIndex);
        y = this.Data2CanvasY(Entry);
      }
    }
    return [x, y];
  }

  DrawEvent(Level, x, txt, colorLine, colorBg)
  {
    var th = 15;
    var y0 = this.Data2CanvasY(this.VrtAxisStart);
    var y1 = this.Data2CanvasY(this.VrtAxisEnd);
    var my = (y0+y1)/2 + (Level-1)*(th+2);
    x = this.Data2CanvasX(x);

    this.ctx.beginPath();
    this.ctx.moveTo(x, y0);
    this.ctx.lineTo(x, my+8);
    this.ctx.moveTo(x, y1);
    this.ctx.lineTo(x, my-8);
    this.ctx.strokeStyle = colorLine;
    this.ctx.lineWidth = 1;
    this.ctx.stroke();

    if(txt)
    {
      this.ctx.font = this.EventFont;

      var tw = this.ctx.measureText(txt).width+2;

      this.ctx.fillStyle = colorBg;
      this.ctx.fillRect(x-tw/2, my-th/2-4, tw, th+1);

      this.ctx.fillStyle = "#fff";
      this.ctx.textAlign = "center";
      this.ctx.fillText(txt, x, my);
    }
  }

  DrawCurve()
  {
    if(!this.DataPoints)
      return;

    this.ctx.beginPath();

    for(var i = 0; i < this.DataPoints.length; ++i)
    {
      var [x, y] = this.CanvasPointAt(i);

      if(i == 0)
        this.ctx.moveTo(x, y);
      else
        this.ctx.lineTo(x, y);
    }

    this.ctx.strokeStyle = this.DataColor;
    this.ctx.lineWidth = 2;
    this.ctx.stroke();
  }

  DrawDots()
  {
    if(!this.DataPoints)
      return;

    this.ctx.fillStyle = this.DataColor;
    for(var i = 0; i < this.DataPoints.length; ++i)
    {
      var [x, y] = this.CanvasPointAt(i);

      this.ctx.beginPath();
      this.ctx.arc(x, y, 5, 0, 2 * Math.PI);
      this.ctx.fill();
    }
  }

  DrawHrzAxisNumbers()
  {
    this.ctx.fillStyle = this.AxisNumberColor;
    this.ctx.textAlign = "center";
    this.ctx.font = this.NoniusFont;
    var y = this.Data2CanvasY(this.VrtAxisStart);

    this.ctx.strokeStyle = this.NoniusColor;
    for(var i = this.HrzAxisStart; i <= this.HrzAxisEnd; i += this.HrzAxisStep)
    {
      var x = this.Data2CanvasX(i);
      this.ctx.beginPath();
      this.ctx.moveTo(x, y - 5);
      this.ctx.lineTo(x, y + 5);
      this.ctx.stroke();
    }

    {
      this.ctx.strokeStyle = this.NoniusColor;
      this.ctx.beginPath();
      this.ctx.moveTo(this.LeftMargin, y);
      this.ctx.lineTo(this.canvas.width - this.RightMargin, y);
      this.ctx.stroke();
    }

    for(var i = this.HrzAxisStart; i <= this.HrzAxisEnd; i += this.HrzAxisStep)
    {
      var x = this.Data2CanvasX(i);
      this.ctx.fillText(i, x, y + 14);
    }
  }

  DrawVrtAxisNumbers()
  {
    this.ctx.fillStyle = this.AxisNumberColor;
    this.ctx.textAlign = "right";
    this.ctx.font = this.NoniusFont;
    var x = this.Data2CanvasX(this.HrzAxisStart);

    this.ctx.strokeStyle = this.NoniusColor;
    for(var i = this.VrtAxisStart; i <= this.VrtAxisEnd; i += this.VrtAxisStep)
    {
      var y = this.Data2CanvasY(i);
      this.ctx.beginPath();
      this.ctx.moveTo(x - 5, y);
      this.ctx.lineTo(x + 5, y);
      this.ctx.stroke();
    }

    {
      this.ctx.strokeStyle = this.NoniusColor;
      this.ctx.beginPath();
      this.ctx.moveTo(x, this.TopMargin);
      this.ctx.lineTo(x, this.canvas.height - this.BottomMargin);
      this.ctx.stroke();
    }

    for(var i = this.VrtAxisStart; i <= this.VrtAxisEnd; i += this.VrtAxisStep)
    {
      var y = this.Data2CanvasY(i);
      this.ctx.fillText(i, x - 6, y + 4);
    }
  }

  DrawBoundaryBox(minValue, maxValue, fillColor, opacity)
  {
    var x0, x1, y0, y1;

    if(this.SwapXYDataValues)
    {
      x0 = this.Data2CanvasX(minValue);
      x1 = this.Data2CanvasX(maxValue);
      y0 = this.Data2CanvasY(this.VrtAxisStart);
      y1 = this.Data2CanvasY(this.VrtAxisEnd);
    }
    else
    {
      x0 = this.Data2CanvasX(this.HrzAxisStart);
      x1 = this.Data2CanvasX(this.HrzAxisEnd);
      y0 = this.Data2CanvasY(minValue);
      y1 = this.Data2CanvasY(maxValue);
    }

    this.ctx.fillStyle = fillColor;
    this.ctx.globalAlpha = opacity;
    this.ctx.fillRect(x0, y0, x1 - x0, y1 - y0);
    this.ctx.globalAlpha = 1.0;
  }

}

)"));
}

#pragma endregion


#pragma region Epilog
} // namespace HBHomeKit
#pragma endregion
