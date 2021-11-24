#include "full_order_detail_handlers.h"
#include <stdio.h>

void MarketConsolePrinter::HandleFullOrderDetail(const FullOrderDetail& full_order_detail) {
	printf("%s %s %s %llu %llu\n"
		, full_order_detail.order.key.id.c_str()
		, (Side::Buy == full_order_detail.side) ? "BUY" : "SELL"
		, full_order_detail.instrument.c_str()
		, full_order_detail.order.quantity
		, full_order_detail.order.price
	);
}
