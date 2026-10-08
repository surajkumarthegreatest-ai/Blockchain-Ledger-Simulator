#include <iostream>
#include "picosha2.h"

using namespace std;

int main() {
    string src = "hello world";
    string hash_hex = picosha2::hash256_hex_string(src);
    cout<< hash_hex <<endl; 
    return 0;
}