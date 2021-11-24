#pragma once

#include <string>
#include <deque>
#include <map>

#ifdef __cpp_concepts
#include <concepts>
#endif

enum class Side {
	Buy,
	Sell,
};

enum class FillExtent {
	None,
	Partial,
	Full,
};

using Price = unsigned long long;
using Quantity = unsigned long long;
using TimeStamp = unsigned long long;
using Id = std::string;
using Instrument = std::string;

// Used as the key to sort resting orders of the same price
struct PriorityKey {
	Id id;
	TimeStamp timestamp;

	bool operator==(const PriorityKey& rhs) const {
		return (id == rhs.id)
			&& (timestamp == rhs.timestamp);
	}

	bool operator!=(const PriorityKey& rhs) const {
		return !((*this) == rhs);
	}

	struct TimeStampComparator {
		bool operator()(const PriorityKey& lhs, const PriorityKey& rhs) const {
			return lhs.timestamp < rhs.timestamp;
		}
	};	
};

// Order within a specific side and instrument.
struct Order {
	Price price;
	Quantity quantity;
	PriorityKey key;
	bool operator==(const Order& rhs) const {
		return (price == rhs.price)
			&& (key == rhs.key)
			&& (quantity == rhs.quantity)
			;
	}
	bool operator!=(const Order& rhs) const {
		return !((*this) == rhs);
	}
	std::string ToString() const {
		char s[1024] = { 0 };
		snprintf(s, sizeof(s), "|%s| %10llu %llu x %llu"
			, key.id.c_str()
			, key.timestamp
			, price
			, quantity);

		return s;
	}
};

// Order in a market (with instrument and side)
struct FullOrderDetail {
	Side side;
	Instrument instrument;
	Order order;
	bool operator==(const FullOrderDetail& rhs) const {
		return (side == rhs.side)
			&& (instrument == rhs.instrument)
			&& (order == rhs.order);
	}
	bool operator!=(const FullOrderDetail& rhs) const {
		return !((*this) == rhs);
	}
};

// Unused for now, but when concepts support is better, we can use these to better document template parameter contracts.
#ifdef __cpp_concepts
template <typename T>
concept IsFullOrderDetailHandler =
requires(T x, const FullOrderDetail& full_order_detail) {
	{ x.HandleFullOrderDetail(full_order_detail) } -> std::same_as<void>;
};

template <typename T>
concept IsTradeEventHandler =
requires(T x, const Side side, const Price matched_price, const Quantity matched_quantity, const Order& aggressor_order, const PriorityKey& opposite_side_key) {
	{ x.HandleTradeEvent(side, matched_price, matched_quantity, aggressor_order, opposite_side_key) } -> std::same_as<void>;
};

template <typename T, typename TradeEventHandler>
concept IsFillAllocator =
requires(T x, const Side side, const Price matched_price, Order& aggressor_order, PrioritySortedOrders& opposite_side_resting_orders, TradeEventHandler& trade_event_handler) {
	{ x.Fill(side, matched_price, aggressor_order, opposite_side_resting_orders, trade_event_handler) } -> std::same_as<void>;
}
&&
IsTradeEventHandler<TradeEventHandler>
;
#endif
