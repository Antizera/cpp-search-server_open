#include "request_queue.h"

using namespace std;

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
	vector<Document> tmp = server_.FindTopDocuments(raw_query, status);
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
vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
	return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
	int result = 0;
	for (int i = 0; i < requests_.size(); i++) {
		if (requests_.at(i).no_result)
			result++;
	}
	return result;
}