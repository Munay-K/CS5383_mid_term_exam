#include "library.hpp"
#include <iostream>

using namespace lib;

static void printYmd(const std::chrono::sys_days& day) {
    using namespace std::chrono;
    const year_month_day ymd(day);
    std::cout << int(ymd.year()) << "-"
              << unsigned(ymd.month()) << "-"
              << unsigned(ymd.day());
}

int main() {
    ConsoleEmailGateway email;
    BioAlert::getInstance().setGateway(&email);

    MemoryDb db;
    // Datos base
    db.books["B1"] = Book{ "B1","Software Engineering",2020, Author{"Ian Sommerville","1951-08-23"},"10th", false };
    db.books["B2"] = Book{ "B2","Clean C++ (New Release)",2025, Author{"Some Author","1980-01-01"},"1st", true };
    db.copies["C1"] = Copy{ "C1","B1", CopyStatus::IN_LIBRARY };
    db.copies["C2"] = Copy{ "C2","B1", CopyStatus::IN_LIBRARY };
    db.readers["R1"] = Reader{ "R1","alice@example.com", {}, {} };
    db.readers["R2"] = Reader{ "R2","bob@example.com",   {}, {} };

    // Suscripciones a BioAlert
    BioAlert::getInstance().subscribe("B1", "R2"); // Bob quiere B1
    BioAlert::getInstance().subscribe("B2", "R1"); // Alice quiere B2

    LibraryService libsvc(db);

    // 1) Préstamo “feliz” de copia normal
    auto d1 = makeDate(2025,10,1);
    std::string L1 = libsvc.borrowCopy("C1","R1", d1);
    std::cout << "Loan L1 creado. Due = d1+30\n";

    // 2) Límite de 3 préstamos
    db.copies["C3"] = Copy{ "C3","B1", CopyStatus::IN_LIBRARY };
    db.copies["C4"] = Copy{ "C4","B1", CopyStatus::IN_LIBRARY };
    std::string L2 = libsvc.borrowCopy("C2","R1", d1);
    std::string L3 = libsvc.borrowCopy("C3","R1", d1);
    try { libsvc.borrowCopy("C4","R1", d1); std::cout << "[ERROR] 4to préstamo permitido\n"; }
    catch(const std::exception& e){ std::cout << "[OK] Tope 3 préstamos: " << e.what() << "\n"; }

    // 3) Devolución con atraso: ban = 2 * lateDays
    auto dReturnLate = makeDate(2025,11,5); // ~35 días después
    libsvc.returnCopy("C1", dReturnLate);
    auto& r1 = db.readers["R1"];
    if (r1.activeBanUntil.has_value()) {
        std::cout << "Ban hasta (YYYY-MM-DD): ";
        printYmd(r1.activeBanUntil.value());
        std::cout << "\n";
    } else {
        std::cout << "[ERROR] Debió quedar baneado\n";
    }

    // 4) “Nuevo sin copias” → prestar original
    try {
        libsvc.borrowOriginalNewRelease("B2","R2", d1); // Bob presta original
        try { libsvc.borrowOriginalNewRelease("B2","R1", d1); std::cout << "[ERROR] 2do original permitido\n"; }
        catch(const std::exception& e){ std::cout << "[OK] Original único: " << e.what() << "\n"; }
        libsvc.returnOriginalNewRelease("B2","R2", makeDate(2025,10,10));
    } catch (const std::exception& e) {
        std::cout << "[ERROR] borrowOriginalNewRelease: " << e.what() << "\n";
    }

    // 5) Notificación al devolver copia de B1:
    libsvc.returnCopy("C2", makeDate(2025,10,5)); // debe imprimir [EMAIL] a bob para B1

    std::cout << "OK\n";
    return 0;
}


