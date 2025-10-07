#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <stdexcept>
#include <functional>
#include <chrono>
#include <iostream>

namespace lib {

enum class CopyStatus { IN_LIBRARY, LOANED, RESERVED, LATE, REPAIR };

struct Author {
    std::string fullName;
    std::string birthDate; // simplificado
};

struct Book {
    std::string id;
    std::string title;
    int year{};
    Author author;
    std::string edition;
    bool isNewRelease{false}; // “libro nuevo sin copias; se presta el original”
};

struct Loan {
    std::string copyId;     // vacío si es préstamo de “original” (new release sin copias)
    std::string bookId;
    std::string readerId;
    std::chrono::sys_days start;
    std::chrono::sys_days due;
    std::optional<std::chrono::sys_days> returned;

    static std::chrono::sys_days addDays(std::chrono::sys_days base, int d);
    long lateDays() const;
};

struct Copy {
    std::string id;
    std::string bookId;
    CopyStatus status{CopyStatus::IN_LIBRARY};
};

struct Reader {
    std::string id;
    std::string email;
    std::optional<std::chrono::sys_days> activeBanUntil;
    std::vector<std::string> activeLoanIds; // IDs de préstamos activos

    bool canBorrow(std::chrono::sys_days today) const;
};

// --- Notificaciones ---
struct NotificationGateway {
    virtual ~NotificationGateway() = default;
    virtual void sendEmail(const std::string& to, const std::string& subject, const std::string& body) = 0;
};

struct ConsoleEmailGateway : NotificationGateway {
    void sendEmail(const std::string& to, const std::string& subject, const std::string& body) override;
};

// Singleton BioAlert (Observer por libro)
class BioAlert {
    std::map<std::string, std::set<std::string>> subs; // bookId -> set(readerId)
    NotificationGateway* gateway{nullptr};
    BioAlert() = default;
public:
    static BioAlert& getInstance();
    void setGateway(NotificationGateway* g);
    void subscribe(const std::string& bookId, const std::string& readerId);
    void notifyAvailable(const std::string& bookId,
                         std::function<std::string(const std::string&)> getEmailByReaderId,
                         std::function<std::string(const std::string&)> getBookTitleByBookId);
    // Limpia el estado entre tests (evita punteros colgantes)
    void reset();
};

// --- “Repos” en memoria ---
struct MemoryDb {
    std::map<std::string, Book> books;
    std::map<std::string, Copy> copies;
    std::map<std::string, Reader> readers;
    std::map<std::string, Loan> loans;
    std::set<std::string> newReleaseBorrowed; // bookId cuando el “original” está prestado
};

// --- Servicio principal ---
class LibraryService {
    MemoryDb& db;
public:
    explicit LibraryService(MemoryDb& db);

    static std::chrono::sys_days today_utc();
    static std::chrono::sys_days addDays(std::chrono::sys_days base, int d);

    // reglas: 30 días, límite 3, sanción 2x
    std::string borrowCopy(const std::string& copyId, const std::string& readerId,
                           std::chrono::sys_days today = today_utc());

    std::string borrowOriginalNewRelease(const std::string& bookId, const std::string& readerId,
                           std::chrono::sys_days today = today_utc());

    void returnCopy(const std::string& copyId, std::chrono::sys_days when = today_utc());
    void returnOriginalNewRelease(const std::string& bookId, const std::string& readerId,
                                  std::chrono::sys_days when = today_utc());
};

// utils
std::chrono::sys_days makeDate(int y, unsigned m, unsigned d);

} // namespace lib
