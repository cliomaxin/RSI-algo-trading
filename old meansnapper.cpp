//+------------------------------------------------------------------+
//|                                     MeanSnapper_Heartbeat_v2.mq5 |
//|                                  Copyright 2026, AI Collaborator |
//+------------------------------------------------------------------+
#property strict
#include <Trade\Trade.mqh>

// --- Strategy Inputs ---
input int      InpBBLength    = 20;          
input double   InpBBMult      = 2.0;         
input int      InpRSILevel    = 14;          
input int      InpRSISell     = 30;          
input int      InpRSIOver     = 70;          
input double   InpLotSize     = 0.1;         
input int      InpMagicNum    = 123456;      

// --- Heartbeat Inputs ---
input int      InpHeartbeatHours = 4;        // Send status every X hours
input bool     InpSendOnStart    = true;     // Send notification on launch

// --- Global Variables ---
int      handleBB, handleRSI;
CTrade   trade;
datetime lastHeartbeat = 0;

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit()
{
   handleBB  = iBands(_Symbol, _Period, InpBBLength, 0, InpBBMult, PRICE_CLOSE);
   handleRSI = iRSI(_Symbol, _Period, InpRSILevel, PRICE_CLOSE);
   
   trade.SetExpertMagicNumber(InpMagicNum);
   
   // Start the system timer (checks every 1 minute)
   EventSetTimer(60); 

   if(InpSendOnStart) {
      SendStatusUpdate("System Initialized");
   }

   return(INIT_SUCCEEDED);
}

//+------------------------------------------------------------------+
//| Expert deinitialization function                                 |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
   EventKillTimer(); // Always kill the timer when stopping
}

//+------------------------------------------------------------------+
//| Timer function - Handles the Heartbeat                           |
//+------------------------------------------------------------------+
void OnTimer()
{
   datetime now = TimeCurrent();
   
   // Check if it's time for a Heartbeat (Hours to Seconds conversion)
   if(now - lastHeartbeat >= (InpHeartbeatHours * 3600))
   {
      SendStatusUpdate("Periodic Heartbeat");
      lastHeartbeat = now;
   }
}

//+------------------------------------------------------------------+
//| Helper: Format and Send the Status Notification                  |
//+------------------------------------------------------------------+
void SendStatusUpdate(string reason)
{
   double balance = AccountInfoDouble(ACCOUNT_BALANCE);
   double equity  = AccountInfoDouble(ACCOUNT_EQUITY);
   double profit  = AccountInfoDouble(ACCOUNT_PROFIT);
   
   string msg = StringFormat("💓 %s\nPair: %s\nBalance: $%.2f\nEquity: $%.2f\nFloating P/L: $%.2f", 
                             reason, _Symbol, balance, equity, profit);
   
   SendNotification(msg);
   Print("Heartbeat Sent: ", msg);
}

//+------------------------------------------------------------------+
//| Expert tick function (The Trading Logic)                         |
//+------------------------------------------------------------------+
void OnTick()
{
   // [Check for Mean Reversion Logic - Same as previous script]
   if(PositionSelectByMagic(InpMagicNum)) return;

   double upper[], lower[], rsi[];
   ArraySetAsSeries(upper, true);
   ArraySetAsSeries(lower, true);
   ArraySetAsSeries(rsi, true);

   if(CopyBuffer(handleBB, 1, 0, 2, upper) < 0) return;
   if(CopyBuffer(handleBB, 2, 0, 2, lower) < 0) return;
   if(CopyBuffer(handleRSI, 0, 0, 2, rsi) < 0) return;

   double closePrice = iClose(_Symbol, _Period, 0);

   if(closePrice < lower[0] && rsi[0] < InpRSISell)
   {
      if(trade.Buy(InpLotSize, _Symbol, SymbolInfoDouble(_Symbol, SYMBOL_ASK), 0, 0, "Snapper Long"))
         SendNotification("🚀 BUY Order Placed: " + _Symbol);
   }

   if(closePrice > upper[0] && rsi[0] > InpRSIOver)
   {
      if(trade.Sell(InpLotSize, _Symbol, SymbolInfoDouble(_Symbol, SYMBOL_BID), 0, 0, "Snapper Short"))
         SendNotification("📉 SELL Order Placed: " + _Symbol);
   }
}

//+------------------------------------------------------------------+
//| Check for Mean Reversion (Exit Logic)                            |
//+------------------------------------------------------------------+
bool PositionSelectByMagic(long magic)
{
   for(int i=PositionsTotal()-1; i>=0; i--)
   {
      ulong ticket = PositionGetTicket(i);
      if(PositionSelectByTicket(ticket))
      {
         if(PositionGetInteger(POSITION_MAGIC) == magic)
         {
            double middle[];
            ArraySetAsSeries(middle, true);
            CopyBuffer(handleBB, 0, 0, 1, middle);
            
            double currentPrice = PositionGetDouble(POSITION_PRICE_CURRENT);
            long type = PositionGetInteger(POSITION_TYPE);
            
            if((type == POSITION_TYPE_BUY && currentPrice >= middle[0]) ||
               (type == POSITION_TYPE_SELL && currentPrice <= middle[0]))
            {
               double p = PositionGetDouble(POSITION_PROFIT);
               trade.PositionClose(ticket);
               SendNotification(StringFormat("✅ Trade Closed! Profit: $%.2f", p));
               return false;
            }
            return true;
         }
      }
   }
   return false;
}
