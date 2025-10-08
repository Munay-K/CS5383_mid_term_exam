from dataclasses import dataclass, field
from datetime import date, timedelta
from typing import Optional, Dict, Set, List, Callable

# ---------------- Dominio ----------------

class CopyStatus:
    IN_LIBRARY = "IN_LIBRARY"
    LOANED = "LOANED"
    RESERVED = "RESERVED"
    LATE = "LATE"
    REPAIR = "REPAIR"

@dataclass
class Author:
    fullName: str
    birthDate: str

@dataclass
class Book:
    id: str
    title: str
    year: int
    author: Author
    edition: str
    isNewRelease: bool = False

@dataclass
class Loan:
    copyId: str        # "" si es préstamo del “original” (new release sin copias)
    bookId: str
    readerId: str
    start: date
    due: date
    returned: Optional[date] = None

    @staticmethod
    def add_days(d: date, n: int) -> date:
        return d + timedelta(days=n)

    def late_days(self) -> int:
        if self.returned is None:
            return 0
        if self.returned > self.due:
            return (self.returned - self.due).days
        return 0

@dataclass
class Copy:
    id: str
    bookId: str
    status: str = CopyStatus.IN_LIBRARY

@dataclass
class Reader:
    id: str
    email: str
    activeBanUntil: Optional[date] = None
    activeLoanIds: List[str] = field(default_factory=list)

    def can_borrow(self, today: date) -> bool:
        banned = self.activeBanUntil is not None and today <= self.activeBanUntil
        return (not banned) and (len(self.activeLoanIds) < 3)

# ---------------- Notificaciones ----------------

class NotificationGateway:
    def send_email(self, to: str, subject: str, body: str) -> None:
        raise NotImplementedError

class ConsoleEmailGateway(NotificationGateway):
    def send_email(self, to: str, subject: str, body: str) -> None:
        print(f"[EMAIL] To: {to} | {subject} | {body}")

class BioAlert:
    _instance = None

    def __init__(self):
        self.subs: Dict[str, Set[str]] = {}
        self.gateway: Optional[NotificationGateway] = None

    @classmethod
    def instance(cls) -> "BioAlert":
        if cls._instance is None:
            cls._instance = BioAlert()
        return cls._instance

    def set_gateway(self, g: Optional[NotificationGateway]) -> None:
        self.gateway = g

    def subscribe(self, bookId: str, readerId: str) -> None:
        self.subs.setdefault(bookId, set()).add(readerId)

    def notify_available(self,
                         bookId: str,
                         get_email_by_reader: Callable[[str], str],
                         get_book_title: Callable[[str], str]) -> None:
        if not self.gateway:
            return
        ids = self.subs.get(bookId, set())
        title = get_book_title(bookId)
        for rid in ids:
            self.gateway.send_email(get_email_by_reader(rid),
                                    f"Disponible: {title}",
                                    "Ya puedes solicitarlo")

    def reset(self) -> None:
        self.subs.clear()
        self.gateway = None

# ---------------- “Repos” en memoria ----------------

@dataclass
class MemoryDb:
    books: Dict[str, Book] = field(default_factory=dict)
    copies: Dict[str, Copy] = field(default_factory=dict)
    readers: Dict[str, Reader] = field(default_factory=dict)
    loans: Dict[str, Loan] = field(default_factory=dict)
    newReleaseBorrowed: Set[str] = field(default_factory=set)  # bookId con original prestado

# ---------------- Servicio principal ----------------

class LibraryService:
    def __init__(self, db: MemoryDb):
        self.db = db

    @staticmethod
    def today() -> date:
        return date.today()

    @staticmethod
    def add_days(d: date, n: int) -> date:
        return d + timedelta(days=n)

    # reglas: 30 días, límite 3, sanción 2x
    def borrow_copy(self, copyId: str, readerId: str, today: Optional[date] = None) -> str:
        today = today or self.today()
        if copyId not in self.db.copies:
            raise ValueError("COPY_NOT_FOUND")
        if readerId not in self.db.readers:
            raise ValueError("READER_NOT_FOUND")

        c = self.db.copies[copyId]
        r = self.db.readers[readerId]

        if not r.can_borrow(today):
            raise ValueError("BORROW_FORBIDDEN")
        if c.status != CopyStatus.IN_LIBRARY:
            raise ValueError("COPY_NOT_AVAILABLE")

        loan = Loan(copyId=copyId, bookId=c.bookId, readerId=readerId,
                    start=today, due=self.add_days(today, 30))
        loanId = f"L{len(self.db.loans)+1}"
        self.db.loans[loanId] = loan
        c.status = CopyStatus.LOANED
        r.activeLoanIds.append(loanId)
        return loanId

    def borrow_original_new_release(self, bookId: str, readerId: str, today: Optional[date] = None) -> str:
        today = today or self.today()
        if bookId not in self.db.books:
            raise ValueError("BOOK_NOT_FOUND")
        if readerId not in self.db.readers:
            raise ValueError("READER_NOT_FOUND")
        b = self.db.books[bookId]
        r = self.db.readers[readerId]

        if not b.isNewRelease:
            raise ValueError("NOT_NEW_RELEASE")
        if not r.can_borrow(today):
            raise ValueError("BORROW_FORBIDDEN")
        if bookId in self.db.newReleaseBorrowed:
            raise ValueError("ORIGINAL_ALREADY_BORROWED")

        loan = Loan(copyId="", bookId=bookId, readerId=readerId,
                    start=today, due=self.add_days(today, 30))
        loanId = f"L{len(self.db.loans)+1}"
        self.db.loans[loanId] = loan
        self.db.newReleaseBorrowed.add(bookId)
        r.activeLoanIds.append(loanId)
        return loanId

    def return_copy(self, copyId: str, when: Optional[date] = None) -> None:
        when = when or self.today()
        if copyId not in self.db.copies:
            raise ValueError("COPY_NOT_FOUND")
        c = self.db.copies[copyId]
        if c.status not in (CopyStatus.LOANED, CopyStatus.LATE):
            raise ValueError("COPY_NOT_LOANED")

        # busca el préstamo activo de esa copia
        loanId = None
        for k, L in self.db.loans.items():
            if L.copyId == copyId and L.returned is None:
                loanId = k
                break
        if not loanId:
            raise ValueError("LOAN_NOT_FOUND")

        L = self.db.loans[loanId]
        R = self.db.readers[L.readerId]
        L.returned = when
        late = L.late_days()
        if late > 0:
            R.activeBanUntil = when + timedelta(days=2 * late)

        c.status = CopyStatus.IN_LIBRARY
        R.activeLoanIds = [x for x in R.activeLoanIds if x != loanId]

        BioAlert.instance().notify_available(
            L.bookId,
            get_email_by_reader=lambda rid: self.db.readers[rid].email,
            get_book_title=lambda bid: self.db.books[bid].title
        )

    def return_original_new_release(self, bookId: str, readerId: str, when: Optional[date] = None) -> None:
        when = when or self.today()
        if bookId not in self.db.books:
            raise ValueError("BOOK_NOT_FOUND")
        if readerId not in self.db.readers:
            raise ValueError("READER_NOT_FOUND")
        if bookId not in self.db.newReleaseBorrowed:
            raise ValueError("ORIGINAL_NOT_BORROWED")

        # préstamo activo del original (copyId == "")
        loanId = None
        for k, L in self.db.loans.items():
            if L.bookId == bookId and L.readerId == readerId and L.copyId == "" and L.returned is None:
                loanId = k
                break
        if not loanId:
            raise ValueError("LOAN_NOT_FOUND")

        L = self.db.loans[loanId]
        R = self.db.readers[readerId]
        L.returned = when
        late = L.late_days()
        if late > 0:
            R.activeBanUntil = when + timedelta(days=2 * late)

        self.db.newReleaseBorrowed.discard(bookId)
        R.activeLoanIds = [x for x in R.activeLoanIds if x != loanId]

        BioAlert.instance().notify_available(
            bookId,
            get_email_by_reader=lambda rid: self.db.readers[rid].email,
            get_book_title=lambda bid: self.db.books[bid].title
        )

