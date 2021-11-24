#include <stdio.h>
#include "trade_event_handlers.h"

void TradeEventConsolePrinter::HandleTradeEvent(const Side, const Price matched_price, const Quantity matched_quantity, const Order& aggressor_order, const PriorityKey& opposite_side_key) {
	printf("TRADE %s %s %s %llu %llu\n"
		, instrument.c_str()
		, aggressor_order.key.id.c_str()
		, opposite_side_key.id.c_str()
		, matched_quantity
		, matched_price
		);
}
