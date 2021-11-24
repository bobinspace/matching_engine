#pragma once
#include "common_types.h"

// Consume as much quantity as possible from a matching order.
struct GreedyFillAllocator {
	template<typename PrioritySortedOrders, typename TradeEventHandler>
	void Fill(const Side side, const Price matched_price, Order& aggressor_order, PrioritySortedOrders& opposite_side_resting_orders, TradeEventHandler& trade_event_handler) {
		while ((!opposite_side_resting_orders.empty()) && (aggressor_order.quantity > 0)) {
			auto it = opposite_side_resting_orders.begin();
			auto& key = it->first;
			auto& quantity = it->second;

			// Greedy, so fill as much as possible
			const auto matched_quantity = std::min(aggressor_order.quantity, quantity);
			trade_event_handler.HandleTradeEvent(side, matched_price, matched_quantity, aggressor_order, key);

			quantity -= matched_quantity;
			aggressor_order.quantity -= matched_quantity;

			if (0 == quantity) {
				opposite_side_resting_orders.erase(it);
			}
			else {
				return;
			}
		}
	}
};
