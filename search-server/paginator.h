#pragma once
#include <iostream>
#include <vector>
#include "document.h"

template <typename iterator>
class IteratorRange {
public:
    iterator begin;
    iterator end;
    //int size = 0;
};

std::ostream& operator<<(std::ostream& os, Document value) {
    os << "{ document_id = " << value.id << ", relevance = " << value.relevance << ", rating = " << value.rating << " }";
    return os;
}
template <typename iterator>
std::ostream& operator<<(std::ostream& os, IteratorRange<iterator> value) {
    for (; value.begin < value.end; ++value.begin) {
        os << *value.begin;
    }
    return os;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator start, Iterator end, int page_size) :
        start_(start), end_(end), page_size_(page_size) {
        IteratorRange<Iterator> buffer;
        buffer.begin = start;
        for (int i = 1; start < end; ++i, ++start) {
            if (i % page_size == 0) {
                buffer.end = start + 1;
                //buffer.size = distance(buffer.begin, buffer.end);
                data.push_back(buffer);
                buffer = IteratorRange<Iterator>();
                buffer.begin = start + 1;
            }
        }
        buffer.end = end;
        if (buffer.begin != buffer.end) data.push_back(buffer);
    }

    auto begin() const {
        return data.begin();
    }

    auto end() const {
        return data.end();
    }
private:
    std::vector<IteratorRange<Iterator>> data;
    Iterator start_;
    Iterator end_;
    int page_size_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}