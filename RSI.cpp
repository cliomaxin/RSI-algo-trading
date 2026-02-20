//+------------------------------------------------------------------+
//|                                     RSI_with_Divergence_v6.mq5   |
//|                                  Copyright 2024, Trading AI.     |
//+------------------------------------------------------------------+
#property copyright "Custom Conversion"
#property link      "https://www.tradingview.com"
#property version   "1.00"
#property indicator_separate_window
#property indicator_buffers 7
#property indicator_plots   6

// Plot RSI
#property indicator_label1  "RSI"
#property indicator_type1   DRAW_LINE
#property indicator_color1  clrMediumPurple
#property indicator_width1  2

// Plot MA
#property indicator_label2  "RSI-based MA"
#property indicator_type2   DRAW_LINE
#property indicator_color2  clrYellow

// Plot Bollinger Bands
#property indicator_label3  "Upper BB"
#property indicator_type3   DRAW_LINE
#property indicator_color3  clrGreen
#property indicator_label4  "Lower BB"
#property indicator_type4   DRAW_LINE
#property indicator_color4  clrGreen

// Divergence Arrows
#property indicator_label5  "Bullish Divergence"
#property indicator_type5   DRAW_ARROW
#property indicator_color5  clrGreen
#property indicator_label6  "Bearish Divergence"
#property indicator_type6   DRAW_ARROW
#property indicator_color6  clrRed

// Enums for Dropdowns
enum ENUM_MA_TYPE {
   MA_NONE,
   MA_SMA,
   MA_SMA_BB, // SMA + Bollinger Bands
   MA_EMA,
   MA_SMMA,
   MA_WMA
};

// --- Inputs ---
input int            InpRsiLength = 14;          // RSI Length
input ENUM_APPLIED_PRICE InpSource = PRICE_CLOSE; // Source
input bool           InpCalcDiv   = true;        // Calculate Divergence

input group "Smoothing"
input ENUM_MA_TYPE   InpMaType    = MA_SMA;      // Moving Average Type
input int            InpMaLength  = 14;          // MA Length
input double         InpBbMult    = 2.0;         // BB StdDev (for BB type)

// --- Buffers ---
double BufferRSI[];
double BufferMA[];
double BufferUpperBB[];
double BufferLowerBB[];
double BufferBull[];
double BufferBear[];
double BufferRsiRaw[]; // Calculation buffer

//+------------------------------------------------------------------+
//| Custom indicator initialization function                         |
//+------------------------------------------------------------------+
int OnInit()
{
   // Map Buffers
   SetIndexBuffer(0, BufferRSI, INDICATOR_DATA);
   SetIndexBuffer(1, BufferMA, INDICATOR_DATA);
   SetIndexBuffer(2, BufferUpperBB, INDICATOR_DATA);
   SetIndexBuffer(3, BufferLowerBB, INDICATOR_DATA);
   SetIndexBuffer(4, BufferBull, INDICATOR_DATA);
   SetIndexBuffer(5, BufferBear, INDICATOR_DATA);
   SetIndexBuffer(6, BufferRsiRaw, INDICATOR_CALCULATIONS);

   // Set Arrow Codes (Wingdings)
   PlotIndexSetInteger(4, PLOT_ARROW, 233); // Arrow up
   PlotIndexSetInteger(5, PLOT_ARROW, 234); // Arrow down
   
   // Clean visual look
   IndicatorSetString(INDICATOR_SHORTNAME, "RSI Divergence V6");
   IndicatorSetInteger(INDICATOR_DIGITS, 2);

   return(INIT_SUCCEEDED);
}

//+------------------------------------------------------------------+
//| Custom indicator iteration function                              |
//+------------------------------------------------------------------+
int OnCalculate(const int rates_total,
                const int prev_calculated,
                const datetime &time[],
                const double &open[],
                const double &high[],
                const double &low[],
                const double &close[],
                const long &tick_volume[],
                const long &volume[],
                const int &spread[])
{
   int start = (prev_calculated > 0) ? prev_calculated - 1 : 0;

   // 1. RSI Calculation (Manual RMA to match Pine Script)
   static double last_up = 0, last_down = 0;
   
   for(int i = start; i < rates_total && !IsStopped(); i++)
   {
      BufferBull[i] = EMPTY_VALUE;
      BufferBear[i] = EMPTY_VALUE;

      if(i == 0) {
         BufferRSI[i] = 50;
         continue;
      }
      
      double change = close[i] - close[i-1];
      double up = MathMax(change, 0);
      double down = -MathMin(change, 0);
      
      double alpha = 1.0 / InpRsiLength;
      last_up = (i == 1) ? up : alpha * up + (1 - alpha) * last_up;
      last_down = (i == 1) ? down : alpha * down + (1 - alpha) * last_down;
      
      BufferRSI[i] = (last_down == 0) ? 100 : (last_up == 0) ? 0 : 100 - (100 / (1 + last_up / last_down));
   }

   // 2. MA Smoothing & Bollinger Bands
   for(int i = start; i < rates_total; i++)
   {
      if(i < InpMaLength) {
         BufferMA[i] = EMPTY_VALUE;
         continue;
      }

      double sum = 0;
      for(int j = 0; j < InpMaLength; j++) sum += BufferRSI[i-j];
      double sma = sum / InpMaLength;
      
      BufferMA[i] = (InpMaType == MA_NONE) ? EMPTY_VALUE : sma;

      if(InpMaType == MA_SMA_BB)
      {
         double stdDev = 0;
         for(int j = 0; j < InpMaLength; j++) stdDev += MathPow(BufferRSI[i-j] - sma, 2);
         stdDev = MathSqrt(stdDev / InpMaLength);
         
         BufferUpperBB[i] = sma + (stdDev * InpBbMult);
         BufferLowerBB[i] = sma - (stdDev * InpBbMult);
      }
      else {
         BufferUpperBB[i] = EMPTY_VALUE;
         BufferLowerBB[i] = EMPTY_VALUE;
      }
   }

   // 3. Simple Divergence Detection
   if(InpCalcDiv && rates_total > 30)
   {
      int lb = 5; // Lookback
      for(int i = start; i < rates_total - lb; i++)
      {
         if(i < 20) continue;
         
         // Bullish Divergence Check
         if(low[i-lb] < low[i-lb-10] && BufferRSI[i-lb] > BufferRSI[i-lb-10])
         {
             if(BufferRSI[i-lb] < 35) // Only in oversold territory
                BufferBull[i-lb] = BufferRSI[i-lb] - 5;
         }
         
         // Bearish Divergence Check
         if(high[i-lb] > high[i-lb-10] && BufferRSI[i-lb] < BufferRSI[i-lb-10])
         {
             if(BufferRSI[i-lb] > 65) // Only in overbought territory
                BufferBear[i-lb] = BufferRSI[i-lb] + 5;
         }
      }
   }

   return(rates_total);
}