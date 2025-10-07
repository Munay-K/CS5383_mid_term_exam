#include <catch2/catch_all.hpp>
#include "library.hpp"
#include <vector>

using namespace lib;
using namespace std;

// Gateway de prueba para capturar emails
struct TestEmailGateway : NotificationGateway {
    struct Msg { string to, subject, body; };
    vector<Msg> out;
    void sendEmail(const string& to, const string& subject, const string& body) override {
        out.push_back({to,subject,body});
    }
};

static void seedMinimal(MemoryDb& db) {
    db.books["B1"] = Book{ "B1","Software Engineering",2020, Author{"Ian Sommerville","1951-08-23"},"10th", false };
    db.books["B2"] = Book{ "B2","Clean C++ (New Release)",2025, Author{"Some Author","1980-01-01"},"1st", true };
    db.copies["C1"] = Copy{ "C1","B1", CopyStatus::IN_LIBRARY };
    db.copies["C2"] = Copy{ "C2","B1", CopyStatus::IN_LIBRARY };
    db.readers["R1"] = Reader{ "R1","alice@example.com", {}, {} };
    db.readers["R2"] = Reader{ "R2","bob@example.com",   {}, {} };
}

TEST_CASE("Borrow: límite de 3 préstamos") {
    BioAlert::getInstance().reset();          // <<< evita punteros colgantes entre tests
    MemoryDb db; seedMinimal(db);
    LibraryService libsvc(db);
    auto d = makeDate(2025,10,1);
    db.copies["C3"] = Copy{ "C3","B1", CopyStatus::IN_LIBRARY };
    db.copies["C4"] = Copy{ "C4","B1", CopyStatus::IN_LIBRARY };

    REQUIRE_NOTHROW(libsvc.borrowCopy("C1","R1",d));
    REQUIRE_NOTHROW(libsvc.borrowCopy("C2","R1",d));
    REQUIRE_NOTHROW(libsvc.borrowCopy("C3","R1",d));
    REQUIRE_THROWS(libsvc.borrowCopy("C4","R1",d)); // 4º bloqueado
}

TEST_CASE("Due date: exactamente 30 días desde start") {
    BioAlert::getInstance().reset();
    MemoryDb db; seedMinimal(db);
    LibraryService libsvc(db);
    auto d = makeDate(2025,10,1);
    string L1 = libsvc.borrowCopy("C1","R1",d);
    auto& loan = db.loans.at(L1);
    REQUIRE(loan.due == LibraryService::addDays(d,30));
}

TEST_CASE("Devolver en el día 30: NO hay sanción") {
    BioAlert::getInstance().reset();
    MemoryDb db; seedMinimal(db);
    LibraryService libsvc(db);
    auto start = makeDate(2025,10,1);
    string L1 = libsvc.borrowCopy("C1","R1",start);
    auto due = LibraryService::addDays(start,30);
    REQUIRE_NOTHROW(libsvc.returnCopy("C1", due));
    auto& r1 = db.readers.at("R1");
    REQUIRE_FALSE(r1.activeBanUntil.has_value());
}

TEST_CASE("Retraso de 1 día: ban = 2 días") {
    BioAlert::getInstance().reset();
    MemoryDb db; seedMinimal(db);
    LibraryService libsvc(db);
    auto start = makeDate(2025,10,1);
    libsvc.borrowCopy("C1","R1",start);
    auto when = LibraryService::addDays(start,31);
    libsvc.returnCopy("C1", when);
    auto& r1 = db.readers.at("R1");
    REQUIRE(r1.activeBanUntil.has_value());
    REQUIRE(r1.activeBanUntil.value() == LibraryService::addDays(when,2));
}

TEST_CASE("Retraso de 5 días: ban = 10 días") {
    BioAlert::getInstance().reset();
    MemoryDb db; seedMinimal(db);
    LibraryService libsvc(db);
    auto start = makeDate(2025,10,1);
    libsvc.borrowCopy("C1","R1",start);
    auto when = LibraryService::addDays(start,35);
    libsvc.returnCopy("C1", when);
    auto& r1 = db.readers.at("R1");
    REQUIRE(r1.activeBanUntil.has_value());
    REQUIRE(r1.activeBanUntil.value() == LibraryService::addDays(when,10));
}

TEST_CASE("New release sin copias: préstamo del original exclusivo") {
    BioAlert::getInstance().reset();
    MemoryDb db; seedMinimal(db);
    LibraryService libsvc(db);
    auto d = makeDate(2025,10,1);

    // R2 toma el original de B2
    REQUIRE_NOTHROW(libsvc.borrowOriginalNewRelease("B2","R2", d));

    // R1 intenta mientras está prestado
    REQUIRE_THROWS(libsvc.borrowOriginalNewRelease("B2","R1", d));

    // R2 devuelve; ahora R1 puede
    REQUIRE_NOTHROW(libsvc.returnOriginalNewRelease("B2","R2", makeDate(2025,10,10)));
    REQUIRE_NOTHROW(libsvc.borrowOriginalNewRelease("B2","R1", d));
}

TEST_CASE("Notificación BioAlert al quedar disponible (copia)") {
    BioAlert::getInstance().reset();
    MemoryDb db; seedMinimal(db);
    TestEmailGateway gw;
    BioAlert::getInstance().setGateway(&gw);
    BioAlert::getInstance().subscribe("B1","R2"); // Bob suscrito a B1

    LibraryService libsvc(db);
    auto d = makeDate(2025,10,1);
    libsvc.borrowCopy("C1","R1", d);                // R1 toma C1
    REQUIRE(gw.out.empty());
    libsvc.returnCopy("C1", makeDate(2025,10,5));   // devuelven y se notifica
    REQUIRE_FALSE(gw.out.empty());
    REQUIRE(gw.out[0].to == "bob@example.com");
}

TEST_CASE("Borrow prohibido si lector está baneado") {
    BioAlert::getInstance().reset();
    MemoryDb db; seedMinimal(db);
    LibraryService libsvc(db);
    auto today = makeDate(2025,10,1);
    db.readers["R1"].activeBanUntil = today; // baneo activo hasta hoy
    REQUIRE_THROWS(libsvc.borrowCopy("C1","R1", today));
}

TEST_CASE("COPY_NOT_AVAILABLE cuando la copia no está en IN_LIBRARY") {
    BioAlert::getInstance().reset();
    MemoryDb db; seedMinimal(db);
    LibraryService libsvc(db);
    auto d = makeDate(2025,10,1);
    db.copies["C1"].status = CopyStatus::LOANED; // no disponible
    REQUIRE_THROWS(libsvc.borrowCopy("C1","R1", d));
}

