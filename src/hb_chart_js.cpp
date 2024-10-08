#pragma region Prolog
/*******************************************************************
$CRT 27 Sep 2024 : hb

$AUT Holger Burkarth
$DAT >>hb_chart_js.cpp<< 07 Okt 2024  11:30:21 - (c) proDAD
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
        this.DataPoints = [];
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
        this.DataColor = "#ea0";
        this.DataLineWidth = 3;
        this.NoniusFont = "12px sans-serif";
        this.EventFont = "15px sans-serif";
        this.XDataToString = null;

        this.ctx = this.canvas.getContext("2d");
        this.ctx.font = this.NoniusFont;

        this.ResetGridColor();
        this.ResetAxisColors();
      }


      SetDataPoints(dataPoints)
      {
        this.DataPoints = dataPoints;

        if(dataPoints == null)
          return;
        if(dataPoints.length == 0)
          return;

        if(dataPoints[0].length < 2 && this.HrzAxisStart == 0 && this.HrzAxisEnd == 1)
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

      SetMargins(left, right, top, bottom)
      {
        this.LeftMargin = left;
        this.RightMargin = right;
        this.TopMargin = top;
        this.BottomMargin = bottom;
      }

      SetHrzMargins(left, right)
      {
        this.LeftMargin = left;
        this.RightMargin = right;
      }

      SetVrtMargins(top, bottom)
      {
        this.TopMargin = top;
        this.BottomMargin = bottom;
      }

      /*
      * mode: (combination of)
      *  - ""         : no auto margins
      *  - "label"    : has labels
      *  - "numbers"  : has numbers
      *  - "left"     : (only for vertical axis) left side
      *  - "right"    : (only for vertical axis) right side
      */
      AutoMargins(hrzAxisMode, vrtAxisMode)
      {
        if(hrzAxisMode == undefined) hrzAxisMode = "";
        if(vrtAxisMode == undefined) vrtAxisMode = "";

        var HrzLabel = hrzAxisMode.includes("label");
        var HrzNumbers = hrzAxisMode.includes("numbers");
        var VrtLabel = vrtAxisMode.includes("label");
        var VrtNumbers = vrtAxisMode.includes("numbers");
        var VrtLeft = vrtAxisMode.includes("left");
        var VrtRight = vrtAxisMode.includes("right");

        if(!VrtLeft && !VrtRight)
          VrtLeft = true;

        this.LeftMargin = 8;
        this.RightMargin = 8;
        this.TopMargin = 8;
        this.BottomMargin = 8;

        if(HrzLabel) this.BottomMargin += 18;
        if(HrzNumbers) this.BottomMargin += 18;

        if(VrtLeft)
        {
          if(VrtLabel) this.LeftMargin += 18;
          if(VrtNumbers) this.LeftMargin += 18;
        }
        if(VrtRight)
        {
          if(VrtLabel) this.RightMargin += 18;
          if(VrtNumbers) this.RightMargin += 18;
        }
      }

      ResetAxisColors()
      {
        this.NoniusColor = "#aaa";
        this.AxisNumberColor = "#fff";
      }
      SetAxisColors(noniusColor, axisNumberColor)
      {
        if(typeof noniusColor === "string" && axisNumberColor == undefined)
        {
          if(noniusColor == 'cyan')
          {
            noniusColor = '#0aa';
            axisNumberColor = '#0ff';
          }
          else if(noniusColor == 'magenta')
          {
            noniusColor = '#f4f';
            axisNumberColor = '#f8f';
          }
        }
        this.NoniusColor = noniusColor;
        this.AxisNumberColor = axisNumberColor;
      }


      ResetGridColor()
      {
        this.GridColor = "#444";
      }
      SetGridColor(gridColor)
      {
        if(typeof gridColor === "string")
        {
          if(gridColor == 'cyan')
            gridColor = '#044';
          else if(gridColor == 'magenta')
            gridColor = '#818';
        }
        this.GridColor = gridColor;
      }


      ResetDataColor()
      {
        this.DataColor = "#ea0";
      }
      SetDataColor(dataColor)
      {
        if(typeof dataColor === "string")
        {
          if(dataColor == 'cyan')
            dataColor = '#0ff';
          else if(dataColor == 'magenta')
            dataColor = '#f4f';
        }
        this.DataColor = dataColor;
      }


      Clear()
      {
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
      }

      HasXYData()
      {
        return this.DataPoints && this.DataPoints.length > 0 && this.DataPoints[0].length == 2;
      }

      DrawHrzGrid()
      {
        this.ctx.strokeStyle = this.GridColor;
        this.ctx.lineWidth = 1;
        var HasXYData = this.HasXYData();

        for(var i = this.HrzAxisStart; i <= this.HrzAxisEnd; i += this.HrzAxisStep)
        {
          var x = this.Data2CanvasX( HasXYData ? i : this.CanvasXDataAt(i) );
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

      CanvasXDataAt(dataIndex)
      {
        return dataIndex;
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
          dataIndex = this.CanvasXDataAt(dataIndex);

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
        var my = (y0 + y1) / 2 + (Level - 1) * (th + 2);
        x = this.Data2CanvasX(x);

        this.ctx.beginPath();
        this.ctx.moveTo(x, y0);
        this.ctx.lineTo(x, my + 8);
        this.ctx.moveTo(x, y1);
        this.ctx.lineTo(x, my - 8);
        this.ctx.strokeStyle = colorLine;
        this.ctx.lineWidth = 1;
        this.ctx.stroke();

        if(txt)
        {
          this.ctx.font = this.EventFont;

          var tw = this.ctx.measureText(txt).width + 2;

          this.ctx.fillStyle = colorBg;
          this.ctx.fillRect(x - tw / 2, my - th / 2 - 4, tw, th + 1);

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
        this.ctx.lineWidth = this.DataLineWidth;
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
        var HasXYData= this.HasXYData();

        this.ctx.strokeStyle = this.NoniusColor;
        for(var i = this.HrzAxisStart; i <= this.HrzAxisEnd; i += this.HrzAxisStep)
        {
          var x = this.Data2CanvasX(HasXYData ? i : this.CanvasXDataAt(i));
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
          var vx = HasXYData ? i : this.CanvasXDataAt(i);
          var x = this.Data2CanvasX(vx);
          var Txt = this.XDataToString ? this.XDataToString(vx) : vx.toFixed(0);
          this.ctx.fillText(Txt, x, y + 14);
        }
      }

      DrawVrtAxisNumbers(leftSide)
      {
        if(leftSide == undefined) leftSide = true;
        this.ctx.fillStyle = this.AxisNumberColor;
        this.ctx.textAlign = leftSide ? "right" : "left";
        this.ctx.font = this.NoniusFont;
        var x = this.Data2CanvasX(leftSide ? this.HrzAxisStart : this.HrzAxisEnd);

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
          this.ctx.fillText(i, x + (leftSide ? -6 : 8), y + 4);
        }
      }

      DrawVrtAxisLabel(labelText, leftSide, etage)
      {
        if(etage == undefined) etage = 1;
        if(leftSide == undefined) leftSide = true;
        this.ctx.textAlign = "left";
        this.ctx.fillStyle = this.NoniusColor;
        var tw = this.ctx.measureText(labelText).width;
        var x = this.Data2CanvasX(leftSide ? this.HrzAxisStart : this.HrzAxisEnd);
        var y = this.Data2CanvasY(this.VrtAxisEnd);

        if(leftSide)
          x = x - 4 - 18 * (etage);
        else
          x = x - 4 + 18 * (etage + 1)

        this.ctx.save();
        this.ctx.translate(x, this.TopMargin + tw + 4);
        this.ctx.rotate(-Math.PI / 2);
        this.ctx.fillText(labelText, 0, 0);
        this.ctx.restore();
      }

      DrawHrzAxisLabel(labelText, etage)
      {
        if(etage == undefined) etage = 1;
        this.ctx.textAlign = "right";
        this.ctx.fillStyle = this.NoniusColor;
        var x = this.Data2CanvasX(this.HrzAxisEnd);
        var y = this.Data2CanvasY(this.VrtAxisStart);
        this.ctx.fillText(labelText, x - 4, y + (etage + 1) * 16 - 4);
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
