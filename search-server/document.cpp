#include "document.h"
Document::Document() = default;
Document::Document(int id, double relevance, int rating) : id(id)
, relevance(relevance)
, rating(rating) {}