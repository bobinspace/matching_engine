#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "common_types.h"
#include "full_order_detail_handlers.h"
#include "market.h"
#include "fill_allocator.h"
#include "trade_event_handlers.h"

void GetWords(const std::string& s, const char delim, std::vector<std::string>& words) {
	std::istringstream line(s);
	std::string word;
	while (std::getline(line, word, delim)) {
		if (!word.empty()) {
			words.push_back(word);
		}
	}
}

bool StringToUnsignedLongLong(char const* const s, unsigned long long& number) {
	if (!s) return false;
	char* end = nullptr;
	number = std::strtoull(s, &end, 10);
	return (0 == errno);
}

bool StringToQuantity(char const* const s, Quantity& quantity) {
	return StringToUnsignedLongLong(s, quantity);
}

bool StringToPrice(char const* const s, Price& price) {
	return StringToUnsignedLongLong(s, price);
}

bool ParseLineToOrderParams(const TimeStamp& timestamp, const std::string& line, Side& side, Instrument& instrument, Order& order) {
	std::vector<std::string> words;
	GetWords(line, ' ', words);
	if (words.size() < 5) {
		return false;
	}
	
	order.key.id = words[0];

	const std::string& side_as_string = words[1];
	if ("SELL" == side_as_string) {
		side = Side::Sell;
	}
	else if ("BUY" == side_as_string) {
		side = Side::Buy;
	}
	else {
		return false;
	}

	instrument = words[2];
		
	if (!StringToQuantity(words[3].c_str(), order.quantity)) {
		return false;
	}

	if (!StringToPrice(words[4].c_str(), order.price)) {
		return false;
	}

	order.key.timestamp = timestamp;

	return true;
}

void RunMarket() {
	
	std::string line;
	GreedyFillAllocator fill_allocator;
	TradeEventConsolePrinter trade_event_console_printer;
	
	Market<PriorityKey::TimeStampComparator, GreedyFillAllocator, TradeEventConsolePrinter> market(fill_allocator, trade_event_console_printer);
	
	TimeStamp t = 0;
	while (std::getline(std::cin, line)) {
		Side side = Side::Buy;
		Instrument instrument;
		Order aggressor_order;

		if (!ParseLineToOrderParams(++t, line, side, instrument, aggressor_order)) {
			continue;
		}

		trade_event_console_printer.instrument = instrument;

		if (Side::Buy == side) {
			market.Buy(instrument, aggressor_order);
		}
		else {
			market.Sell(instrument, aggressor_order);
		}
	}

	printf("\n");
	MarketConsolePrinter market_console_printer;
	market.ForEachOrderByTime(market_console_printer);
}

int main() {
	RunMarket();
	return 0;
}
