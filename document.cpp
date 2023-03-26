#include"document.h"

std::ostream& operator<<(std::ostream& out, Document doc) {
    using namespace std::string_literals;
    return out << "{ document_id = "s << doc.id << ", relevance = "s << doc.relevance << ", rating = "s << doc.rating << " }"s;
}