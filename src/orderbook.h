#pragma once
#include "common_types.h"

template<typename FillAllocator, typename TradeEventHandler, typename OppositeSideLevels>
FillExtent FindBestPricesThenFill(const Side side, FillAllocator& fill_allocator, TradeEventHandler& trade_event_handler, Order& aggressor_order, OppositeSideLevels& opposite_side_levels) {
	if (opposite_side_levels.empty()) {
		return FillExtent::None;
	}

	// Remember the original order quantity so that at the end of this method, we can check the extent of the fill.
	const auto original_order_quantity = aggressor_order.quantity;
	if (0 == original_order_quantity) {
		return FillExtent::Full;
	}

	// Start with best price
	auto it = opposite_side_levels.begin();

	// Fill as much of the aggressor order as possible, starting from the best price level,
	// until either the aggressor order is completely filled, or there are no more resting orders to match.
	while ((aggressor_order.quantity > 0) 
		&& ((Side::Buy == side) ? (it->first <= aggressor_order.price) : (it->first >= aggressor_order.price))
		&& (opposite_side_levels.end() != it)
		) {
		const Price& matched_price = it->first;
		auto& opposite_side_resting_orders = it->second;

		fill_allocator.Fill(side, matched_price, aggressor_order, opposite_side_resting_orders, trade_event_handler);

		// If all the opposite side's resting orders at this price level have been completed matched, 
		// delete this price level from the opposite side.
		if (opposite_side_resting_orders.empty()) {
			it = opposite_side_levels.erase(it);
		}
		else {
			++it;
		}
	}

	// We now check the order's remaining quantity (i.e. unfilled) to determine the extent of the fill.
	if (0 == aggressor_order.quantity) {
		return FillExtent::Full;
	}

	return
		(original_order_quantity == aggressor_order.quantity)
		? FillExtent::None
		: FillExtent::Partial
		;
}

template<typename MatchingOrdersComparator, typename FillAllocator, typename TradeEventHandler>
class Orderbook {
public:
	using PrioritySortedOrders = std::map<PriorityKey, Quantity, MatchingOrdersComparator>;

	// We want .begin() to be the best bid/ask
	using BuyLevels = std::map<Price, PrioritySortedOrders, std::greater<Price>>;
	using SellLevels = std::map<Price, PrioritySortedOrders, std::less<Price>>;

private:
	BuyLevels buys_;
	SellLevels sells_;

public:
	FillExtent Buy(FillAllocator& fill_allocator, TradeEventHandler& trade_event_handler, Order& aggressor_order) {
		const auto fill_extent = FindBestPricesThenFill(Side::Buy, fill_allocator, trade_event_handler, aggressor_order, sells_);
		if (FillExtent::Full != fill_extent) {
			buys_[aggressor_order.price][aggressor_order.key] = aggressor_order.quantity;
		}
		return fill_extent;

	}

	FillExtent Sell(FillAllocator& fill_allocator, TradeEventHandler& trade_event_handler, Order& aggressor_order) {
		const auto fill_extent = FindBestPricesThenFill(Side::Sell, fill_allocator, trade_event_handler, aggressor_order, buys_);
		if (FillExtent::Full != fill_extent) {
			sells_[aggressor_order.price][aggressor_order.key] = aggressor_order.quantity;
		}
		return fill_extent;
	}

	const auto& Buys() const {
		return buys_;
	}

	const auto& Sells() const {
		return sells_;
	}
};

