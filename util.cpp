
#include <iostream>   // for std::cout
#include <chrono>

#include "main.hpp"

// -------------------------------------------------------------
// Stupid function / procedure

void on_age(int age)
{ 
  std::cout << "On age: " << age << '\n';
}

// -------------------------------------------------------------

// check if file exists AND it can be opened for reading
 
bool DoesFileExist (const std::string& name) {

  bool isOK = false;

  try {
    if (FILE *file = fopen(name.c_str(), "r")) {
      fclose(file);
      isOK = true;
      }
    }
  catch( const std::exception& e ) {
    std::cout << "\n ERRPR: Unable to open the file: " << name << '\n' ;
    std::cerr << e.what() << '\n' ;
    isOK = false;
    }
  return isOK;
}


// -------------------------------------------------------------

// print C++ version according to compiler...  just to check features

void printVersions() {

    std::cout << "C++ Version:  " ;

    if (__cplusplus == 201703L) std::cout << "C++17\n";
    else if (__cplusplus == 201402L) std::cout << "C++14\n";
    else if (__cplusplus == 201103L) std::cout << "C++11\n";
    else if (__cplusplus == 199711L) std::cout << "C++98\n";
    else std::cout << "pre-standard C++\n";

    std::cout << "Compile time: "  << __DATE__ << std::endl;
}

// -------------------------------------------------------------


