#include <cstdio>
#include <cstdlib>

int main() {
    int count = 0; 
    while (fgetc(stdin) != EOF) count++;
    fprintf(stdout, "%d\n", count);
    exit(0); 
}