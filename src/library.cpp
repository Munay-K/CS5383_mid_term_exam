#include "library.hpp"

namespace lib {

// ----- Loan -----
std::chrono::sys_days Loan::addDays(std::chrono::sys_days base, int d) {
    return base + std::chrono::days(d);
}

long Loan::lateDays() const {
    if (!returned.has_value()) return 0;
    auto r = returned.value();
    if (r > due) return (r - due).count();
    return 0;
}

// ----- Reader -----
bool Reader::canBorrow(std::chrono::sys_days today) const {
    bool banned = activeBanUntil.has_value() && today <= activeBanUntil.value();
    return !banned && activeLoanIds.size() < 3;
}

// ----- ConsoleEmailGateway -----
void ConsoleEmailGateway::sendEmail(const std::string& to, const std::string& subject, const std::string& body) {
    std::cout << "[EMAIL] To: " << to << " | " << subject << " | " << body << "\n";
}

// ----- BioAlert -----
BioAlert& BioAlert::getInstance() {
    static BioAlert instance;
    return instance;
}
void BioAlert::setGateway(NotificationGateway* g) { gateway = g; }
void BioAlert::subscribe(const std::string& bookId, const std::string& readerId) {
    subs[bookId].insert(readerId);
}
void BioAlert::notifyAvailable(const std::string& bookId,
                         std::function<std::string(const std::string&)> getEmailByReaderId,
                         std::function<std::string(const std::string&)> getBookTitleByBookId) {
    if (!gateway) return;
    auto it = subs.find(bookId);
    if (it == subs.end()) return;
    std::string title = getBookTitleByBookId(bookId);
    for (auto& rid : it->second) {
        gateway->sendEmail(getEmailByReaderId(rid),
                           "Disponible: " + title,
                           "Ya puedes solicitarlo");
    }
}
void BioAlert::reset() {
    subs.clear();
    gateway = nullptr;
}

// ----- LibraryService -----
LibraryService::LibraryService(MemoryDb& db) : db(db) {}

std::chrono::sys_days LibraryService::today_utc() {
    using namespace std::chrono;
    const auto now = floor<days>(system_clock::now());
    return time_point_cast<days>(now);
}
std::chrono::sys_days LibraryService::addDays(std::chrono::sys_days base, int d) {
    return base + std::chrono::days(d);
}

std::string LibraryService::borrowCopy(const std::string& copyId, const std::string& readerId,
                                       std::chrono::sys_days today) {
    if (!db.copies.count(copyId)) throw std::runtime_error("COPY_NOT_FOUND");
    if (!db.readers.count(readerId)) throw std::runtime_error("READER_NOT_FOUND");
    auto& c = db.copies[copyId];
    auto& r = db.readers[readerId];

    if (!r.canBorrow(today)) throw std::runtime_error("BORROW_FORBIDDEN");
    if (c.status != CopyStatus::IN_LIBRARY) throw std::runtime_error("COPY_NOT_AVAILABLE");

    Loan loan;
    loan.copyId   = copyId;
    loan.bookId   = c.bookId;
    loan.readerId = readerId;
    loan.start    = today;
    loan.due      = addDays(today, 30);

    const std::string loanId = "L" + std::to_string(db.loans.size() + 1);
    db.loans[loanId] = loan;

    c.status = CopyStatus::LOANED;
    r.activeLoanIds.push_back(loanId);
    return loanId;
}

std::string LibraryService::borrowOriginalNewRelease(const std::string& bookId, const std::string& readerId,
                                                     std::chrono::sys_days today) {
    if (!db.books.count(bookId))  throw std::runtime_error("BOOK_NOT_FOUND");
    if (!db.readers.count(readerId)) throw std::runtime_error("READER_NOT_FOUND");
    auto& b = db.books[bookId];
    auto& r = db.readers[readerId];

    if (!b.isNewRelease) throw std::runtime_error("NOT_NEW_RELEASE");
    if (!r.canBorrow(today)) throw std::runtime_error("BORROW_FORBIDDEN");
    if (db.newReleaseBorrowed.count(bookId)) throw std::runtime_error("ORIGINAL_ALREADY_BORROWED");

    Loan loan;
    loan.copyId   = ""; // sin copia física
    loan.bookId   = bookId;
    loan.readerId = readerId;
    loan.start    = today;
    loan.due      = addDays(today, 30);

    const std::string loanId = "L" + std::to_string(db.loans.size() + 1);
    db.loans[loanId] = loan;

    db.newReleaseBorrowed.insert(bookId);
    r.activeLoanIds.push_back(loanId);
    return loanId;
}

void LibraryService::returnCopy(const std::string& copyId, std::chrono::sys_days when) {
    if (!db.copies.count(copyId)) throw std::runtime_error("COPY_NOT_FOUND");
    auto& c = db.copies[copyId];
    if (c.status != CopyStatus::LOANED && c.status != CopyStatus::LATE) {
        throw std::runtime_error("COPY_NOT_LOANED");
    }

    std::string loanId;
    for (auto& kv : db.loans) {
        auto& L = kv.second;
        if (L.copyId == copyId && !L.returned.has_value()) { loanId = kv.first; break; }
    }
    if (loanId.empty()) throw std::runtime_error("LOAN_NOT_FOUND");

    auto& L = db.loans[loanId];
    auto& R = db.readers[L.readerId];

    L.returned = when;
    const long late = L.lateDays();
    if (late > 0) {
        R.activeBanUntil = when + std::chrono::days(late * 2);
    }

    c.status = CopyStatus::IN_LIBRARY;

    {
        std::vector<std::string> tmp;
        for (auto& id : R.activeLoanIds) if (id != loanId) tmp.push_back(id);
        R.activeLoanIds.swap(tmp);
    }

    BioAlert::getInstance().notifyAvailable(
        L.bookId,
        [&](const std::string& rid){ return db.readers.at(rid).email; },
        [&](const std::string& bid){ return db.books.at(bid).title; }
    );
}

void LibraryService::returnOriginalNewRelease(const std::string& bookId, const std::string& readerId,
                                              std::chrono::sys_days when) {
    if (!db.books.count(bookId)) throw std::runtime_error("BOOK_NOT_FOUND");
    if (!db.readers.count(readerId)) throw std::runtime_error("READER_NOT_FOUND");
    if (!db.newReleaseBorrowed.count(bookId)) throw std::runtime_error("ORIGINAL_NOT_BORROWED");

    std::string loanId;
    for (auto& kv : db.loans) {
        auto& L = kv.second;
        if (L.bookId == bookId && L.readerId == readerId && !L.returned.has_value() && L.copyId.empty()) {
            loanId = kv.first; break;
        }
    }
    if (loanId.empty()) throw std::runtime_error("LOAN_NOT_FOUND");

    auto& L = db.loans[loanId];
    auto& R = db.readers[readerId];
    L.returned = when;
    const long late = L.lateDays();
    if (late > 0) R.activeBanUntil = when + std::chrono::days(late * 2);

    db.newReleaseBorrowed.erase(bookId);

    {
        std::vector<std::string> tmp;
        for (auto& id : R.activeLoanIds) if (id != loanId) tmp.push_back(id);
        R.activeLoanIds.swap(tmp);
    }

    BioAlert::getInstance().notifyAvailable(
        bookId,
        [&](const std::string& rid){ return db.readers.at(rid).email; },
        [&](const std::string& bid){ return db.books.at(bid).title; }
    );
}

// ----- utils -----
std::chrono::sys_days makeDate(int y, unsigned m, unsigned d) {
    using namespace std::chrono;
    return sys_days{year{y}/month{m}/day{d}};
}

} // namespace lib
