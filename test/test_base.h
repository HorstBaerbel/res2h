#pragma once

#include <iostream>
#include <string>

// ----- macros to run tests --------------------------------------------

// Test suite example:
// 
// bool test_value() { -> test function that returns true on success, false on failure
//     CHECK(value == 123) -> checks if condition true, else prints condition and exits function returning false
//     CHECK_EQUAL(value, 123) -> checks if value == 123, else prints condition and exits function returning false
//     CHECK_FLOAT_EQUAL(value, 123.5, 1e-5) -> checks if abs(value - 123) < 1e-5, else prints condition and exits function returning false
//     CHECK_THROW(functionThatThrowsException, std::runtime_error)
//     TEST_SUCCEEDED -> returns true
// }
// 
// START_SUITE("Important values") -> Start the suite and define result variable
// RUN_TEST("Value 1", test_value) -> Run the test and check return value
// END_SUITE -> Print result and pass success/failure value to caller
//

#define START_SUITE( name )\
auto main(int /*argc*/, char **/*argv*/) -> int\
{\
    std::cout << "Test suite \"" << (name) << "\"..." << std::endl;\
    static bool suiteSucceded = true;

#define RUN_TEST( name, func )\
{\
    std::cout << "Test \"" << (name) << "\"... ";\
    try {\
        if (func()) { std::cout << "succeed." << std::endl; }\
        else { std::cout << " Failed." << std::endl; suiteSucceded = false; }\
    }\
    catch(...) { std::cout << " Failed." << std::endl; suiteSucceded = false; }\
}

#define END_SUITE \
    std::cout << "Suite " << (suiteSucceded ? "succeeded" : "failed") << std::endl;\
    return suiteSucceded ? 0 : 1;\
}

// ----- macros to check values --------------------------------------------
// Warning: There might be UB dragons and black preprocessor magic

// Convert token to string: https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html
#define STRINGIZEX(a) #a
#define STRINGIZE(a) STRINGIZEX(a)

// Snatched from here: http://stackoverflow.com/questions/8487986/file-macro-shows-full-path
//#define FILEBASENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define LINEINFO " (Line " STRINGIZE(__LINE__) ")"

#define TEST_SUCCEEDED return true;

// Check if statement a can be converted to true, else print a and return false
#define CHECK(a) \
    try { if (!(a)) { std::cerr << "Check failed: " << STRINGIZE(a) LINEINFO; return false; }} \
    catch(...) { std::cerr << "Check failed: " << STRINGIZE(a) LINEINFO; return false; }
// Check if a == b, else print "a == b" and return false. Use if CHECK can not be used
#define CHECK_EQUAL(a, b)\
    try { if ((a) != (b)) { std::cerr << std::string("Check failed: ") + (STRINGIZE(a) "==" STRINGIZE(b) LINEINFO); return false; }}\
    catch(...) { std::cerr << std::string("Check failed: ") + STRINGIZE(a) "==" STRINGIZE(b) LINEINFO; return false; }
// Compare two floating point values for equality using an epsilon, else print "a == b" and return false
#define CHECK_FLOAT_EQUAL(a, b, epsilon) if (std::abs(a - b) > epsilon) { std::cerr << "Check failed: " << STRINGIZE(a) "==" STRINGIZE(b) LINEINFO; return false; }
// Check if a statement a throws exception e and return true only if it does
#define CHECK_THROW(a, ex)\
    try { (a); { std::cerr << "Check failed. Exception expected: " << STRINGIZE(a) LINEINFO; return false; }}\
    catch(const ex &e) { return true; }
