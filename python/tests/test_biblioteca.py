import datetime as dt
import pytest
from biblioteca_py.domain import *

def seed(db: MemoryDb):
    db.books["B1"] = Book("B1","Software Engineering",2020, Author("Ian Sommerville","1951-08-23"),"10th", False)
    db.books["B2"] = Book("B2","Clean C++ (New Release)",2025, Author("Some Author","1980-01-01"),"1st", True)
    db.copies["C1"] = Copy("C1","B1")
    db.copies["C2"] = Copy("C2","B1")
    db.readers["R1"] = Reader("R1","alice@example.com")
    db.readers["R2"] = Reader("R2","bob@example.com")

def d(y,m,d): return dt.date(y,m,d)

@pytest.fixture(autouse=True)
def reset_alert():
    BioAlert.instance().reset()
    yield
    BioAlert.instance().reset()

def test_limite_3_prestamos():
    db = MemoryDb(); seed(db)
    db.copies["C3"] = Copy("C3","B1")
    db.copies["C4"] = Copy("C4","B1")
    lib = LibraryService(db)
    start = d(2025,10,1)
    lib.borrow_copy("C1","R1",start)
    lib.borrow_copy("C2","R1",start)
    lib.borrow_copy("C3","R1",start)
    with pytest.raises(ValueError):
        lib.borrow_copy("C4","R1",start)

def test_due_30_dias():
    db = MemoryDb(); seed(db)
    lib = LibraryService(db)
    start = d(2025,10,1)
    loanId = lib.borrow_copy("C1","R1",start)
    assert db.loans[loanId].due == start + dt.timedelta(days=30)

def test_devolver_dia_30_sin_sancion():
    db = MemoryDb(); seed(db)
    lib = LibraryService(db)
    start = d(2025,10,1)
    lib.borrow_copy("C1","R1",start)
    lib.return_copy("C1", start + dt.timedelta(days=30))
    assert db.readers["R1"].activeBanUntil is None

def test_retraso_1dia_ban_2():
    db = MemoryDb(); seed(db)
    lib = LibraryService(db)
    start = d(2025,10,1)
    lib.borrow_copy("C1","R1",start)
    when = start + dt.timedelta(days=31)
    lib.return_copy("C1", when)
    assert db.readers["R1"].activeBanUntil == when + dt.timedelta(days=2)

def test_retraso_5dias_ban_10():
    db = MemoryDb(); seed(db)
    lib = LibraryService(db)
    start = d(2025,10,1)
    lib.borrow_copy("C1","R1",start)
    when = start + dt.timedelta(days=35)
    lib.return_copy("C1", when)
    assert db.readers["R1"].activeBanUntil == when + dt.timedelta(days=10)

def test_new_release_original_exclusivo():
    db = MemoryDb(); seed(db)
    lib = LibraryService(db)
    start = d(2025,10,1)
    lib.borrow_original_new_release("B2","R2", start)
    with pytest.raises(ValueError):
        lib.borrow_original_new_release("B2","R1", start)
    lib.return_original_new_release("B2","R2", d(2025,10,10))
    lib.borrow_original_new_release("B2","R1", start)

def test_borrow_prohibido_si_baneado():
    db = MemoryDb(); seed(db)
    lib = LibraryService(db)
    today = d(2025,10,1)
    db.readers["R1"].activeBanUntil = today
    with pytest.raises(ValueError):
        lib.borrow_copy("C1","R1", today)

