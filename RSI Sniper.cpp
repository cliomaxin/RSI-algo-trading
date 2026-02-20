//+------------------------------------------------------------------+
//|                                             RSISniper_V3_Snap.mq5 |
//|                                  Copyright 2026, AI Collaborator |
//+------------------------------------------------------------------+
#property strict
#include <Trade\Trade.mqh>

// --- Strategy Inputs ---
input int      InpRsiLength   = 14;          // RSI Period
input int      InpRsiOver     = 67;          // SELL Threshold
input int      InpRsiUnder    = 37;          // BUY Threshold
input double   InpLotSize     = 0.02;         // Trading Lot Size
input int      InpMagicNum    = 987654;      // Unique ID for this bot

// --- Heartbeat & Notification Inputs ---
input int      InpHeartbeatHrs = 4;          // Hours between status updates
input bool     InpNotifyTrade  = true;       // Notify on entry/exit

// --- Global Variables ---
CTrade   trade;
datetime lastHeartbeat = 0;
double   last_up = 0, last_down = 0;         // For the manual RSI calculation

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit()
{
   trade.SetExpertMagicNumber(InpMagicNum);
   EventSetTimer(60); // Check heartbeat every minute
   
   // Pre-calculate current RSI values to "warm up" the math
   WarmUpRSI();
   
   SendNotification("🚀 RSI Sniper Started | Buy < 37, Sell > 67");
   return(INIT_SUCCEEDED);
}

void OnDeinit(const int reason) { EventKillTimer(); }

//+------------------------------------------------------------------+
//| Calculate RSI (Using your manual smoothing logic)                |
//+------------------------------------------------------------------+
double CalculateManualRSI()
{
   double close[];
   ArraySetAsSeries(close, true);
   if(CopyClose(_Symbol, _Period, 0, 2, close) < 2) return 50.0;

   double change = close[0] - close[1];
   double up = MathMax(change, 0);
   double down = -MathMin(change, 0);
   
   double alpha = 1.0 / InpRsiLength;
   
   // Update the running averages (RMA)
   last_up = alpha * up + (1.0 - alpha) * last_up;
   last_down = alpha * down + (1.0 - alpha) * last_down;
   
   if(last_down == 0) return 100.0;
   if(last_up == 0) return 0.0;
   
   return 100.0 - (100.0 / (1.0 + last_up / last_down));
}

//+------------------------------------------------------------------+
//| Expert tick function                                             |
//+------------------------------------------------------------------+
void OnTick()
{
   double rsi = CalculateManualRSI();
   
   // --- Logic: Check if we have an open position ---
   bool hasPosition = false;
   if(PositionSelectByMagic(InpMagicNum)) hasPosition = true;

   // --- ENTRY LOGIC (If no trade is open) ---
   if(!hasPosition)
   {
      // BUY if RSI drops below 37
      if(rsi < InpRsiUnder)
      {
         if(trade.Buy(InpLotSize, _Symbol, SymbolInfoDouble(_Symbol, SYMBOL_ASK), 0, 0, "RSI Under 37"))
            if(InpNotifyTrade) SendNotification("📈 BUY Triggered | RSI: " + DoubleToString(rsi, 2));
      }
      // SELL if RSI rises above 67
      else if(rsi > InpRsiOver)
      {
         if(trade.Sell(InpLotSize, _Symbol, SymbolInfoDouble(_Symbol, SYMBOL_BID), 0, 0, "RSI Over 67"))
            if(InpNotifyTrade) SendNotification("📉 SELL Triggered | RSI: " + DoubleToString(rsi, 2));
      }
   }
   // --- EXIT LOGIC: Reversion to the Mean (RSI 50) ---
   else
   {
      long type = PositionGetInteger(POSITION_TYPE);
      
      // If we are Long, exit when RSI returns to the 50 "neutral" line
      if(type == POSITION_TYPE_BUY && rsi >= 50.0)
      {
         trade.PositionClose(PositionGetTicket(0));
         SendNotification("✅ Long Closed at Mean (RSI 50)");
      }
      // If we are Short, exit when RSI returns to the 50 "neutral" line
      if(type == POSITION_TYPE_SELL && rsi <= 50.0)
      {
         trade.PositionClose(PositionGetTicket(0));
         SendNotification("✅ Short Closed at Mean (RSI 50)");
      }
   }
}

//+------------------------------------------------------------------+
//| Heartbeat Timer                                                  |
//+------------------------------------------------------------------+
void OnTimer()
{
   datetime now = TimeCurrent();
   if(now - lastHeartbeat >= (InpHeartbeatHrs * 3600))
   {
      double equity = AccountInfoDouble(ACCOUNT_EQUITY);
      double profit = AccountInfoDouble(ACCOUNT_PROFIT);
      SendNotification(StringFormat("💓 Heartbeat: %s\nEquity: $%.2f\nFloat: $%.2f", _Symbol, equity, profit));
      lastHeartbeat = now;
   }
}

// Helper to find our specific trades
bool PositionSelectByMagic(long magic)
{
   for(int i=PositionsTotal()-1; i>=0; i--)
      if(PositionSelectByTicket(PositionGetTicket(i)))
         if(PositionGetInteger(POSITION_MAGIC) == magic) return true;
   return false;
}

// Warm up function to get the RSI averages ready before trading
void WarmUpRSI()
{
   double close[];
   ArraySetAsSeries(close, true);
   int count = CopyClose(_Symbol, _Period, 0, 100, close);
   last_up = 0; last_down = 0;
   double alpha = 1.0 / InpRsiLength;
   
   for(int i=count-2; i>=0; i--) {
      double change = close[i] - close[i+1];
      last_up = alpha * MathMax(change, 0) + (1.0 - alpha) * last_up;
      last_down = alpha * (-MathMin(change, 0)) + (1.0 - alpha) * last_down;
   }
}