#pragma once
#include <vector>
#include <iostream>
#include "document.h"
template <typename Iterator>
class IteratorRange {
private:
	Iterator begin_;
	Iterator end_;
	int size_ = distance(begin_, end_);
public:
	IteratorRange(Iterator begin, Iterator end) {
		begin_ = begin;
		end_ = end;
		size_ = distance(begin, end);
	}
	Iterator begin() {
		return begin_;
	}
	Iterator end() {
		return end_;
	}
	int size() {
		return size_;
	}
};

template <typename Iterator>
class Paginator {
public:
	Paginator(Iterator begin, Iterator end, int page_size) {
		int book_size = distance(begin, end) / page_size;
		IteratorRange<Iterator> tmp(begin, end);
		for (int i = 0; i < book_size; ++i) {
			IteratorRange<Iterator> tmp(begin, begin + page_size);
			pages_.push_back(tmp);
			begin += page_size;
		}
		if (distance(begin, end) % page_size != 0)
		{
			IteratorRange<Iterator> tmp(begin, end);
			pages_.push_back(tmp);
		}
	}

	typename std::vector<IteratorRange<Iterator>>::const_iterator begin() const {
		return pages_.begin();
	}

	typename std::vector<IteratorRange<Iterator>>::const_iterator end() const {
		return pages_.end();
	}

private:
	std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
	return Paginator(begin(c), end(c), page_size);
}



template<typename It>
std::ostream& operator<<(std::ostream& out, IteratorRange<It> page) {
	for (auto it = page.begin(); it < page.end(); ++it) {
		out << *it;
	}
	return out;
}

template<typename Iterator>
bool operator !=(IteratorRange<Iterator> lhr, IteratorRange<Iterator> rhr) {
	return lhr.begin != rhr.begin;
}

template<typename Iterator>
IteratorRange<Iterator>& operator ++(IteratorRange<Iterator>& i) {
	i.begin_ += i.size;
	return i;
}