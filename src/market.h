#pragma once
#include "orderbook.h"

// All instruments' orderbooks
template<typename MatchingOrdersComparator, typename FillAllocator, typename TradeEventHandler>
class Market {
	FillAllocator& fill_allocator_;
	TradeEventHandler& trade_event_handler_;
	std::map<Instrument, Orderbook<MatchingOrdersComparator, FillAllocator, TradeEventHandler>> orderbooks_;

public:
	Market(FillAllocator& fill_allocator, TradeEventHandler& trade_event_handler)
		: fill_allocator_(fill_allocator)
		, trade_event_handler_(trade_event_handler)
	{}

	FillExtent Buy(const Instrument& instrument, Order& aggressor_order) {
		return orderbooks_[instrument].Buy(fill_allocator_, trade_event_handler_, aggressor_order);
	}

	FillExtent Sell(const Instrument& instrument, Order& aggressor_order) {
		return orderbooks_[instrument].Sell(fill_allocator_, trade_event_handler_, aggressor_order);
	}

	const auto& Buys(const Instrument& instrument) const {
		return orderbooks_[instrument].Buys();
	}

	const auto& Sells(const Instrument& instrument) const {
		return orderbooks_[instrument].Sells();
	}

	template<typename FullOrderDetailHandler>
	void ForEachOrderByTime(FullOrderDetailHandler& full_order_details_handler) const {
		std::map<TimeStamp, FullOrderDetail> sells_timestamp_to_full_order_details;
		std::map<TimeStamp, FullOrderDetail> buys_timestamp_to_full_order_details;
		for (const auto& [instrument, orderbook] : orderbooks_) {
			for (const auto& [price, keys] : orderbook.Sells()) {
				for (const auto& [key, quantity] : keys) {
					sells_timestamp_to_full_order_details[key.timestamp] = { Side::Sell, instrument, { price, quantity, key } };
				}
			}
			for (const auto& [price, keys] : orderbook.Buys()) {
				for (const auto& [key, quantity] : keys) {
					buys_timestamp_to_full_order_details[key.timestamp] = { Side::Buy, instrument, { price, quantity, key } };
				}
			}
		}

		for (const auto& [timestamp, full_order_detail] : sells_timestamp_to_full_order_details) {
			full_order_details_handler.HandleFullOrderDetail(full_order_detail);
		}
		for (const auto& [timestamp, full_order_detail] : buys_timestamp_to_full_order_details) {
			full_order_details_handler.HandleFullOrderDetail(full_order_detail);
		}
	}
};
