#include "src/global/CopyOnWrite.h" // Adjust path if necessary from execution root
#include "src/global/macros.h" // For PRETTY_FUNCTION, NODISCARD (though Cow should include it)
#include "src/global/logging.h" // For MMLOG_DEBUG

#include <string>
#include <iostream> // For std::cout in this test program
#include <cassert>  // For assert()
#include <memory>   // For std::make_shared

// Dummy type for testing
struct TestType {
    std::string data;
    int id;

    TestType(std::string d = "", int i = 0) : data(std::move(d)), id(i) {
        // MMLOG_DEBUG() << "TestType Constructor: " << data << " id: " << id << " this: " << this;
    }
    TestType(const TestType& other) : data(other.data), id(other.id) {
        // MMLOG_DEBUG() << "TestType Copy Constructor from " << &other << " to " << this;
    }
    TestType& operator=(const TestType& other) {
        // MMLOG_DEBUG() << "TestType Copy Assignment from " << &other << " to " << this;
        if (this != &other) {
            data = other.data;
            id = other.id;
        }
        return *this;
    }
    // ~TestType() {
    //     MMLOG_DEBUG() << "TestType Destructor: " << data << " id: " << id << " this: " << this;
    // }

    bool operator==(const TestType& other) const {
        return data == other.data && id == other.id;
    }

    friend std::ostream& operator<<(std::ostream& os, const TestType& tt) {
        os << "TestType(\"" << tt.data << "\", " << tt.id << ")";
        return os;
    }
};


// Basic test like the prototype
void run_basic_string_test() {
    std::cout << "\n--- Running Basic String Test ---" << std::endl;
    CopyOnWrite<std::string> c1; // Default constructor
    MMLOG_DEBUG() << "c1 created (default)";
    assert(c1.isMutable() && "Default constructed Cow should be mutable");

    c1.getMutable() = "Hello, World!";
    MMLOG_DEBUG() << "c1.getMutable() = \"Hello, World!\"";
    assert(c1.isMutable() && "c1 should be mutable after getMutable()");

    std::cout << "c1 read-only: " << c1.getReadOnly() << std::endl;
    MMLOG_DEBUG() << "c1.getReadOnly() called";
    assert(c1.isReadOnly() && "c1 should be read-only after getReadOnly()");

    std::cout << "c1 read-only again: " << c1.getReadOnly() << std::endl;
    MMLOG_DEBUG() << "c1.getReadOnly() called again";
    assert(c1.isReadOnly() && "c1 should still be read-only");

    (void)c1.getMutable(); // Make it mutable again
    MMLOG_DEBUG() << "c1.getMutable() called";
    assert(c1.isMutable() && "c1 should be mutable after getMutable()");
    c1.getMutable() = "Hello, Cow!";
    MMLOG_DEBUG() << "c1.getMutable() = \"Hello, Cow!\"";

    CopyOnWrite<std::string> c2 = c1; // Copy constructor
    MMLOG_DEBUG() << "c2 created by copy from c1";
    assert(c1.isReadOnly() && "c1 should be read-only after being copied from");
    assert(c2.isReadOnly() && "c2 should be read-only after copy construction");
    assert(c1.getReadOnly() == "Hello, Cow!");
    assert(c2.getReadOnly() == "Hello, Cow!");
    assert(c1.getReadOnly() == c2.getReadOnly() && "c1 and c2 should have equal read-only content");

    std::cout << "c1 (after copy): " << c1.getReadOnly() << std::endl;
    std::cout << "c2 (after copy): " << c2.getReadOnly() << std::endl;

    c1.getMutable() = "Goodbye, World!";
    MMLOG_DEBUG() << "c1.getMutable() = \"Goodbye, World!\"";
    assert(c1.isMutable() && "c1 should be mutable after modification");
    assert(c1.getReadOnly() == "Goodbye, World!"); // getReadOnly makes it RO again
    assert(c2.getReadOnly() == "Hello, Cow!" && "c2 should not be affected by c1's mutation");

    std::cout << "c1 (after c1 mod): " << c1.getReadOnly() << std::endl;
    std::cout << "c2 (after c1 mod): " << c2.getReadOnly() << std::endl;

    CopyOnWrite<std::string> c3("Initial Data"); // Const T& constructor
    MMLOG_DEBUG() << "c3 created from const char* (via const std::string&)";
    assert(c3.isMutable() && "c3 from T& should be initially mutable");
    assert(c3.getReadOnly() == "Initial Data");
    assert(c3.isReadOnly() && "c3 should be RO after getReadOnly");

    std::cout << "c3: " << c3.getReadOnly() << std::endl;
    std::cout << "--- Basic String Test Passed ---" << std::endl;
}

void run_custom_type_test() {
    std::cout << "\n--- Running Custom Type Test ---" << std::endl;
    CopyOnWrite<TestType> ct1; // Default constructor (TestType())
    MMLOG_DEBUG() << "ct1 created (default)";
    assert(ct1.isMutable());
    ct1.getMutable().data = "Custom Hello";
    ct1.getMutable().id = 10;
    MMLOG_DEBUG() << "ct1 modified: " << ct1.getReadOnly().data << ", id " << ct1.getReadOnly().id;
    assert(ct1.isReadOnly());

    CopyOnWrite<TestType> ct2 = ct1; // Copy constructor
    MMLOG_DEBUG() << "ct2 created by copy from ct1";
    assert(ct1.isReadOnly());
    assert(ct2.isReadOnly());
    assert(ct1.getReadOnly().data == "Custom Hello" && ct1.getReadOnly().id == 10);
    assert(ct2.getReadOnly().data == "Custom Hello" && ct2.getReadOnly().id == 10);
    assert(ct1.getReadOnly() == ct2.getReadOnly());


    ct1.getMutable().data = "Custom Goodbye";
    ct1.getMutable().id = 20;
    MMLOG_DEBUG() << "ct1 modified again: " << ct1.getReadOnly().data << ", id " << ct1.getReadOnly().id;

    assert(ct1.getReadOnly().data == "Custom Goodbye" && ct1.getReadOnly().id == 20);
    assert(ct2.getReadOnly().data == "Custom Hello" && ct2.getReadOnly().id == 10 && "ct2 data changed!");
    assert(!(ct1.getReadOnly() == ct2.getReadOnly()));

    TestType initial_tt("Initial TestType", 123);
    CopyOnWrite<TestType> ct3(initial_tt); // Const T& constructor
    MMLOG_DEBUG() << "ct3 created from TestType&: " << ct3.getReadOnly().data;
    assert(ct3.isMutable()); // Initially mutable from T&
    assert(ct3.getReadOnly() == initial_tt);
    assert(ct3.isReadOnly());

    // Test operator==
    CopyOnWrite<TestType> ct4(initial_tt); // ct4 has same content as ct3 initially
    MMLOG_DEBUG() << "ct4 created from TestType&: " << ct4.getReadOnly().data;
    assert(ct3.getReadOnly() == ct4.getReadOnly()); // Should be equal values
    assert(ct3 == ct4); // Test Cow::operator==

    ct4.getMutable().id = 456; // Modify ct4
    MMLOG_DEBUG() << "ct4 modified. ct3: " << ct3.getReadOnly() << " ct4: " << ct4.getReadOnly();
    assert(!(ct3 == ct4)); // Should no longer be equal

    std::cout << "--- Custom Type Test Passed ---" << std::endl;
}

void run_shared_ptr_constructor_test() {
    std::cout << "\n--- Running Shared Ptr Constructor Test ---" << std::endl;
    std::shared_ptr<const std::string> shared_ro_str = std::make_shared<const std::string>("Shared RO String");
    CopyOnWrite<std::string> c_shared_ro(shared_ro_str);
    MMLOG_DEBUG() << "c_shared_ro created from shared_ptr<const std::string>";
    assert(c_shared_ro.isReadOnly()); // Should be read-only
    assert(c_shared_ro.getReadOnly() == "Shared RO String");

    // Test copying this Cow
    CopyOnWrite<std::string> c_shared_ro_copy = c_shared_ro;
    MMLOG_DEBUG() << "c_shared_ro_copy created from c_shared_ro";
    assert(c_shared_ro.isReadOnly());
    assert(c_shared_ro_copy.isReadOnly());
    assert(c_shared_ro_copy.getReadOnly() == "Shared RO String");

    // Mutating the copy should not affect the original if the original shared_ptr was const
    c_shared_ro_copy.getMutable() = "Modified Copy of Shared RO";
    MMLOG_DEBUG() << "c_shared_ro_copy mutated";
    assert(c_shared_ro_copy.isMutable());
    assert(c_shared_ro.getReadOnly() == "Shared RO String" && "Original shared RO data changed!");
    std::cout << "Original shared_ro_str: " << *shared_ro_str << std::endl;
    std::cout << "c_shared_ro: " << c_shared_ro.getReadOnly() << std::endl;
    std::cout << "c_shared_ro_copy: " << c_shared_ro_copy.getReadOnly() << std::endl;


    // Test with a mutable shared_ptr (though Cow constructor takes shared_ptr<const T>)
    // The constructor Cow(ReadOnly data) is `explicit Cow(std::shared_ptr<const T> data)`
    // We don't have a direct `Cow(std::shared_ptr<T> data)` constructor.
    // Instead, one would do:
    // std::shared_ptr<std::string> shared_rw_str = std::make_shared<std::string>("Shared RW String");
    // CopyOnWrite<std::string> c_shared_rw(std::const_pointer_cast<const std::string>(shared_rw_str)); // Explicit cast
    // or rely on CopyOnWrite(const T&) if dereferencing: CopyOnWrite<std::string> c_shared_rw(*shared_rw_str);
    // For now, the shared_ptr<const T> constructor is the main one to test for this category.

    std::cout << "--- Shared Ptr Constructor Test Passed ---" << std::endl;
}


int main() {
    MMLOG_DEBUG() << "Starting Cow manual tests...";

    run_basic_string_test();
    run_custom_type_test();
    run_shared_ptr_constructor_test();

    // Test for default constructor of T requirement
    struct NoDefaultCtor {
        NODISCARD explicit NoDefaultCtor(int) {}
        // Need operator== for some tests if we were to use it more deeply
        bool operator==(const NoDefaultCtor&) const { return true; }
    };
    // CopyOnWrite<NoDefaultCtor> c_no_default; // This should fail to compile if T{} is used.
                                             // The current Cow() : Cow{T{}} requires default constructible T.
                                             // The old one Cow() : m_data(std::make_shared<T>()) also value-initializes.
    CopyOnWrite<NoDefaultCtor> c_no_default_arg{NoDefaultCtor{42}}; // This should be fine.
    MMLOG_DEBUG() << "Successfully created Cow<NoDefaultCtor> with argument.";

    std::cout << "\nAll manual tests completed." << std::endl;
    return 0;
}
