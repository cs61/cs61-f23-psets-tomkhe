#include <iostream>
#include <cstdlib>
#include <vector>
#include <algorithm>

using namespace std; 

int main() {
    freopen("sol.txt", "w", stdout); 
    vector<int> x = {1, 2, 3, 4, 5, 6}; 
    do
    {
        for (int i : x) cout << i << " "; 
        cout << "\n"; 
    }while (next_permutation(x.begin(), x.end())); 
    exit(0); 
}