#pragma once
#include "common_types.h"

struct TradeEventConsolePrinter {
	Instrument instrument;
	void HandleTradeEvent(const Side side, const Price matched_price, const Quantity matched_quantity, const Order& aggressor_order, const PriorityKey& opposite_side_key);
};
