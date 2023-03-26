#pragma once
#include "document.h"
#include "search_server.h"
#include <vector>
#include <string>
#include <deque>
#include <cmath>

class RequestQueue {
public:
	explicit RequestQueue(const SearchServer& search_server) :server_(search_server) {}
	template <typename DocumentPredicate>
	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
		std::vector<Document> tmp = server_.FindTopDocuments(raw_query, document_predicate);
		if (tmp.empty()) {
			requests_.push_back({ false, true });
		}
		else {
			requests_.push_back({ true, false });
		}
		if (requests_.size() > min_in_day_) {
			requests_.pop_front();
		}
		return tmp;
	}
	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
	std::vector<Document> AddFindRequest(const std::string& raw_query);
	int GetNoResultRequests() const;
private:
	struct QueryResult {
		bool have_result;
		bool no_result;
	};
	const SearchServer& server_;
	std::deque<QueryResult> requests_;
	const static int min_in_day_ = 1440;
};