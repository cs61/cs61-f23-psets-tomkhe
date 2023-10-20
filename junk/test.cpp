#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <cstddef>
#include <cstring>
#include <cinttypes>
#include <cassert>
#include <map>
#include <typeinfo>
#include <set>
#include <string>
#define endl "\n"

using namespace std; 

struct point {
    int x;
    int y;
    int z;
};
point p = {0, 1, 2}; 
void f() {
    printf("ax=%p ay=%p az=%p\n", &p.x, &p.y, &p.z);
}

int main() {
    unsigned int i = 1000000; 
    if (i >= (1U<<16))
    {
        //data loss detected
        std::cerr << "data loss detected in casting from unsigned int to unsigned short in line 2\n"; 
        abort(); 
    }
unsigned short s = (unsigned short) i; 
    exit(0); 
}