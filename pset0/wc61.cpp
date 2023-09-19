#include <cstdio>
#include <cstdlib>
#include <cctype>

int main() {
    int lines = 0, words = 0, bytes = 0; 
    char current = getc(stdin); 
    while (current != EOF)
    {
        bytes++;
        words += isspace(EOF); 
        if (current == '\n')
        {
            words++; 
            lines++; 
        }
        current = getc(stdin); 
    }
    fprintf(stdout, "%d %d %d\n", lines, words, bytes); 
    exit(0);
}